#include "xlsxexporter.h"
#include <QBuffer>
#include <QSaveFile>
#include <QXmlStreamWriter>
#include <QtEndian>
namespace {
quint32 crc32(const QByteArray &bytes) {
  quint32 c = 0xffffffff;
  for (unsigned char b : bytes) {
    c ^= b;
    for (int k = 0; k < 8; ++k)
      c = (c >> 1) ^ (0xedb88320u & quint32(-int(c & 1)));
  }
  return ~c;
}
void u16(QByteArray &b, quint16 n) {
  b.append(char(n));
  b.append(char(n >> 8));
}
void u32(QByteArray &b, quint32 n) {
  u16(b, quint16(n));
  u16(b, quint16(n >> 16));
}
struct Entry {
  QByteArray name, data;
  quint32 offset = 0, crc = 0;
};
} // namespace
bool XlsxExporter::write(const QString &path, const QList<MarketingRow> &rows,
                         QString *error) {
  if (error)
    error->clear();
  if (rows.size() > 5000) {
    if (error)
      *error = "导出超过单页5000条限制";
    return false;
  }
  QByteArray sheet;
  QXmlStreamWriter xml(&sheet);
  xml.writeStartDocument();
  xml.writeStartElement("worksheet");
  xml.writeDefaultNamespace(
      "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
  xml.writeStartElement("sheetData");
  auto cell = [&](int col, int row, const QString &value, bool number) {
    xml.writeStartElement("c");
    xml.writeAttribute("r", QString(QChar('A' + col)) + QString::number(row));
    if (number)
      xml.writeTextElement("v", value);
    else {
      xml.writeAttribute("t", "inlineStr");
      xml.writeStartElement("is");
      xml.writeStartElement("t");
      xml.writeAttribute("xml:space", "preserve");
      xml.writeCharacters(value);
      xml.writeEndElement();
      xml.writeEndElement();
    }
    xml.writeEndElement();
  };
  xml.writeStartElement("row");
  xml.writeAttribute("r", "1");
  auto headers = ReportModel::headers();
  for (int c = 0; c < headers.size(); ++c)
    cell(c, 1, headers[c], false);
  xml.writeEndElement();
  int row = 2;
  for (const auto &r : rows) {
    xml.writeStartElement("row");
    xml.writeAttribute("r", QString::number(row));
    cell(0, row, r.date, false);
    cell(1, row, r.campaign, false);
    cell(2, row, r.channel, false);
    cell(3, row, QString::number(r.impressions), true);
    cell(4, row, QString::number(r.clicks), true);
    cell(5, row, QString::number(r.spend, 'f', 2), true);
    cell(6, row, QString::number(r.conversions), true);
    cell(7, row,
         r.impressions
             ? QString::number(100.0 * r.clicks / r.impressions, 'f', 2) + "%"
             : "--",
         false);
    xml.writeEndElement();
    ++row;
  }
  xml.writeEndElement();
  xml.writeEndElement();
  xml.writeEndDocument();
  QList<Entry> entries{
      {"[Content_Types].xml",
       R"(<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>)"},
      {"_rels/.rels",
       R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)"},
      {"xl/workbook.xml",
       QStringLiteral(
           "<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook "
           "xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/"
           "main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/"
           "relationships\"><sheets><sheet name=\"营销数据\" sheetId=\"1\" "
           "r:id=\"rId1\"/></sheets></workbook>")
           .toUtf8()},
      {"xl/_rels/workbook.xml.rels",
       R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/></Relationships>)"},
      {"xl/worksheets/sheet1.xml", sheet}};
  QByteArray archive;
  for (auto &e : entries) {
    e.offset = quint32(archive.size());
    e.crc = crc32(e.data);
    u32(archive, 0x04034b50);
    u16(archive, 20);
    u16(archive, 0x800);
    u16(archive, 0);
    u16(archive, 0);
    u16(archive, 33);
    u32(archive, e.crc);
    u32(archive, quint32(e.data.size()));
    u32(archive, quint32(e.data.size()));
    u16(archive, quint16(e.name.size()));
    u16(archive, 0);
    archive += e.name;
    archive += e.data;
  }
  const quint32 central = quint32(archive.size());
  for (const auto &e : entries) {
    u32(archive, 0x02014b50);
    u16(archive, 20);
    u16(archive, 20);
    u16(archive, 0x800);
    u16(archive, 0);
    u16(archive, 0);
    u16(archive, 33);
    u32(archive, e.crc);
    u32(archive, quint32(e.data.size()));
    u32(archive, quint32(e.data.size()));
    u16(archive, quint16(e.name.size()));
    for (int i = 0; i < 4; ++i)
      u16(archive, 0);
    u32(archive, 0);
    u32(archive, e.offset);
    archive += e.name;
  }
  const quint32 size = quint32(archive.size()) - central;
  u32(archive, 0x06054b50);
  u16(archive, 0);
  u16(archive, 0);
  u16(archive, quint16(entries.size()));
  u16(archive, quint16(entries.size()));
  u32(archive, size);
  u32(archive, central);
  u16(archive, 0);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(archive) != archive.size() || !file.commit()) {
    if (error)
      *error = file.errorString();
    return false;
  }
  return true;
}
