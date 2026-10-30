#ifndef CHATCLIENT_H
#define CHATCLIENT_H
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QtGlobal>
#include <QJsonObject>
#include <QByteArray>
#include <QTimer>
#include <QElapsedTimer>
class ChatClient :public QObject
{
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent=nullptr);

    void connectToServer(const QHostAddress &address, quint16 port);
    void disconnectFromServer();
    bool sendMessage(const QJsonObject &message);//发送业务层准备好的JSON
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &message);
    void messageReceived(const QJsonObject &message);//客户端成功收到一条完整 LanIM JSON 消息时发出
    void protocolError(const QString &message);//连接仍存在，但收到的数据违反长度头或 JSON 协议时发出
private:
    void onReadyRead();
    void sendHeartbeat();
    void scheduleReconnect();
    void reconnectToServer();
    QByteArray m_receiveBuffer;
    QTcpSocket m_socket;
    QTimer m_heartbeatTimer;
    QTimer m_reconnectTimer;//断线后等待 3 秒再重连

    QElapsedTimer m_lastPongTimer;//记录上次收到 pong 到现在过去多久
    //保存重连目标
    QHostAddress m_serverAddress;
    quint16 m_serverPort = 0;

    bool m_shouldReconnect = false;//区分“网络异常断线”与“程序主动退出
};

#endif // CHATCLIENT_H
