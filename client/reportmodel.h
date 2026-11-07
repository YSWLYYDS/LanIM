#pragma once
#include <QAbstractTableModel>
#include <QByteArray>
#include <QStringList>
#include <QVariant>

struct MarketingRow {
  QString date, campaign, channel;
  qint64 impressions = 0, clicks = 0, conversions = 0;
  double spend = 0;
};
struct ReportData {
  QList<MarketingRow> rows;
  int total = 0;
  QString error;
};
class ReportModel : public QAbstractTableModel {
public:
  using QAbstractTableModel::QAbstractTableModel;
  int rowCount(const QModelIndex &parent = {}) const override {
    return parent.isValid() ? 0 : int(m_rows.size());
  }
  int columnCount(const QModelIndex &parent = {}) const override {
    return parent.isValid() ? 0 : 8;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  void replace(QList<MarketingRow> rows) {
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
  }
  const QList<MarketingRow> &rows() const { return m_rows; }
  static ReportData parse(const QByteArray &json);
  static QStringList headers();
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

private:
  QList<MarketingRow> m_rows;
};
