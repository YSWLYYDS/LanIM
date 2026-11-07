#include "ticketpanel.h"
#include "businessstyle.h"
#include <QJsonArray>
#include <QUuid>
#include <QtWidgets>
namespace {
QString statusText(const QString &s) {
  return QMap<QString, QString>{{"pending", "待处理"},
                                {"processing", "处理中"},
                                {"resolved", "已解决"},
                                {"closed", "已关闭"}}
      .value(s, s);
}
QString priorityText(const QString &s) {
  return QMap<QString, QString>{
      {"low", "低"}, {"normal", "普通"}, {"high", "高"}}
      .value(s, s);
}
} // namespace
TicketPanel::TicketPanel(ChatClient *client, QWidget *parent)
    : QWidget(parent), m_client(client) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);
  auto *metrics = new QHBoxLayout;
  for (const auto &caption :
       QStringList{"待处理 / 当前页", "处理中 / 当前页", "已解决 / 当前页",
                   "已关闭 / 当前页"}) {
    QLabel *value;
    metrics->addWidget(BusinessUi::metric(caption, value));
    m_metrics << value;
  }
  layout->addLayout(metrics);
  auto *bar = new QHBoxLayout;
  m_role = new QLabel("请先登录");
  bar->addWidget(m_role);
  auto *create = new QPushButton("新建业务问题");
  create->setProperty("primary", true);
  create->setObjectName("createTicket");
  bar->addWidget(create);
  m_status = new QComboBox;
  m_status->addItem("全部状态", "");
  for (const auto &s :
       QStringList{"pending", "processing", "resolved", "closed"})
    m_status->addItem(statusText(s), s);
  m_key = new QLineEdit;
  m_key->setPlaceholderText("按标题 / 软件设备搜索");
  m_key->setMaxLength(100);
  auto *search = new QPushButton("查询 / 刷新");
  bar->addWidget(m_status);
  bar->addWidget(m_key, 1);
  bar->addWidget(search);
  layout->addLayout(bar);
  auto *split = new QSplitter(Qt::Vertical);
  m_table = new QTableWidget(0, 7);
  m_table->setObjectName("ticketTable");
  m_table->setHorizontalHeaderLabels({"标题", "软件 / 设备", "优先级", "状态",
                                      "提交人", "处理人", "创建时间"});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setAlternatingRowColors(true);
  m_table->verticalHeader()->setDefaultSectionSize(38);
  m_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  split->addWidget(m_table);
  m_detail = new QTextEdit;
  m_detail->setReadOnly(true);
  m_detail->setObjectName("ticketDetail");
  m_detail->setPlaceholderText("选择一条问题查看详情和处理记录；也可以从报表页直接提交数据问题。");
  split->addWidget(m_detail);
  layout->addWidget(split, 1);
  auto *actions = new QHBoxLayout;
  auto button = [&](const QString &label) {
    auto *b = new QPushButton(label);
    actions->addWidget(b);
    return b;
  };
  m_claim = button("认领");
  m_resolve = button("提交解决说明");
  m_close = button("确认关闭");
  m_reopen = button("退回重处理");
  m_older = button("加载更早沟通 / 操作记录");
  auto *copy = button("复制工单摘要");
  connect(copy, &QPushButton::clicked, this, [this] {
    if (!m_ticket.isEmpty())
      QApplication::clipboard()->setText(
          QString("%1\n状态：%2\n描述：%3\n解决说明：%4")
              .arg(m_ticket.value("title").toString(),
                   statusText(m_ticket.value("status").toString()),
                   m_ticket.value("description").toString(),
                   m_ticket.value("resolution").toString()));
  });
  layout->addLayout(actions);
  auto *msgRow = new QHBoxLayout;
  m_message = new QLineEdit;
  m_message->setMaxLength(2000);
  m_message->setPlaceholderText("工单留言：对方离线也可保存，重新登录后查询");
  auto *send = new QPushButton("发送留言");
  msgRow->addWidget(m_message, 1);
  msgRow->addWidget(send);
  layout->addLayout(msgRow);
  auto *pages = new QHBoxLayout;
  m_previous = new QPushButton("上一页");
  m_next = new QPushButton("下一页");
  m_info = new QLabel;
  pages->addWidget(m_previous);
  pages->addWidget(m_next);
  pages->addWidget(m_info, 1);
  layout->addLayout(pages);
  connect(create, &QPushButton::clicked, this, &TicketPanel::createTicket);
  connect(search, &QPushButton::clicked, this, [this] {
    m_offset = 0;
    refresh();
  });
  connect(m_key, &QLineEdit::returnPressed, search, &QPushButton::click);
  connect(m_status, &QComboBox::currentIndexChanged, this, [this] {
    m_offset = 0;
    refresh();
  });
  connect(m_previous, &QPushButton::clicked, this, [this] {
    m_offset = qMax(0, m_offset - 50);
    refresh();
  });
  connect(m_next, &QPushButton::clicked, this, [this] {
    m_offset += 50;
    refresh();
  });
  connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
    auto *item = m_table->item(m_table->currentRow(), 0);
    if (!item)
      return;
    m_selectedId = item->data(Qt::UserRole).toString();
    m_ticket = {};
    m_detail->clear();
    updateActions();
    detail();
  });
  connect(m_claim, &QPushButton::clicked, this, [this] { mutate("claim"); });
  connect(m_resolve, &QPushButton::clicked, this,
          [this] { mutate("resolve"); });
  connect(m_close, &QPushButton::clicked, this, [this] { mutate("close"); });
  connect(m_reopen, &QPushButton::clicked, this, [this] { mutate("reopen"); });
  connect(m_older, &QPushButton::clicked, this,
          [this] { detail(m_historyOffset + 50); });
  auto sendMessage = [this] {
    if (m_selectedId.isEmpty() || m_message->text().trimmed().isEmpty())
      return;
    request("message",
            {{"id", m_selectedId}, {"text", m_message->text().trimmed()}});
  };
  connect(send, &QPushButton::clicked, this, sendMessage);
  connect(m_message, &QLineEdit::returnPressed, this, sendMessage);
  connect(client, &ChatClient::messageReceived, this, &TicketPanel::received);
  connect(client, &ChatClient::disconnected, this, [this] { setSession({}); });
  setSession({});
}
void TicketPanel::setSession(const QString &user) {
  m_user = user;
  m_requests.clear();
  m_selectedId.clear();
  m_ticket = {};
  m_offset = 0;
  m_staff = false;
  m_table->setRowCount(0);
  m_detail->clear();
  m_message->clear();
  m_info->clear();
  for (auto *metric : m_metrics)
    metric->setText("0");
  setEnabled(!user.isEmpty());
  m_role->setText(user.isEmpty() ? "请先登录" : user);
  updateActions();
  if (!user.isEmpty())
    refresh();
}
void TicketPanel::request(const QString &action, QJsonObject body) {
  if (m_user.isEmpty())
    return;
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_requests[id] = {{"action", action}, {"body", body}};
  if (!m_client->sendMessage({{"version", 1},
                              {"type", "ticket." + action},
                              {"requestId", id},
                              {"body", body}})) {
    m_requests.remove(id);
    m_info->setText("发送失败，请确认连接并重新登录");
  }
}
void TicketPanel::refresh() {
  request("list", {{"status", m_status->currentData().toString()},
                   {"keyword", m_key->text().trimmed()},
                   {"offset", m_offset}});
}
void TicketPanel::detail(int offset) {
  if (!m_selectedId.isEmpty())
    request("detail", {{"id", m_selectedId}, {"offset", offset}});
}
void TicketPanel::createTicket() {
  QDialog dialog(this);
  dialog.setWindowTitle("提交业务问题");
  auto *form = new QFormLayout(&dialog);
  QLineEdit title, device;
  title.setMaxLength(100);
  device.setMaxLength(100);
  QTextEdit description;
  title.setText(m_draftTitle);
  device.setText(m_draftContext);
  description.setPlainText(m_draftDescription);
  description.setPlaceholderText("请填写异常现象、复现步骤和预期结果");
  QComboBox priority;
  priority.addItem("普通", "normal");
  priority.addItem("高", "high");
  priority.addItem("低", "low");
  form->addRow("标题", &title);
  form->addRow("渠道 / 业务模块", &device);
  form->addRow("问题描述（最多2000字）", &description);
  form->addRow("优先级", &priority);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  form->addRow(&buttons);
  auto *validation = new QLabel;
  validation->setStyleSheet("color:#be463b;");
  form->addRow(validation);
  buttons.button(QDialogButtonBox::Ok)->setText("提交问题");
  buttons.button(QDialogButtonBox::Cancel)->setText("取消");
  connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
    const auto text = description.toPlainText().trimmed();
    if (title.text().trimmed().isEmpty() || text.isEmpty() || text.size() > 2000) {
      validation->setText("请填写标题及1至2000字的问题描述");
      return;
    }
    dialog.accept();
  });
  connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  dialog.resize(500, 340);
  if (dialog.exec() != QDialog::Accepted)
    return;
  request("create",
          {{"title", title.text().trimmed()},
           {"device", device.text().trimmed()},
           {"description", description.toPlainText().trimmed()},
           {"priority", priority.currentData().toString()},
           {"clientKey", QUuid::createUuid().toString(QUuid::WithoutBraces)}});
}
void TicketPanel::createFromReport(const QString &title, const QString &context,
                                 const QString &description) {
  if (m_user.isEmpty()) return;
  m_draftTitle = title.left(100);
  m_draftContext = context.left(100);
  m_draftDescription = description;
  createTicket();
  m_draftTitle.clear(); m_draftContext.clear(); m_draftDescription.clear();
}
void TicketPanel::mutate(const QString &action) {
  if (m_ticket.isEmpty())
    return;
  QString note;
  if (action == "resolve" || action == "reopen") {
    bool accepted = false;
    note = QInputDialog::getMultiLineText(
        this, "处理说明", "填写解决方案或退回原因：", {}, &accepted);
    if (!accepted)
      return;
    if (note.trimmed().isEmpty() || note.trimmed().size() > 2000) {
      m_info->setText("处理说明不能为空且不能超过2000字");
      return;
    }
  }
  if (action == "close" && QMessageBox::question(this, "确认问题已解决",
      "确认关闭此问题？关闭后将停止接收留言。") != QMessageBox::Yes) return;
  request(action, {{"id", m_selectedId},
                   {"version", m_ticket.value("version")},
                   {"note", note.trimmed()}});
}
void TicketPanel::received(const QJsonObject &message) {
  if (m_user.isEmpty())
    return;
  const QString type = message.value("type").toString();
  if (type == "ticket.changed") {
    refresh();
    if (message.value("body").toObject().value("id") == m_selectedId)
      detail();
    return;
  }
  if (type != "ticket.result")
    return;
  const QString id = message.value("requestId").toString();
  if (!m_requests.contains(id))
    return;
  const auto pending = m_requests.take(id);
  const auto body = message.value("body").toObject();
  const QString action = pending.value("action").toString();
  if (!body.value("success").toBool()) {
    m_info->setText(body.value("message").toString());
    if (action != "list" && action != "detail")
      detail();
    return;
  }
  if (action == "list") {
    const auto requested = pending.value("body").toObject();
    if (requested.value("offset").toInt() != m_offset ||
        requested.value("status").toString() !=
            m_status->currentData().toString() ||
        requested.value("keyword").toString() != m_key->text().trimmed())
      return;
    m_staff = body.value("isSupport").toBool();
    m_role->setText(m_user + (m_staff ? " / 技术支持" : " / 业务用户"));
    m_total = body.value("total").toInt();
    QSignalBlocker block(m_table);
    m_table->setRowCount(0);
    const auto items = body.value("items").toArray();
    QMap<QString, int> counts;
    int selected = -1;
    for (const auto &v : items) {
      auto t = v.toObject();
      ++counts[t.value("status").toString()];
      int r = m_table->rowCount();
      m_table->insertRow(r);
      QStringList cells{t.value("title").toString(),
                        t.value("device").toString(),
                        priorityText(t.value("priority").toString()),
                        statusText(t.value("status").toString()),
                        t.value("creator").toString(),
                        t.value("assignee").toString(),
                        t.value("created_at").toString()};
      for (int c = 0; c < cells.size(); ++c)
        m_table->setItem(r, c, new QTableWidgetItem(cells[c]));
      m_table->item(r, 0)->setData(Qt::UserRole, t.value("id").toString());
      const auto status = t.value("status").toString();
      m_table->item(r, 3)->setForeground(
          QColor(status == "closed"       ? "#73839c"
                 : status == "resolved"   ? "#13836d"
                 : status == "processing" ? "#2d5eea"
                                          : "#b37a22"));
      if (t.value("priority") == "high")
        m_table->item(r, 2)->setForeground(QColor("#ca6352"));
      if (t.value("id") == m_selectedId)
        selected = r;
    }
    for (int i = 0; i < m_metrics.size(); ++i)
      m_metrics[i]->setText(QString::number(counts.value(
          QStringList{"pending", "processing", "resolved", "closed"}[i])));
    if (selected >= 0)
      m_table->selectRow(selected);
    else if (!m_selectedId.isEmpty()) {
      m_selectedId.clear();
      m_ticket = {};
      m_detail->clear();
    }
    m_previous->setEnabled(m_offset > 0);
    m_next->setEnabled(m_offset + 50 < m_total);
    m_info->setText(QString("共 %1 条，当前第 %2 页；每页50条")
                        .arg(m_total)
                        .arg(m_offset / 50 + 1));
    updateActions();
  } else if (action == "detail") {
    if (pending.value("body").toObject().value("id") != m_selectedId)
      return;
    showDetail(body);
  } else {
    if (action == "message")
      m_message->clear();
    if (action == "create")
      m_selectedId = body.value("ticket").toObject().value("id").toString();
    refresh();
    detail();
  }
}
void TicketPanel::showDetail(const QJsonObject &body) {
  m_ticket = body.value("ticket").toObject();
  m_historyOffset = body.value("offset").toInt();
  if (m_historyOffset == 0) {
    m_detail->clear();
    m_detail->append(
        QString("工单：%1\n标题：%2\n软件/设备：%3\n状态：%4\n提交人：%5  "
                "处理人：%6\n故障描述：%7\n解决说明：%8\n")
            .arg(m_ticket.value("id").toString(),
                 m_ticket.value("title").toString(),
                 m_ticket.value("device").toString(),
                 statusText(m_ticket.value("status").toString()),
                 m_ticket.value("creator").toString(),
                 m_ticket.value("assignee").toString(),
                 m_ticket.value("description").toString(),
                 m_ticket.value("resolution").toString())
            .toHtmlEscaped()
            .replace('\n', "<br>"));
  }
  auto append = [this](const QString &s) {
    m_detail->append(s.toHtmlEscaped());
  };
  append(QString("—— 记录页 %1（各取最近50条，页内由新到旧）——")
             .arg(m_historyOffset / 50 + 1));
  for (const auto &v : body.value("messages").toArray()) {
    auto o = v.toObject();
    append(QString("[沟通] %1 %2：%3")
               .arg(o.value("created_at").toString(),
                    o.value("sender").toString(), o.value("text").toString()));
  }
  for (const auto &v : body.value("events").toArray()) {
    auto o = v.toObject();
    append(QString("[操作] %1 %2 %3：%4")
               .arg(o.value("created_at").toString(),
                    o.value("actor").toString(), o.value("action").toString(),
                    o.value("note").toString()));
  }
  updateActions();
  m_older->setEnabled(body.value("messagesHasMore").toBool() ||
                      body.value("eventsHasMore").toBool());
}
void TicketPanel::updateActions() {
  const QString s = m_ticket.value("status").toString();
  const bool owner = m_ticket.value("creator") == m_user;
  m_claim->setEnabled(m_staff && s == "pending");
  m_resolve->setEnabled(m_staff && s == "processing" &&
                        m_ticket.value("assignee") == m_user);
  m_close->setEnabled(owner && s == "resolved");
  m_reopen->setEnabled(owner && s == "resolved");
  m_message->setEnabled(!m_ticket.isEmpty() && s != "closed");
  if (m_ticket.isEmpty()) m_older->setEnabled(false);
}
