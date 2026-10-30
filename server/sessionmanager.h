#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QTcpSocket;

class SessionManager
{
public:

    bool isLoggedIn(QTcpSocket *socket)const;

    QString usernameForSocket(QTcpSocket *socket)const;//由连接查用户名

    QTcpSocket *socketForUsername(const QString &username) const;//由用户名查连接，私聊时使用

    QTcpSocket *createOrReplaceSession//创建新会话；若账号已在线，返回旧 socket
        (
        QTcpSocket *socket,
        const QString &username);

    QString removeSession(QTcpSocket *socket);//退出或断线时删除会话

    QList<QTcpSocket *>onlineSockets() const;//获取所有在线用户连接，用于广播
    QStringList onlineUsernames() const;//新登录用户生成“在线名单快照”

private:
    QHash<QTcpSocket *,QString>m_usernamesBySocket;
    QHash<QString,QTcpSocket *>m_socketsByUsername;
};

#endif // SESSIONMANAGER_H
