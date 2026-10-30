#include "sessionmanager.h"


bool SessionManager::isLoggedIn(QTcpSocket *socket) const
{
    return m_usernamesBySocket.contains(socket);
}

QString SessionManager::usernameForSocket(QTcpSocket *socket) const
{
    return m_usernamesBySocket.value(socket);
}

QTcpSocket *SessionManager::socketForUsername(const QString &username) const
{
    return m_socketsByUsername.value(username, nullptr);
}

QTcpSocket *SessionManager::createOrReplaceSession(QTcpSocket *socket, const QString &username)
{
    QTcpSocket *previousSocket=socketForUsername(username);

    if(previousSocket==socket){
        return nullptr;
    }

    if(previousSocket != nullptr){
        m_usernamesBySocket.remove(previousSocket);
    }

    const QString oldUsername=m_usernamesBySocket.take(socket);


    if(!oldUsername.isEmpty()&&m_socketsByUsername.value(oldUsername)==socket){
        m_socketsByUsername.remove(oldUsername);
    }

    m_usernamesBySocket.insert(socket, username);
    m_socketsByUsername.insert(username, socket);

    return previousSocket;

}

QString SessionManager::removeSession(QTcpSocket *socket)
{
    const QString username=m_usernamesBySocket.take(socket);

    if(!username.isEmpty()&&m_socketsByUsername.value(username)==socket){
        m_socketsByUsername.remove(username);
    }

    return username;
}

QList<QTcpSocket *> SessionManager::onlineSockets() const
{
    return m_usernamesBySocket.keys();
}

QStringList SessionManager::onlineUsernames() const
{
    QStringList usernames;
    //遍历 用户名 → socket 映射表
    for(auto it = m_socketsByUsername.cbegin();it!=m_socketsByUsername.cend();++it){
        usernames.append(it.key());
    }

    usernames.sort();
    return usernames;
}
