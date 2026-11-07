#pragma once
#include "reportmodel.h"
class XlsxExporter {
public:
  // Minimal OOXML workbook: text uses inline strings (never spreadsheet
  // formulas).
  static bool write(const QString &path, const QList<MarketingRow> &rows,
                    QString *error = nullptr);
};
