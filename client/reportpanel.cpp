#include "reportpanel.h"
#include "businessstyle.h"
#include "reportanalytics.h"
#include "xlsxexporter.h"
#include <QFutureWatcher>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QtConcurrent>
#include <QtWidgets>
#include <memory>
namespace {
ReportData builtInDemo(const QDate &from, const QDate &to,
                       const QString &channelFilter,
                       const QString &campaignFilter, int offset) {
  ReportData result;
  const QStringList channels{"搜索广告", "信息流", "内容渠道"};
  const QStringList campaigns{"新品推广 DEMO", "品牌营销 DEMO", "活动引流 DEMO"};
  const QDate first = qMax(from, QDate::currentDate().addDays(-59));
  const QDate last = qMin(to, QDate::currentDate());
  for (QDate day = first; day.isValid() && day <= last; day = day.addDays(1)) {
    const int age = day.daysTo(QDate::currentDate());
    for (int i = 0; i < channels.size(); ++i) {
      if (!channelFilter.isEmpty() &&
          !channels[i].contains(channelFilter, Qt::CaseInsensitive)) continue;
      for (int j = 0; j < campaigns.size(); ++j) {
        if (!campaignFilter.isEmpty() &&
            !campaigns[j].contains(campaignFilter, Qt::CaseInsensitive)) continue;
        MarketingRow row;
        row.date = day.toString(Qt::ISODate); row.channel = channels[i];
        row.campaign = campaigns[j];
        row.impressions = 10000 + (59 - age) * 170 + i * 520 + j * 130;
        row.clicks = 300 + (59 - age) * 7 + i * 10 + j * 5;
        row.conversions = 20 + (59 - age) / 2 + j;
        row.spend = 180 + (59 - age) * 2.3 + i * 10 + j * 6.5;
        result.rows.append(row);
      }
    }
  }
  result.total = result.rows.size();
  result.rows = result.rows.mid(qMax(0, offset), 200);
  return result;
}
bool isDefaultDemoEndpoint(const QUrl &url, const QString &token) {
  return token.isEmpty() && url.scheme() == "http" &&
         (url.host() == "127.0.0.1" || url.host() == "localhost") &&
         url.port(45455) == 45455 && url.path() == "/api/reports";
}
} // namespace
ReportPanel::~ReportPanel() {
  ++m_generation;
  if (m_reply) {
    m_reply->disconnect(this);
    m_reply->abort();
  }
}
ReportPanel::ReportPanel(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);
  setAcceptDrops(true);
  auto *stats = new QHBoxLayout;
  for (const auto &caption : QStringList{"曝光量 / 当前页", "点击量 / 当前页",
                                         "消耗金额 / 元", "转化量 / 当前页"}) {
    QLabel *value;
    stats->addWidget(BusinessUi::metric(caption, value));
    m_metrics << value;
  }
  layout->addLayout(stats);
  m_analytics = new ReportAnalytics;
  layout->addWidget(m_analytics);
  auto *tools = new QHBoxLayout;
  auto *sourceToggle = new QPushButton("数据源设置");
  sourceToggle->setCheckable(true);
  auto *import = new QPushButton("导入离线 JSON");
  auto *history = new QPushButton("请求记录");
  auto *refreshRate = new QComboBox;
  refreshRate->setObjectName("reportRefreshRate");
  refreshRate->addItem("手动刷新", 0);
  refreshRate->addItem("每30秒刷新", 30000);
  refreshRate->addItem("每60秒刷新", 60000);
  tools->addWidget(sourceToggle);
  tools->addWidget(import);
  tools->addWidget(history);
  tools->addStretch();
  tools->addWidget(new QLabel("自动刷新"));
  tools->addWidget(refreshRate);
  layout->addLayout(tools);
  auto *sourcePanel = new QWidget;
  auto *server = new QFormLayout;
  sourcePanel->setLayout(server);
  sourcePanel->hide();
  connect(sourceToggle, &QPushButton::toggled, sourcePanel,
          &QWidget::setVisible);
  m_url = new QLineEdit(
      QSettings()
          .value("report/url", "http://127.0.0.1:45455/api/reports")
          .toString());
  m_url->setObjectName("reportUrl");
  m_token = new QLineEdit;
  m_token->setEchoMode(QLineEdit::Password);
  m_token->setPlaceholderText("真实接口按约定填写；不保存到磁盘");
  server->addRow("HTTP报表接口", m_url);
  server->addRow("Bearer Token", m_token);
  layout->addWidget(sourcePanel);
  auto *filters = new QHBoxLayout;
  m_from = new QDateEdit(QDate::currentDate().addDays(-30));
  m_to = new QDateEdit(QDate::currentDate());
  m_from->setObjectName("reportFrom");
  m_to->setObjectName("reportTo");
  for (auto *d : {m_from, m_to}) {
    d->setCalendarPopup(true);
    d->setDisplayFormat("yyyy-MM-dd");
    // Do not let the global report-table stylesheet style the calendar's
    // internal QTableView.  A fixed, readable calendar also prevents the
    // popup from being compressed on high-DPI Windows desktops.
    auto *calendar = new QCalendarWidget;
    calendar->setGridVisible(true);
    calendar->setFirstDayOfWeek(Qt::Monday);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setMinimumSize(380, 270);
    d->setCalendarWidget(calendar);
  }
  m_channel = new QLineEdit;
  m_channel->setPlaceholderText("渠道关键词");
  m_campaign = new QLineEdit;
  m_campaign->setPlaceholderText("投放项目关键词");
  auto *query = new QPushButton("查询 / 重试");
  query->setObjectName("reportQuery");
  query->setProperty("primary", true);
  auto *cancel = new QPushButton("取消请求");
  cancel->setObjectName("reportCancel");
  filters->addWidget(m_from);
  filters->addWidget(new QLabel("至"));
  filters->addWidget(m_to);
  filters->addWidget(m_channel);
  filters->addWidget(m_campaign);
  filters->addWidget(query);
  filters->addWidget(cancel);
  layout->addLayout(filters);
  auto *shortcuts = new QHBoxLayout;
  shortcuts->addWidget(new QLabel("快捷日期"));
  for (int days : {7, 30}) {
    auto *button = new QPushButton(QString("近%1天").arg(days));
    shortcuts->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, days] {
      m_from->setDate(QDate::currentDate().addDays(1 - days));
      m_to->setDate(QDate::currentDate());
      reload();
    });
  }
  auto *reset = new QPushButton("重置筛选并查询");
  shortcuts->addWidget(reset);
  shortcuts->addStretch();
  auto *issue = new QPushButton("将选中数据提交为问题");
  issue->setObjectName("reportCreateIssue");
  issue->setEnabled(false);
  shortcuts->addWidget(issue);
  layout->addLayout(shortcuts);
  m_model = new ReportModel(this);
  m_view = new QTableView;
  m_view->setObjectName("marketingReportTable");
  m_view->setModel(m_model);
  m_view->setSortingEnabled(true);
  m_view->setToolTip("点击列标题排序，仅作用于当前已加载页");
  m_view->verticalHeader()->setDefaultSectionSize(36);
  m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_view->setSelectionMode(QAbstractItemView::SingleSelection);
  m_view->setAlternatingRowColors(true);
  m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layout->addWidget(m_view, 1);
  auto *footer = new QHBoxLayout;
  m_previous = new QPushButton("上一页");
  m_next = new QPushButton("下一页");
  m_export = new QPushButton("导出当前页 Excel (.xlsx)");
  m_export->setEnabled(false);
  m_info = new QLabel(
      "选择筛选条件后查询，或拖入离线 JSON 报表。默认连接本地演示数据源。");
  m_info->setWordWrap(true);
  m_info->setObjectName("reportStatus");
  footer->addWidget(m_previous);
  footer->addWidget(m_next);
  footer->addWidget(m_export);
  layout->addLayout(footer);
  layout->addWidget(m_info);
  connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [this, issue] {
    issue->setEnabled(!m_busy && m_view->currentIndex().isValid());
  });
  connect(issue, &QPushButton::clicked, this, [this] {
    const int row = m_view->currentIndex().row();
    if (row < 0 || row >= m_model->rows().size()) return;
    const auto &r = m_model->rows()[row];
    emit issueRequested(r.campaign + "：投放数据核查", r.channel,
        QString("数据来源：%1\n日期：%2\n项目：%3\n渠道：%4\n曝光：%5；点击：%6；转化：%7；消耗：%8元\n\n异常现象：\n预期结果：")
            .arg(m_displayContext, r.date, r.campaign, r.channel)
            .arg(r.impressions).arg(r.clicks).arg(r.conversions)
            .arg(r.spend, 0, 'f', 2));
  });
  connect(reset, &QPushButton::clicked, this, [this] {
    m_from->setDate(QDate::currentDate().addDays(-30));
    m_to->setDate(QDate::currentDate());
    m_channel->clear(); m_campaign->clear(); reload();
  });
  auto filtersChanged = [this] {
    // Invalidate pending work: a response must not appear under new filters.
    ++m_generation;
    if (m_reply) m_reply->abort();
    m_offset = m_displayOffset;
    finishState();
    m_previous->setEnabled(false); m_next->setEnabled(false);
    m_info->setText("筛选条件已修改，请查询；当前表格仍为上次结果");
  };
  connect(m_from, &QDateEdit::dateChanged, this, filtersChanged);
  connect(m_to, &QDateEdit::dateChanged, this, filtersChanged);
  connect(m_channel, &QLineEdit::textEdited, this, filtersChanged);
  connect(m_campaign, &QLineEdit::textEdited, this, filtersChanged);
  connect(m_channel, &QLineEdit::returnPressed, query, &QPushButton::click);
  connect(m_campaign, &QLineEdit::returnPressed, query, &QPushButton::click);
  connect(query, &QPushButton::clicked, this, [this] { reload(); });
  connect(import, &QPushButton::clicked, this, [this] { importJsonFile(); });
  connect(history, &QPushButton::clicked, this, [this] {
    QDialog d(this);
    d.setWindowTitle("请求与文件处理记录");
    d.resize(760, 440);
    auto *box = new QVBoxLayout(&d);
    auto *text = new QPlainTextEdit;
    text->setReadOnly(true);
    text->setPlainText(m_requestLog.isEmpty() ? "尚无记录"
                                              : m_requestLog.join('\n'));
    box->addWidget(text);
    d.exec();
  });
  connect(refreshRate, &QComboBox::currentIndexChanged, this,
          [this, refreshRate] {
            const auto ms = refreshRate->currentData().toInt();
            if (ms > 0)
              m_autoRefresh.start(ms);
            else
              m_autoRefresh.stop();
          });
  connect(&m_autoRefresh, &QTimer::timeout, this, [this] {
    if (!m_busy && !m_offline)
      reload(false);
  });
  connect(cancel, &QPushButton::clicked, this, [this] {
    ++m_generation;
    if (m_reply)
      m_reply->abort();
    m_offset = m_displayOffset;
    finishState();
    m_info->setText("已取消，保留已显示结果");
  });
  connect(m_previous, &QPushButton::clicked, this, [this] {
    m_offset = qMax(0, m_offset - 200);
    reload(false);
  });
  connect(m_next, &QPushButton::clicked, this, [this] {
    m_offset += 200;
    reload(false);
  });
  connect(m_export, &QPushButton::clicked, this, &ReportPanel::exportReport);
  m_previous->setEnabled(false);
  m_next->setEnabled(false);
  if (QCoreApplication::arguments().contains("--smoke-test") &&
      qEnvironmentVariableIsSet("BUSINESS_REPORT_URL")) {
    m_url->setText(qEnvironmentVariable("BUSINESS_REPORT_URL"));
    QTimer::singleShot(100, this, [this] { reload(); });
  }
}
void ReportPanel::reload(bool reset) {
  const QUrl url(m_url->text().trimmed());
  if (!url.isValid() || url.host().isEmpty() ||
      (url.scheme() != "http" && url.scheme() != "https") ||
      !url.userInfo().isEmpty()) {
    m_info->setText("请输入有效的HTTP/HTTPS接口地址，不要将凭据放入URL");
    return;
  }
  if (m_from->date() > m_to->date()) {
    m_info->setText("开始日期不能晚于结束日期");
    return;
  }
  if (reset)
    m_offset = 0;
  ++m_generation;
  if (m_reply)
    m_reply->abort();
  m_requestUrl = url;
  m_busy = true;
  findChild<QPushButton *>("reportCreateIssue")->setEnabled(false);
  m_requestToken = m_token->text();
  QUrlQuery query(url);
  query.removeAllQueryItems("from");
  query.removeAllQueryItems("to");
  query.removeAllQueryItems("channel");
  query.removeAllQueryItems("campaign");
  query.removeAllQueryItems("offset");
  query.removeAllQueryItems("limit");
  query.addQueryItem("from", m_from->date().toString(Qt::ISODate));
  query.addQueryItem("to", m_to->date().toString(Qt::ISODate));
  query.addQueryItem("channel", m_channel->text().trimmed());
  query.addQueryItem("campaign", m_campaign->text().trimmed());
  query.addQueryItem("offset", QString::number(m_offset));
  query.addQueryItem("limit", "200");
  m_requestUrl.setQuery(query);
  if (!QCoreApplication::arguments().contains("--smoke-test"))
    QSettings().setValue("report/url", url.toString());
  send(m_generation, 0);
}
void ReportPanel::send(quint64 generation, int attempt) {
  if (generation != m_generation)
    return;
  QNetworkRequest request(m_requestUrl);
  request.setRawHeader("Accept", "application/json");
  if (!m_requestToken.isEmpty())
    request.setRawHeader("Authorization",
                         ("Bearer " + m_requestToken).toUtf8());
  request.setTransferTimeout(10000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  auto *reply = m_network.get(request);
  const auto started = QDateTime::currentMSecsSinceEpoch();
  m_reply = reply;
  m_export->setEnabled(false);
  auto bytes = std::make_shared<QByteArray>();
  auto oversize = std::make_shared<bool>(false);
  m_info->setText(QString("请求中（第%1次，超时10秒）…").arg(attempt + 1));
  m_previous->setEnabled(false);
  m_next->setEnabled(false);
  connect(reply, &QNetworkReply::readyRead, this, [reply, bytes, oversize] {
    bytes->append(reply->readAll());
    if (bytes->size() > 5 * 1024 * 1024) {
      *oversize = true;
      reply->abort();
    }
  });
  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, bytes, oversize, generation, attempt, started] {
        reply->deleteLater();
        if (generation != m_generation)
          return;
        bytes->append(reply->readAll());
        const int code =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_requestLog.prepend(
            QString("%1  GET  HTTP %2  %3ms  尝试%4  %5 bytes")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                .arg(code)
                .arg(QDateTime::currentMSecsSinceEpoch() - started)
                .arg(attempt + 1)
                .arg(bytes->size()));
        if (m_requestLog.size() > 100)
          m_requestLog.removeLast();
        if (*oversize || bytes->size() > 5 * 1024 * 1024) {
          m_info->setText("响应超过5MiB保护上限，已拒绝");
          m_offset = m_displayOffset;
          finishState();
          return;
        }
        if (reply->error() != QNetworkReply::NoError || code != 200) {
          const bool retry = code >= 500 || code == 0;
          const bool useBuiltInDemo =
              isDefaultDemoEndpoint(m_requestUrl, m_requestToken) && code == 0;
          if (useBuiltInDemo) {
            acceptData(QtConcurrent::run([from = m_from->date(),
                                          to = m_to->date(),
                                          channel = m_channel->text().trimmed(),
                                          campaign = m_campaign->text().trimmed(),
                                          offset = m_offset] {
                         return builtInDemo(from, to, channel, campaign, offset);
                       }),
                       generation, "内置演示数据（本地服务未启动）", false);
          } else if (retry && attempt < 2) {
            m_info->setText(QString("连接/服务异常，%1秒后自动重试（最多2次）")
                                .arg(3 * (attempt + 1)));
            QTimer::singleShot(
                3000 * (attempt + 1), this,
                [this, generation, attempt] { send(generation, attempt + 1); });
          } else {
            m_info->setText(QString("请求失败：HTTP %1；%2。可点击查询重试。")
                                .arg(code)
                                .arg(reply->errorString()));
            m_offset = m_displayOffset;
            finishState();
          }
          return;
        }
        acceptData(QtConcurrent::run([payload = *bytes] {
                     return ReportModel::parse(payload);
                   }),
                   generation, "HTTP 查询", false);
      });
}
void ReportPanel::exportReport() {
  QString path = QFileDialog::getSaveFileName(
      this, "导出营销报表（当前已加载页）",
      "投放报表-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".xlsx", "Excel (*.xlsx)");
  if (path.isEmpty())
    return;
  if (!path.endsWith(".xlsx", Qt::CaseInsensitive))
    path += ".xlsx";
  m_export->setEnabled(false);
  m_exportBusy = true;
  auto snapshot = m_model->rows();
  auto *watcher = new QFutureWatcher<QString>(this);
  connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
    auto error = watcher->result();
    watcher->deleteLater();
    m_exportBusy = false;
    m_export->setEnabled(!m_busy && m_model->rowCount() > 0);
    m_info->setText(error.isEmpty() ? "Excel导出成功（当前已加载页）"
                                    : "导出失败：" + error);
  });
  watcher->setFuture(QtConcurrent::run([path, snapshot] {
    QString error;
    XlsxExporter::write(path, snapshot, &error);
    return error;
  }));
}
void ReportPanel::finishState() {
  m_busy = false;
  m_previous->setEnabled(!m_offline && m_displayOffset > 0);
  m_next->setEnabled(!m_offline && m_displayOffset + 200 < m_total);
  m_export->setEnabled(!m_exportBusy && m_model->rowCount() > 0);
  findChild<QPushButton *>("reportCreateIssue")->setEnabled(
      m_view->currentIndex().isValid() && m_model->rowCount() > 0);
}
void ReportPanel::acceptData(QFuture<ReportData> future, quint64 generation,
                             const QString &source, bool offline) {
  auto *watcher = new QFutureWatcher<ReportData>(this);
  connect(
      watcher, &QFutureWatcher<ReportData>::finished, this,
      [this, watcher, generation, source, offline,
       context = offline ? QString("离线报表") : m_requestUrl.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery)] {
        auto data = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation)
          return;
        if (offline && data.error.isEmpty() && data.total != data.rows.size())
          data.error =
              "离线文件的 total 必须等于 items 条数，请导入完整单文件报表";
        if (!data.error.isEmpty()) {
          m_info->setText("处理失败：" + data.error + "；保留原结果");
          m_requestLog.prepend(source + "：" + data.error);
          while(m_requestLog.size()>100)m_requestLog.removeLast();
          m_offset = m_displayOffset;
          finishState();
          return;
        }
        m_model->replace(data.rows);
        m_displayContext = context;
        m_model->sort(m_view->horizontalHeader()->sortIndicatorSection(),
                      m_view->horizontalHeader()->sortIndicatorOrder());
        m_offline = offline;
        m_total = data.total;
        m_displayOffset = offline ? 0 : m_offset;
        m_offset = m_displayOffset;
        m_analytics->setRows(data.rows);
        const auto summary = ReportSummary::from(data.rows);
        m_metrics[0]->setText(QString::number(summary.impressions));
        m_metrics[1]->setText(QString::number(summary.clicks));
        m_metrics[2]->setText(QString::number(summary.spend, 'f', 2));
        m_metrics[3]->setText(QString::number(summary.conversions));
        finishState();
        m_info->setText(
            QString(
                "%1  ·  共%2条 / 已载入%3条  ·  当前页点击率 %4  ·  更新于 %5")
                .arg(source)
                .arg(m_total)
                .arg(data.rows.size())
                .arg(summary.impressions
                         ? QString::number(100.0 * summary.clicks /
                                               summary.impressions,
                                           'f', 2) +
                               "%"
                         : "--")
                .arg(QTime::currentTime().toString("HH:mm:ss")));
        if (offline) {
          m_autoRefresh.stop();
          findChild<QComboBox*>("reportRefreshRate")->setCurrentIndex(0);
          m_requestLog.prepend(QString("%1  离线文件载入 %2 条")
                                   .arg(QTime::currentTime().toString())
                                   .arg(data.rows.size()));
        }
        while (m_requestLog.size() > 100)
          m_requestLog.removeLast();
      });
  watcher->setFuture(future);
}
void ReportPanel::importJsonFile(QString path) {
  if (path.isEmpty())
    path = QFileDialog::getOpenFileName(this, "导入离线投放报表", {},
                                        "JSON 报表 (*.json)");
  if (path.isEmpty())
    return;
  ++m_generation;
  if (m_reply)
    m_reply->abort();
  m_busy = true;
  m_export->setEnabled(false);
  m_previous->setEnabled(false);
  m_next->setEnabled(false);
  m_info->setText("正在后台读取、校验并解析文件…");
  acceptData(QtConcurrent::run([path] {
               QFile file(path);
               ReportData error;
               if (!file.open(QIODevice::ReadOnly)) {
                 error.error = "无法读取文件：" + file.errorString();
                 return error;
               }
               if (file.size() > 5 * 1024 * 1024) {
                 error.error = "文件超过5MiB限制";
                 return error;
               }
               const auto payload = file.read(5 * 1024 * 1024 + 1);
               if (payload.size() > 5 * 1024 * 1024) {
                 error.error = "文件超过5MiB限制";
                 return error;
               }
               return ReportModel::parse(payload);
             }),
             m_generation, "离线 JSON", true);
}
void ReportPanel::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls())
    for (const auto &url : event->mimeData()->urls())
      if (url.isLocalFile() &&
          QFileInfo(url.toLocalFile())
                  .suffix()
                  .compare("json", Qt::CaseInsensitive) == 0) {
        event->acceptProposedAction();
        return;
      }
}
void ReportPanel::dropEvent(QDropEvent *event) {
  for (const auto &url : event->mimeData()->urls())
    if (url.isLocalFile() &&
        QFileInfo(url.toLocalFile())
                .suffix()
                .compare("json", Qt::CaseInsensitive) == 0) {
      event->acceptProposedAction();
      importJsonFile(url.toLocalFile());
      return;
    }
}
