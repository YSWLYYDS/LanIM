#include "reportmodel.h"
#include <QColor>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
void ReportModel::sort(int column, Qt::SortOrder order) {
  if (column < 0 || column >= 8)
    return;
  beginResetModel();
  std::stable_sort(
      m_rows.begin(), m_rows.end(),
      [column, order](const MarketingRow &a, const MarketingRow &b) {
        int result = 0;
        if (column < 3) {
          const QString x = column == 0   ? a.date
                            : column == 1 ? a.campaign
                                          : a.channel;
          const QString y = column == 0   ? b.date
                            : column == 1 ? b.campaign
                                          : b.channel;
          result = QString::localeAwareCompare(x, y);
        } else {
          auto value = [column](const MarketingRow &r) -> double {
            switch (column) {
            case 3:
              return double(r.impressions);
            case 4:
              return double(r.clicks);
            case 5:
              return r.spend;
            case 6:
              return double(r.conversions);
            default:
              return r.impressions ? double(r.clicks) / r.impressions : -1;
            }
          };
          const double x = value(a), y = value(b);
          result = x < y ? -1 : x > y ? 1 : 0;
        }
        return order == Qt::AscendingOrder ? result < 0 : result > 0;
      });
  endResetModel();
}
QStringList ReportModel::headers() {
  return {"日期",   "投放项目",       "渠道",   "曝光量",
          "点击量", "消耗金额（元）", "转化数", "点击率"};
}
QVariant ReportModel::headerData(int s, Qt::Orientation o, int role) const {
  if (role != Qt::DisplayRole)
    return {};
  return o == Qt::Horizontal ? QVariant(headers().value(s)) : QVariant(s + 1);
}
QVariant ReportModel::data(const QModelIndex &i, int role) const {
  if (!i.isValid() || i.row() < 0 || i.row() >= m_rows.size() ||
      i.column() < 0 || i.column() >= 8)
    return {};
  const auto &r = m_rows[i.row()];
  if (role == Qt::TextAlignmentRole)
    return int(i.column() < 3 ? Qt::AlignLeft | Qt::AlignVCenter
                              : Qt::AlignRight | Qt::AlignVCenter);
  if (role == Qt::BackgroundRole && i.row() % 2)
    return QColor("#f0f4fa");
  if (role == Qt::ToolTipRole && i.column() == 7)
    return QStringLiteral("点击量 / 曝光量；零曝光显示 --");
  if (role != Qt::DisplayRole)
    return {};
  switch (i.column()) {
  case 0:
    return r.date;
  case 1:
    return r.campaign;
  case 2:
    return r.channel;
  case 3:
    return r.impressions;
  case 4:
    return r.clicks;
  case 5:
    return QString::number(r.spend, 'f', 2);
  case 6:
    return r.conversions;
  case 7:
    return r.impressions
               ? QString::number(100.0 * r.clicks / r.impressions, 'f', 2) + "%"
               : "--";
  }
  return {};
}
ReportData ReportModel::parse(const QByteArray &json) {
  ReportData out;
  QJsonParseError err;
  auto d = QJsonDocument::fromJson(json, &err);
  auto fail = [&](const QString &s) {
    out.rows.clear();
    out.error = s;
    return out;
  };
  if (err.error != QJsonParseError::NoError || !d.isObject())
    return fail("响应不是合法JSON对象");
  auto o = d.object();
  if (!o.value("items").isArray() || !o.value("total").isDouble() ||
      o.value("total").toDouble() < 0 ||
      o.value("total").toDouble() > 10000000 ||
      std::floor(o.value("total").toDouble()) != o.value("total").toDouble())
    return fail("缺少合法 items/total 字段");
  auto items = o.value("items").toArray();
  if (items.size() > 5000)
    return fail("单页数据超过5000条保护上限");
  out.total = o.value("total").toInt();
  if (out.total < items.size())
    return fail("总条数小于返回条数");
  for (const auto &v : items) {
    if (!v.isObject())
      return fail("数据行不是对象");
    auto x = v.toObject();
    MarketingRow r;
    r.date = x.value("date").toString();
    r.campaign = x.value("campaign").toString();
    r.channel = x.value("channel").toString();
    if (!QDate::fromString(r.date, Qt::ISODate).isValid() ||
        r.campaign.trimmed().isEmpty() || r.channel.trimmed().isEmpty() ||
        r.campaign.size() > 100 || r.channel.size() > 100)
      return fail("日期/投放项目/渠道不合法");
    for (const auto &key :
         QStringList{"impressions", "clicks", "conversions", "spend"}) {
      const auto value = x.value(key);
      const double n = value.toDouble(-1);
      if (!value.isDouble() || !std::isfinite(n) || n < 0 || n > 1e12 ||
          (key != "spend" && std::floor(n) != n))
        return fail("指标必须是非负数字，计数必须为整数");
    }
    r.impressions = qint64(x.value("impressions").toDouble());
    r.clicks = qint64(x.value("clicks").toDouble());
    r.conversions = qint64(x.value("conversions").toDouble());
    r.spend = x.value("spend").toDouble();
    out.rows.append(r);
  }
  return out;
}
