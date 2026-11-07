#pragma once
#include "chatclient.h"
#include <QHash>
#include <QJsonObject>
#include <QWidget>
class QTableWidget;
class QTextEdit;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class TicketPanel : public QWidget {
  Q_OBJECT
public:
  explicit TicketPanel(ChatClient *client, QWidget *parent = nullptr);
  void setSession(const QString &user);
  void createFromReport(const QString &title, const QString &context,
                        const QString &description);

private:
  void request(const QString &action, QJsonObject body = {});
  void refresh();
  void detail(int offset = 0);
  void createTicket();
  void mutate(const QString &action);
  void received(const QJsonObject &message);
  void showDetail(const QJsonObject &body);
  void updateActions();
  QString m_draftTitle, m_draftContext, m_draftDescription;
  ChatClient *m_client;
  QString m_user;
  QJsonObject m_ticket;
  QHash<QString, QJsonObject> m_requests;
  QTableWidget *m_table;
  QTextEdit *m_detail;
  QLineEdit *m_key, *m_message;
  QComboBox *m_status;
  QLabel *m_info, *m_role;
  QPushButton *m_previous, *m_next, *m_older;
  QPushButton *m_claim, *m_resolve, *m_close, *m_reopen;
  int m_offset = 0, m_total = 0, m_historyOffset = 0;
  bool m_staff = false;
  QString m_selectedId;
  QList<QLabel *> m_metrics;
};
