#pragma once
#include <QJsonObject>
#include <QSqlDatabase>
#include <QStringList>

// Only the server supplies actor/role. Neither is accepted from client JSON.
class TicketRepository {
public:
  explicit TicketRepository(const QSqlDatabase &db) : m_db(db) {}
  bool initialize();
  QJsonObject handle(const QString &actor, bool staff, const QString &action,
                     const QJsonObject &body);
  bool canAccess(const QString &actor, bool staff, const QString &id);
  QString lastError() const { return m_error; }

private:
  QSqlDatabase m_db;
  QString m_error;
  QJsonObject fail(const QString &code, const QString &message) const;
  QJsonObject ticket(const QString &id);
  bool event(const QString &id, const QString &actor, const QString &action,
             const QString &note);
};
