#pragma once
#include "reportmodel.h"
#include <QMap>
#include <QWidget>
struct ReportSummary {
  qint64 impressions = 0, clicks = 0, conversions = 0;
  double spend = 0;
  QMap<QString, double> dailySpend, channelSpend;
  static ReportSummary from(const QList<MarketingRow> &rows) {
    ReportSummary s;
    for (const auto &r : rows) {
      s.impressions += r.impressions;
      s.clicks += r.clicks;
      s.conversions += r.conversions;
      s.spend += r.spend;
      s.dailySpend[r.date] += r.spend;
      s.channelSpend[r.channel] += r.spend;
    }
    return s;
  }
};
class ReportAnalytics : public QWidget {
public:
  explicit ReportAnalytics(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumHeight(185);
    setMaximumHeight(210);
  }
  void setRows(const QList<MarketingRow> &rows) {
    m_data = ReportSummary::from(rows);
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override;

private:
  ReportSummary m_data;
};
