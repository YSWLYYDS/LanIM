#ifndef CHATSERVER_H
#define CHATSERVER_H
//监听端口，发现新连接

#include <QObject>
#include <QTcpServer>
#include <QtGlobal>
#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>
#include "sessionmanager.h"

class QTcpSocket;
class UserRepository;
class MessageRepository;

class ChatServer :public QObject
{
public:
    explicit ChatServer(UserRepository &userRepository,MessageRepository &messageRepository,QObject *parent = nullptr);

    bool start(quint16 port);

private:
    void onNewConnection();//当有客户端连上时被调用
    void setupClient(QTcpSocket *socket);//为一个新客户端安装断开、收数据等处理逻辑
    void onReadyRead(QTcpSocket *socket);//处理某个客户端有新网络数据到达
    void checkHeartbeatTimeouts();//超时检测
    void sendMessage(QTcpSocket *socket,const QJsonObject &message);
    void handlePublicChat(QTcpSocket *socket,const QJsonObject &message);//校验一位用户发来的公共消息，并生成服务器消息
    void handlePrivateChat(QTcpSocket *socket,
                           const QJsonObject &message);//验证发送者、收件人、消息内容；保存消息；投递给双方

    void sendPrivateChatResult(QTcpSocket *socket,
                               const QString &requestId,
                               bool success,
                               const QString &code,
                               const QString &text);//统一返回“发送成功 / 对方不在线 / 参数非法”等结果
    void handlePrivateHistoryRequest(
        QTcpSocket *socket,
        const QJsonObject &message);//解析客户端请求

    void sendPrivateHistory(
        QTcpSocket *socket,
        const QString &requestId,
        const QString &peer,
        int limit,
        int offset);//查询数据库并组装历史消息响应
    void broadcastMessage(const QJsonObject &message,
                          QTcpSocket *excludedSocket = nullptr);//把一条已经准备好的消息发送给所有在线且已登录的用户,新用户已经收到完整在线列表，就不必再收到一条“自己上线”的事件
    void handleRegister(QTcpSocket *socket,const QJsonObject &message);//处理一条auth.register请求。
    void sendRegisterResult(QTcpSocket *socket,const QString &requestId,
                            bool success,const QString &code,const QString &text);//统一生成并发送注册结果，避免每个错误分支重复组装 JSON。
    void handleLogin(QTcpSocket *socket,const QJsonObject &message);//负责登录业务判断。

    void sendLoginResult(QTcpSocket *socket,
                         const QString &requestId,
                         bool success,
                         const QString &code,
                         const QString &text);//负责统一生成登录结果 JSON

    void handleLogout(QTcpSocket *socket,
                      const QJsonObject &message);//验证当前连接是否已登录，并移除登录状态。

    void sendLogoutResult(QTcpSocket *socket,
                          const QString &requestId,
                          bool success,
                          const QString &code,
                          const QString &text);
    void sendPublicHistory(QTcpSocket *socket);//向当前登录用户发送最近公共消息
    void sendOnlineUserList(QTcpSocket *socket);//给一个刚登录成功的客户端发送全量名单
    void broadcastUserPresence(const QString &username,
                               bool online,
                               QTcpSocket *excludedSocket = nullptr);//给其他在线客户端发送某人上线或下线的增量事件

    UserRepository &m_userRepository;//用户注册、登录查询
    MessageRepository &m_messageRepository;//公共消息保存、历史查询
    QTcpServer m_tcpServer;//负责监听端口、接受连接
    QHash<QTcpSocket *, QByteArray> m_receiveBuffers;//保存每个连接尚未拆完的网络字节
    SessionManager m_sessionManager;//保存每个已登录连接对应的用户名
    QHash<QTcpSocket *, QDateTime> m_lastActivityTimes;
    QTimer m_heartbeatCheckTimer;

};

#endif // CHATSERVER_H
