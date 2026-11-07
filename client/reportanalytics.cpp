#include "reportanalytics.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
void ReportAnalytics::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(Qt::white);
  p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 9, 9);
  const int divide = int(width() * .62);
  p.setPen(QColor("#334966"));
  p.drawText(18, 25, "消耗趋势 / 当前页");
  p.drawText(divide + 18, 25, "渠道消耗 / 当前页");
  if (m_data.dailySpend.isEmpty()) {
    p.setPen(QColor("#94a3b8"));
    p.drawText(rect(), Qt::AlignCenter, "查询或导入报表后查看分析");
    return;
  }
  QRectF chart(65, 48, qMax(60, divide - 90), height() - 82);
  double peak = 1;
  for (auto v : m_data.dailySpend)
    peak = qMax(peak, v);
  for (int i = 0; i <= 3; ++i) {
    double y = chart.bottom() - chart.height() * i / 3;
    p.setPen(QColor("#e8eef7"));
    p.drawLine(QPointF(chart.left(), y), QPointF(chart.right(), y));
    p.setPen(QColor("#8795aa"));
    p.drawText(QRectF(2, y - 9, 55, 18), Qt::AlignRight | Qt::AlignVCenter,
               QString::number(peak * i / 3, 'f', 0));
  }
  QPainterPath path;
  QVector<QPointF> points;
  int index = 0;
  for (auto it = m_data.dailySpend.cbegin(); it != m_data.dailySpend.cend();
       ++it, ++index) {
    const double x = chart.left() + chart.width() * index /
                                        qMax(1, m_data.dailySpend.size() - 1);
    QPointF point(x, chart.bottom() - chart.height() * it.value() / peak);
    points << point;
    if (index == 0)
      path.moveTo(point);
    else
      path.lineTo(point);
    if (index == 0 || index == m_data.dailySpend.size() - 1) {
      p.setPen(QColor("#8795aa"));
      p.drawText(QRectF(x - 27, chart.bottom() + 8, 60, 18), Qt::AlignCenter,
                 it.key().mid(5));
    }
  }
  auto area = path;
  area.lineTo(points.last().x(), chart.bottom());
  area.lineTo(points.first().x(), chart.bottom());
  area.closeSubpath();
  p.fillPath(area, QColor(45, 94, 234, 22));
  p.setPen(QPen(QColor("#2d5eea"), 2.5));
  p.drawPath(path);
  if (points.size() == 1) {
    p.setBrush(QColor("#2d5eea"));
    p.drawEllipse(points.first(), 3, 3);
  }
  QList<QPair<QString, double>> channels;
  double max = 1;
  for (auto it = m_data.channelSpend.cbegin(); it != m_data.channelSpend.cend();
       ++it) {
    channels.append({it.key(), it.value()});
    max = qMax(max, it.value());
  }
  std::stable_sort(
      channels.begin(), channels.end(),
      [](const auto &a, const auto &b) { return a.second > b.second; });
  int y = 52;
  for (int i = 0; i < qMin(4, channels.size()); ++i) {
    const auto &c = channels[i];
    p.setPen(QColor("#62758f"));
    p.drawText(divide + 18, y, c.first.left(12));
    p.drawText(QRectF(width() - 100, y - 13, 80, 18), Qt::AlignRight,
               QString::number(c.second, 'f', 2));
    QRectF bar(divide + 18, y + 7, qMax(20, width() - divide - 38), 8);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#edf2fa"));
    p.drawRoundedRect(bar, 4, 4);
    bar.setWidth(bar.width() * c.second / max);
    p.setBrush(QColor(i == 0 ? "#2d5eea" : "#34a897"));
    p.drawRoundedRect(bar, 4, 4);
    y += 36;
  }
}
