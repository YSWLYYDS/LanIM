#include "ticketrepository.h"
#include <QDateTime>
#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>
#include <algorithm>

namespace {
QString now() {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
QJsonObject row(const QSqlQuery &q) {
  QJsonObject o;
  for (int i = 0; i < q.record().count(); ++i)
    o.insert(q.record().fieldName(i), QJsonValue::fromVariant(q.value(i)));
  return o;
}
QJsonObject ok() {
  return {{"success", true}, {"message", QStringLiteral("操作成功")}};
}
bool validText(const QString &s, int max) {
  return !s.trimmed().isEmpty() && s.size() <= max;
}
} // namespace
bool TicketRepository::initialize() {
  const QStringList schema{
      "CREATE TABLE IF NOT EXISTS tickets(id TEXT PRIMARY KEY,creator TEXT NOT "
      "NULL,assignee TEXT NOT NULL DEFAULT '',title TEXT NOT NULL,device TEXT "
      "NOT NULL,description TEXT NOT NULL,priority TEXT NOT NULL,status TEXT "
      "NOT NULL DEFAULT 'pending',resolution TEXT NOT NULL DEFAULT '',version "
      "INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL,updated_at TEXT NOT "
      "NULL,client_key TEXT NOT NULL,UNIQUE(creator,client_key))",
      "CREATE TABLE IF NOT EXISTS ticket_messages(id INTEGER PRIMARY KEY "
      "AUTOINCREMENT,ticket_id TEXT NOT NULL,sender TEXT NOT NULL,text TEXT "
      "NOT NULL,created_at TEXT NOT NULL)",
      "CREATE TABLE IF NOT EXISTS ticket_events(id INTEGER PRIMARY KEY "
      "AUTOINCREMENT,ticket_id TEXT NOT NULL,actor TEXT NOT NULL,action TEXT "
      "NOT NULL,note TEXT NOT NULL,created_at TEXT NOT NULL)",
      "CREATE INDEX IF NOT EXISTS idx_tickets_creator_time ON "
      "tickets(creator,created_at DESC,id)",
      "CREATE INDEX IF NOT EXISTS idx_tickets_status ON "
      "tickets(status,updated_at DESC,id)",
      "CREATE INDEX IF NOT EXISTS idx_ticket_messages ON "
      "ticket_messages(ticket_id,id)",
      "CREATE INDEX IF NOT EXISTS idx_ticket_events ON "
      "ticket_events(ticket_id,id)"};
  for (const auto &s : schema) {
    QSqlQuery q(m_db);
    if (!q.exec(s)) {
      m_error = q.lastError().text();
      return false;
    }
  }
  return true;
}
QJsonObject TicketRepository::fail(const QString &code,
                                   const QString &message) const {
  return {{"success", false}, {"code", code}, {"message", message}};
}
QJsonObject TicketRepository::ticket(const QString &id) {
  QSqlQuery q(m_db);
  q.prepare("SELECT "
            "id,creator,assignee,title,device,description,priority,status,"
            "resolution,version,created_at,updated_at FROM tickets WHERE id=?");
  q.addBindValue(id);
  if (!q.exec()) {
    m_error = q.lastError().text();
    return {};
  }
  return q.next() ? row(q) : QJsonObject{};
}
bool TicketRepository::canAccess(const QString &actor, bool staff,
                                 const QString &id) {
  const auto t = ticket(id);
  return !actor.isEmpty() && !t.isEmpty() &&
         (staff || t.value("creator").toString() == actor);
}
bool TicketRepository::event(const QString &id, const QString &actor,
                             const QString &action, const QString &note) {
  QSqlQuery q(m_db);
  q.prepare("INSERT INTO ticket_events(ticket_id,actor,action,note,created_at) "
            "VALUES(?,?,?,?,?)");
  for (const auto &v : QStringList{id, actor, action, note, now()})
    q.addBindValue(v.isNull() ? QStringLiteral("") : v);
  if (!q.exec()) {
    m_error = q.lastError().text();
    return false;
  }
  return true;
}
QJsonObject TicketRepository::handle(const QString &actor, bool staff,
                                     const QString &action,
                                     const QJsonObject &b) {
  m_error.clear();
  if (actor.isEmpty())
    return fail("not_logged_in", QStringLiteral("请先登录"));
  const auto dbError = [this]() {
    return fail("database_error", QStringLiteral("数据库操作失败，未提交更改"));
  };
  if (action == "list") {
    const QString status = b.value("status").toString();
    if (!status.isEmpty() &&
        !QStringList{"pending", "processing", "resolved", "closed"}.contains(
            status))
      return fail("invalid_request", QStringLiteral("状态无效"));
    QString key = b.value("keyword").toString().trimmed();
    if (key.size() > 100)
      return fail("invalid_request", QStringLiteral("关键词过长"));
    key.replace("\\", "\\\\");
    key.replace("%", "\\%");
    key.replace("_", "\\_");
    const int offset = std::clamp(b.value("offset").toInt(), 0, 1000000);
    const QString where =
        " WHERE (?=1 OR creator=?) AND (?='' OR status=?) AND (title LIKE ? "
        "ESCAPE '\\' OR device LIKE ? ESCAPE '\\')";
    auto bind = [&](QSqlQuery &q) {
      q.addBindValue(staff ? 1 : 0);
      q.addBindValue(actor);
      q.addBindValue(status.isEmpty() ? QStringLiteral("") : status);
      q.addBindValue(status.isEmpty() ? QStringLiteral("") : status);
      q.addBindValue("%" + key + "%");
      q.addBindValue("%" + key + "%");
    };
    QSqlQuery count(m_db);
    count.prepare("SELECT COUNT(*) FROM tickets" + where);
    bind(count);
    if (!count.exec() || !count.next())
      return dbError();
    const int total = count.value(0).toInt();
    QSqlQuery q(m_db);
    q.prepare("SELECT "
              "id,creator,assignee,title,device,priority,status,version,"
              "created_at,updated_at FROM tickets" +
              where + " ORDER BY created_at DESC,id DESC LIMIT 50 OFFSET ?");
    bind(q);
    q.addBindValue(offset);
    if (!q.exec())
      return dbError();
    QJsonArray items;
    while (q.next())
      items.append(row(q));
    return {{"success", true},
            {"items", items},
            {"total", total},
            {"offset", offset},
            {"isSupport", staff}};
  }
  if (action == "create") {
    const QString title = b.value("title").toString().trimmed(),
                  device = b.value("device").toString().trimmed(),
                  description = b.value("description").toString().trimmed();
    const QString priority = b.value("priority").toString(),
                  key = b.value("clientKey").toString();
    if (!validText(title, 100) || !validText(device, 100) ||
        !validText(description, 2000) || !validText(key, 64) ||
        !QStringList{"low", "normal", "high"}.contains(priority))
      return fail(
          "invalid_request",
          QStringLiteral("标题/软件或设备/故障描述必填，优先级或长度不合法"));
    QSqlQuery old(m_db);
    old.prepare("SELECT id,title,device,description,priority FROM tickets "
                "WHERE creator=? AND client_key=?");
    old.addBindValue(actor);
    old.addBindValue(key);
    if (!old.exec())
      return dbError();
    if (old.next()) {
      if (old.value(1).toString() != title ||
          old.value(2).toString() != device ||
          old.value(3).toString() != description ||
          old.value(4).toString() != priority)
        return fail("key_conflict", QStringLiteral("重复请求标识对应不同内容"));
      auto result = ok();
      result["ticket"] = ticket(old.value(0).toString());
      result["replayed"] = true;
      return result;
    }
    if (!m_db.transaction())
      return dbError();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces),
                  time = now();
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO "
              "tickets(id,creator,title,device,description,priority,created_at,"
              "updated_at,client_key) VALUES(?,?,?,?,?,?,?,?,?)");
    for (const auto &v : QStringList{id, actor, title, device, description,
                                     priority, time, time, key})
      q.addBindValue(v);
    if (!q.exec() || !event(id, actor, "created", title) || !m_db.commit()) {
      m_db.rollback();
      return dbError();
    }
    auto result = ok();
    result["ticket"] = ticket(id);
    return result;
  }
  const QString id = b.value("id").toString();
  const auto t = ticket(id);
  if (t.isEmpty())
    return m_error.isEmpty() ? fail("not_found", QStringLiteral("工单不存在"))
                             : dbError();
  if (!staff && t.value("creator").toString() != actor)
    return fail("forbidden", QStringLiteral("无权查看或操作此工单"));
  if (action == "detail") {
    const int offset = std::clamp(b.value("offset").toInt(), 0, 1000000);
    QJsonObject result = ok();
    result["ticket"] = t;
    result["offset"] = offset;
    for (const QString &table :
         QStringList{"ticket_messages", "ticket_events"}) {
      QSqlQuery q(m_db);
      q.prepare("SELECT * FROM " + table +
                " WHERE ticket_id=? ORDER BY id DESC LIMIT 51 OFFSET ?");
      q.addBindValue(id);
      q.addBindValue(offset);
      if (!q.exec())
        return dbError();
      QJsonArray items;
      while (q.next())
        items.append(row(q));
      const bool more = items.size() > 50;
      if (more)
        items.removeLast();
      const QString name = table == "ticket_messages" ? "messages" : "events";
      result[name] = items;
      result[name + "HasMore"] = more;
    }
    return result;
  }
  if (action == "message") {
    const QString text = b.value("text").toString().trimmed();
    if (!validText(text, 2000))
      return fail("invalid_request", QStringLiteral("消息长度应为1～2000字符"));
    if (t.value("status") == "closed")
      return fail("invalid_state", QStringLiteral("已关闭工单不能继续留言"));
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO ticket_messages(ticket_id,sender,text,created_at) "
              "VALUES(?,?,?,?)");
    for (const auto &v : QStringList{id, actor, text, now()})
      q.addBindValue(v);
    if (!q.exec())
      return dbError();
    auto result = ok();
    result["ticket"] = t;
    return result;
  }
  QString next, assignee = t.value("assignee").toString(),
                resolution = t.value("resolution").toString();
  const QString state = t.value("status").toString();
  const QString note = b.value("note").toString().trimmed();
  if (note.size() > 2000)
    return fail("invalid_request", QStringLiteral("说明过长"));
  if (action == "claim" && staff && state == "pending") {
    next = "processing";
    assignee = actor;
  } else if (action == "resolve" && staff && assignee == actor &&
             state == "processing" && validText(note, 2000)) {
    next = "resolved";
    resolution = note;
  } else if (action == "close" && t.value("creator") == actor &&
             state == "resolved")
    next = "closed";
  else if (action == "reopen" && t.value("creator") == actor &&
           state == "resolved" && validText(note, 2000)) {
    next = "pending";
    assignee = "";
    resolution = "";
  } else
    return fail("forbidden_or_state",
                QStringLiteral("权限或状态不允许；解决/退回需要填写说明"));
  if (!b.value("version").isDouble() ||
      b.value("version").toInt(-1) != t.value("version").toInt())
    return fail("conflict", QStringLiteral("工单已变化，请刷新后重试"));
  if (!m_db.transaction())
    return dbError();
  QSqlQuery q(m_db);
  q.prepare("UPDATE tickets SET "
            "status=?,assignee=?,resolution=?,updated_at=?,version=version+1 "
            "WHERE id=? AND version=?");
  q.addBindValue(next);
  q.addBindValue(assignee.isEmpty() ? QStringLiteral("") : assignee);
  q.addBindValue(resolution.isEmpty() ? QStringLiteral("") : resolution);
  q.addBindValue(now());
  q.addBindValue(id);
  q.addBindValue(t.value("version").toInt());
  if (!q.exec()) {
    m_db.rollback();
    return dbError();
  }
  if (q.numRowsAffected() != 1) {
    m_db.rollback();
    return fail("conflict", QStringLiteral("工单已变化，请刷新后重试"));
  }
  if (!event(id, actor, action, note) || !m_db.commit()) {
    m_db.rollback();
    return dbError();
  }
  auto result = ok();
  result["ticket"] = ticket(id);
  return result;
}
