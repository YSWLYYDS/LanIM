#pragma once
#include "reportmodel.h"
#include <QFuture>
class ReportAnalytics;
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QWidget>
class QLineEdit;
class QDateEdit;
class QTableView;
class QLabel;
class QPushButton;
class QNetworkReply;
class ReportPanel : public QWidget {
  Q_OBJECT
public:
  explicit ReportPanel(QWidget *parent = nullptr);
  ~ReportPanel() override;

signals:
  void issueRequested(const QString &title, const QString &context,
                      const QString &description);

private:
  void reload(bool resetPage = true);
  void send(quint64 generation, int attempt);
  void exportReport();
  void importJsonFile(QString path = {});
  void acceptData(QFuture<ReportData> future, quint64 generation,
                  const QString &source, bool offline);
  void finishState();

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  QNetworkAccessManager m_network;
  QPointer<QNetworkReply> m_reply;
  ReportModel *m_model;
  QLineEdit *m_url, *m_channel, *m_campaign, *m_token;
  QDateEdit *m_from, *m_to;
  QTableView *m_view;
  QLabel *m_info;
  QPushButton *m_export, *m_previous, *m_next;
  quint64 m_generation = 0;
  int m_offset = 0, m_total = 0;
  QUrl m_requestUrl;
  QString m_requestToken;
  QString m_displayContext;
  ReportAnalytics *m_analytics;
  QList<QLabel *> m_metrics;
  QStringList m_requestLog;
  QTimer m_autoRefresh;
  bool m_busy = false, m_offline = false, m_exportBusy = false;
  int m_displayOffset = 0;
};
