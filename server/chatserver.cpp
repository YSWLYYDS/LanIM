#include "chatserver.h"
#include "protocol.h"
#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>
#include <QJsonObject>
#include "userrepository.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QUuid>
#include "messagerepository.h"
#include <QJsonArray>
#include "ticketrepository.h"


namespace  {

QString hashPassword(const QString &password)
{
    const QByteArray hash=QCryptographicHash::hash(
        password.toUtf8(),
        QCryptographicHash::Sha256);

    return QString::fromLatin1(hash.toHex());
}

}

ChatServer::ChatServer(UserRepository &userRepository,MessageRepository &messageRepository,TicketRepository &tickets,const QStringList &supportUsers,QObject *parent)
    :QObject(parent),m_tickets(tickets),m_supportUsers(supportUsers),m_userRepository(userRepository),m_messageRepository(messageRepository)
{
    connect(&m_tcpServer,&QTcpServer::newConnection,
                this,&ChatServer::onNewConnection);
    m_heartbeatCheckTimer.setInterval(5 * 1000);

    connect(&m_heartbeatCheckTimer, &QTimer::timeout,
            this, &ChatServer::checkHeartbeatTimeouts);
}

bool ChatServer::start(quint16 port)
{
    if(!m_tcpServer.listen(QHostAddress::AnyIPv4,port)){

        qCritical()<<"Failed to listen on port"<<port
                    <<":"<<m_tcpServer.errorString();
        return false;
    }
    m_heartbeatCheckTimer.start();
    qInfo()<<"LanIM server is listening on port"
          <<m_tcpServer.serverPort();

    return true;
}

void ChatServer::onNewConnection()
{
    while(m_tcpServer.hasPendingConnections()){
        QTcpSocket * socket=m_tcpServer.nextPendingConnection();
        setupClient(socket);
    }
}

void ChatServer::setupClient(QTcpSocket *socket)
{
    const QString peer=QString("%1:%2")
                        .arg(socket->peerAddress().toString())//获取客户端的网络地址转为字符串
                             .arg(socket->peerPort());//获取客户端连接时使用的源端口

    qInfo()<<
        "Client connected: "<<peer;

    m_receiveBuffers.insert(socket,QByteArray {});
    m_lastActivityTimes.insert(
        socket, QDateTime::currentDateTimeUtc());
    connect(socket,&QTcpSocket::readyRead,this,[this,socket](){
            onReadyRead(socket);
    });

    connect(socket,&QTcpSocket::disconnected,
            this,[this,socket,peer](){
        //取出这个 socket 对应的用户名，同时从哈希表删除它
        m_receiveBuffers.remove(socket);
        m_lastActivityTimes.remove(socket);
        const QString username=m_sessionManager.removeSession(socket);
        if(!username.isEmpty()){
            qInfo()<<"User logged out:"<<username;
            broadcastUserPresence(username, false);
        }

        qInfo()<<"Client disconnected:"<<peer;
        socket->deleteLater();
    });
}

void ChatServer::onReadyRead(QTcpSocket *socket)
{
    QByteArray &buffer=m_receiveBuffers[socket];
    buffer.append(socket->readAll());//读取此次到达的全部 TCP 字节

    while(true)//半包：保留数据，等待下次;完整消息：打印，并继续拆下一条;非法消息：断开连接
    {
        QJsonObject message;
        QString errormessage;

        const Protocol::DecodeResult result=
            Protocol::unpack(buffer,message,&errormessage);

        if(result==Protocol::DecodeResult::NeedMoreData)//如果缓冲区中的字节还不足以组成一条完整消息，立即结束本次处理。
        {
            return;
        }

        if(result != Protocol::DecodeResult::Success){
            qWarning()<<"Invalid message from"
                       <<socket->peerAddress().toString()
                       <<":"<<errormessage;
            socket->disconnectFromHost();
            return;
        }
        m_lastActivityTimes.insert(
            socket, QDateTime::currentDateTimeUtc());

        const QString type =
            message.value("type").toString();

        QJsonObject logMessage = message;

        if (type == "auth.register"
            || type == "auth.login") {
            QJsonObject body =
                logMessage.value("body").toObject();

            body.insert("password", "***");
            logMessage.insert("body", body);
        }

        // Log routing metadata, not passwords, conversations or fault descriptions.
        qInfo() << "Received type:" << type << "requestId:" << message.value("requestId").toString();

        if(type.startsWith("ticket.")){
            handleTicket(socket,message);
        }
        else if(type=="system.hello"){
            const QJsonObject welcomeMessage{
                {"version",1},
                {"type","system.welcome"},
                {"requestId",message.value("requestId").toString()},
                {
                    "body",QJsonObject{
                        {"message","Welcome to LanIM"}
                    }
                }
            };
            sendMessage(socket,welcomeMessage);
            qInfo()<<"Sent system.welcome.";
        }
        else if(type=="auth.register"){
            handleRegister(socket,message);
        }
        else if(type=="auth.login"){
            handleLogin(socket,message);
        }
        else if(type=="chat.public.send"){
            handlePublicChat(socket,message);
        }
        else if (type == "auth.logout") {
            handleLogout(socket, message);
        }
        else if (type == "chat.private.send") {
            handlePrivateChat(socket, message);
        }
        else if (type == "chat.private.history.request")
        {
            handlePrivateHistoryRequest(socket, message);
        }
        else if (type == "system.ping")//客户端每发一次心跳，服务端立即回包
        {
            const QJsonObject pong{
                {"version", 1},
                {"type", "system.pong"},
                {"requestId",
                 message.value("requestId").toString()},
                {"body", QJsonObject{}}
            };

            sendMessage(socket, pong);
        }
    }


}

void ChatServer::handleTicket(QTcpSocket *socket,const QJsonObject &message)
{
    const QString actor=m_sessionManager.usernameForSocket(socket);
    const QString action=message.value("type").toString().mid(7);
    const auto body=m_tickets.handle(actor,m_supportUsers.contains(actor),action,message.value("body").toObject());
    sendMessage(socket,{{"version",1},{"type","ticket.result"},{"requestId",message.value("requestId")},{"body",body}});
    if(!body.value("success").toBool()||action=="list"||action=="detail")return;
    const QString id=body.value("ticket").toObject().value("id").toString();
    for(auto *peer:m_sessionManager.onlineSockets()){
        const QString user=m_sessionManager.usernameForSocket(peer);
        if(m_tickets.canAccess(user,m_supportUsers.contains(user),id))
            sendMessage(peer,{{"version",1},{"type","ticket.changed"},{"body",QJsonObject{{"id",id}}}});
    }
}

void ChatServer::checkHeartbeatTimeouts()
{
    const QDateTime now =
        QDateTime::currentDateTimeUtc();

    QList<QTcpSocket *> staleSockets;

    for (auto it = m_lastActivityTimes.cbegin();
         it != m_lastActivityTimes.cend();
         ++it) {
        if (it.value().msecsTo(now) > 30 * 1000) {
            staleSockets.append(it.key());
        }
    }

    for (QTcpSocket *socket : staleSockets) {
        qWarning() << "Heartbeat timeout:"
                   << socket->peerAddress().toString();

        m_lastActivityTimes.remove(socket);
        socket->disconnectFromHost();
    }
}

void ChatServer::sendMessage(QTcpSocket *socket, const QJsonObject &message)
{
    const QByteArray frame=Protocol::pack(message);
    if(frame.isEmpty()){
        qWarning()<<"Failed to pack outgoing message.";
        return;
    }

    const qint64 queuedBytes=socket->write(frame);

    if(queuedBytes!=static_cast<qint64>(frame.size())){
        qWarning()<<"Failed to queue complete outgoing message.";
        socket->disconnectFromHost();
    }
}

void ChatServer::handlePublicChat(QTcpSocket *socket, const QJsonObject &message)
{
    if(!m_sessionManager.isLoggedIn(socket))//未登录用户处理
    {
        qWarning()<<"Unauthenticated client tried to send chat message.";
        socket->disconnectFromHost();
        return;
    }
    const QJsonObject body=message.value("body").toObject();
    const QString text=body.value("text").toString().trimmed();//从 JSON 的 body.text 取得消息并去除首尾空格。

    if(text.isEmpty()||text.size()>500)
    {
        qWarning()<<"Invalid public chat message.";
        return;
    }

    const QString sender=m_sessionManager.usernameForSocket(socket);;//公共聊天功能也改为通过新的会话管理器确认身份。服务器仍然只认 socket 绑定的用户名，不信任客户端伪造的发送者字段。

    PublicMessage publicMessage;
    publicMessage.messageId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);//WithoutBraces 表示不要 UUID 外面的 {}
    publicMessage.sender = sender;
    publicMessage.text = text;
    publicMessage.sentAt =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    if (!m_messageRepository.savePublicMessage(publicMessage))
    {
        qWarning() << "Failed to save public message:"
                   << m_messageRepository.lastError();
        return;
    }
    //数据库保存成功后，再构造要广播给客户端的 JSON 消息。
    const QJsonObject broadcast{
        {"version", 1},
        {"type", "chat.public.message"},
        {"body", QJsonObject{
                     {"messageId", publicMessage.messageId},
                     {"sender", publicMessage.sender},
                     {"text", publicMessage.text},
                     {"sentAt", publicMessage.sentAt}
                 }}
    };

    broadcastMessage(broadcast);
    qInfo() << "Public message from" << sender << ":" << text;
}

void ChatServer::handlePrivateChat(QTcpSocket *socket, const QJsonObject &message)
{
    if (!m_sessionManager.isLoggedIn(socket))
    {
        qWarning() << "Unauthenticated client tried to send private message.";
        socket->disconnectFromHost();
        return;
    }

    const QString requestId =
        message.value("requestId").toString();

    const QJsonObject body =
        message.value("body").toObject();

    const QString recipient =
        body.value("recipient").toString().trimmed();

    const QString text =
        body.value("text").toString().trimmed();

    if (recipient.size() < 2 || recipient.size() > 32) {
        sendPrivateChatResult(
            socket, requestId, false,
            "invalid_recipient",
            "收件人用户名不合法。");
        return;
    }

    if (text.isEmpty() || text.size() > 500) {
        sendPrivateChatResult(
            socket, requestId, false,
            "invalid_text",
            "私聊消息长度必须为 1 到 500 个字符。");
        return;
    }
    //从会话中获取真实发送者
    const QString sender =
        m_sessionManager.usernameForSocket(socket);

    if (sender == recipient) {
        sendPrivateChatResult(
            socket, requestId, false,
            "cannot_message_self",
            "不能给自己发送私聊消息。");
        return;
    }

    QTcpSocket *recipientSocket =
        m_sessionManager.socketForUsername(recipient);

    if (recipientSocket == nullptr) {
        sendPrivateChatResult(
            socket, requestId, false,
            "recipient_offline",
            "对方当前不在线。");
        return;
    }

    PrivateMessage privateMessage;
    privateMessage.messageId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    privateMessage.sender = sender;
    privateMessage.recipient = recipient;
    privateMessage.text = text;
    privateMessage.sentAt =
        QDateTime::currentDateTimeUtc().toString(
            Qt::ISODateWithMs);

    if (!m_messageRepository.savePrivateMessage(
            privateMessage))
    {
        qWarning() << "Failed to save private message:"
                   << m_messageRepository.lastError();

        sendPrivateChatResult(
            socket, requestId, false,
            "database_error",
            "保存私聊消息失败。");
        return;
    }

    const QJsonObject delivery{
        {"version", 1},
        {"type", "chat.private.message"},
        {"body", QJsonObject{
                     {"messageId", privateMessage.messageId},
                     {"sender", privateMessage.sender},
                     {"recipient", privateMessage.recipient},
                     {"text", privateMessage.text},
                     {"sentAt", privateMessage.sentAt}
                 }}
    };

    sendMessage(socket, delivery);
    sendMessage(recipientSocket, delivery);

    sendPrivateChatResult(
        socket, requestId, true, "ok", "私聊消息发送成功。");

    qInfo() << "Private message from"
            << sender << "to" << recipient
            << ":" << text;
}

void ChatServer::sendPrivateChatResult(QTcpSocket *socket, const QString &requestId, bool success, const QString &code, const QString &text)
{
    const QJsonObject response{
        {"version", 1},
        {"type", "chat.private.send.result"},
        {"requestId", requestId},
        {"body", QJsonObject{
                     {"success", success},
                     {"code", code},
                     {"message", text}
                 }}
    };

    sendMessage(socket, response);
}

void ChatServer::handlePrivateHistoryRequest(QTcpSocket *socket, const QJsonObject &message)
{
    if (!m_sessionManager.isLoggedIn(socket)) {
        qWarning() << "Unauthenticated client requested private history.";
        socket->disconnectFromHost();
        return;
    }

    const QString requestId =
        message.value("requestId").toString();

    const QJsonObject body =
        message.value("body").toObject();

    const QString peer =
        body.value("peer").toString().trimmed();

    const int requestedLimit =
        body.value("limit").toInt(30);

    const int limit =
        qBound(1, requestedLimit, 50);

    const int offset =
        qMax(0, body.value("offset").toInt());

    const QString currentUser =
        m_sessionManager.usernameForSocket(socket);

    if (peer.size() < 2 || peer.size() > 32
        || peer == currentUser) {
        qWarning() << "Invalid private history request.";
        return;
    }

    sendPrivateHistory(
        socket, requestId, peer, limit, offset);
}

void ChatServer::sendPrivateHistory(QTcpSocket *socket, const QString &requestId, const QString &peer, int limit, int offset)
{
    const QString currentUser =
        m_sessionManager.usernameForSocket(socket);

    QList<PrivateMessage> messages;

    if (!m_messageRepository.loadRecentPrivateMessages(
            currentUser, peer, limit, offset, messages)) {
        qWarning() << "Failed to load private history:"
                   << m_messageRepository.lastError();
        return;
    }

    QJsonArray messageArray;

    for (const PrivateMessage &message : messages) {
        messageArray.append(QJsonObject{
            {"messageId", message.messageId},
            {"sender", message.sender},
            {"recipient", message.recipient},
            {"text", message.text},
            {"sentAt", message.sentAt}
        });
    }

    const QJsonObject response{
        {"version", 1},
        {"type", "chat.private.history"},
        {"requestId", requestId},
        {"body", QJsonObject{
                     {"peer", peer},
                     {"offset", offset},
                     {"hasMore", messages.size() == limit},
                     {"messages", messageArray}
                 }}
    };

    sendMessage(socket, response);
}

void ChatServer::broadcastMessage(
    const QJsonObject &message,
    QTcpSocket *excludedSocket)
{
    const QList<QTcpSocket *> sockets =
        m_sessionManager.onlineSockets();
    //向全部在线用户发送；但可以选择性跳过一个用户
    for (QTcpSocket *socket : sockets) {
        if (socket == excludedSocket) {
            continue;
        }

        sendMessage(socket, message);
    }
}

void ChatServer::handleRegister(QTcpSocket *socket, const QJsonObject &message)
{
    const QString requestedId =message.value("requestId").toString();
    const QJsonObject body=message.value("body").toObject();
    const QString username=body.value("username").toString().trimmed();
    const QString password=body.value("password").toString();

    if(username.size()<2||username.size()>32){
        sendRegisterResult(socket,requestedId,false,"invalid_username","用户名长度必须为 2 到 32 个字符。");
        return;
    }

    if(password.size()<5||password.size()>64){
        sendRegisterResult(socket,requestedId,false,"invalid_password","密码长度必须为5到64个字符");
        return;
    }

    bool exists=false;
    if(!m_userRepository.usernameExists(username,exists)){
        qWarning()<<"Failed to query username:"
                   <<m_userRepository.lastError();

        sendRegisterResult(socket,requestedId,false,"database_error","查询用户名时发生数据库错误。");
        return;
    }

    if(exists){
        sendRegisterResult(socket,requestedId,false,"username_exists","该用户名已被注册。");
        return;
    }

    const QString passwordHash=hashPassword(password);

    if(!m_userRepository.createUser(username,passwordHash)){
        qWarning()<<"Failed to create user:"
                   <<m_userRepository.lastError();

        sendRegisterResult(socket,requestedId,false,"database_error","创建用户时发生数据库错误。");
        return;
    }

    qInfo()<<"User registered: "<<username;

    sendRegisterResult(socket,requestedId,true,"ok","注册成功。");
}

void ChatServer::sendRegisterResult(QTcpSocket *socket, const QString &requestId, bool success, const QString &code, const QString &text)
{
    const QJsonObject response{
        {"version",1},
        {"type","auth.register.result"},
        {"requestId",requestId},
        {"body",QJsonObject{
                {"success",success},
                {"code",code},
                {"message",text}
            }}
    };
    sendMessage(socket,response);
}

void ChatServer::handleLogin(QTcpSocket *socket, const QJsonObject &message)
{
    const QString requestId =message.value("requestId").toString();
    const QJsonObject body=message.value("body").toObject();
    const QString username=body.value("username").toString().trimmed();
    const QString password=body.value("password").toString();
    if(m_sessionManager.isLoggedIn(socket))//防止重复登录
    {
        sendLoginResult(socket,requestId,false,"already_logged_in","当前连接已经登录。");
        return;
    }
    if(username.isEmpty()||password.isEmpty()){
        sendLoginResult(socket,requestId,false,"invalid_request","用户名和密码不能为空。");
        return;
    }
    QString storedPasswordHash;
    bool found=false;

    if(!m_userRepository.findPasswordHash(
            username,storedPasswordHash,found)){
        qWarning()<<"Failed to query login user:"
                   <<m_userRepository.lastError();
        sendLoginResult(socket,requestId,false,"database_error","登录时发生数据库错误。");
        return;
    }

    const QString inputPasswordHash=hashPassword(password);
    if(!found||storedPasswordHash!=inputPasswordHash){
        sendLoginResult(socket,requestId,false,"invalid_credentials","用户名或密码错误。");
        return;
    }
    QTcpSocket *previousSocket =
        m_sessionManager.createOrReplaceSession(socket, username);

    if (previousSocket != nullptr)
    {
        const QJsonObject kickedMessage{
            {"version", 1},
            {"type", "auth.kicked"},
            {"body", QJsonObject{
                         {"message", "该账号已在其他客户端登录。"}
                     }}
        };

        sendMessage(previousSocket, kickedMessage);
        previousSocket->disconnectFromHost();

        qInfo() << "Previous session kicked:" << username;
    }

    qInfo() << "User logged in:" << username;
    sendLoginResult(socket, requestId, true, "ok", "登录成功。");
    sendPublicHistory(socket);
    sendOnlineUserList(socket);
    //同账号互踢，账号逻辑上一直在线，因此不重复广播上线事件
    if (previousSocket == nullptr)
    {
        broadcastUserPresence(username, true, socket);
    }
}

void ChatServer::sendLoginResult(QTcpSocket *socket, const QString &requestId, bool success, const QString &code, const QString &text)
{
    const QJsonObject response{
        {"version",1},
        {"type","auth.login.result"},
        {"requestId",requestId},
        {"body",QJsonObject{
                {"success",success},
                {"code",code},
                {"message",text}
            }}
    };
    sendMessage(socket,response);
}

void ChatServer::handleLogout(QTcpSocket *socket, const QJsonObject &message)
{
    const QString requestId=message.value("requestId").toString();

    if(!m_sessionManager.isLoggedIn(socket)){
        sendLogoutResult(socket,requestId,false,"not_logged_in","当前连接尚未登录。");
        return;
    }

    const QString username =
        m_sessionManager.removeSession(socket);//取出当前登录用户名，方便记录日志,从在线会话表中删除当前 socket


    qInfo()<<"User logged out:"<<username;
    sendLogoutResult(socket,requestId,true,"ok","退出登录成功。");
    broadcastUserPresence(username, false);
}

void ChatServer::sendLogoutResult(QTcpSocket *socket, const QString &requestId, bool success, const QString &code, const QString &text)
{
    const QJsonObject response{
        {"version", 1},
        {"type", "auth.logout.result"},
        {"requestId", requestId},
        {"body", QJsonObject{
                     {"success", success},
                     {"code", code},
                     {"message", text}
                 }}
    };
    sendMessage(socket,response);
}

void ChatServer::sendPublicHistory(QTcpSocket *socket)
{
    QList<PublicMessage>messages;

    if(!m_messageRepository.loadRecentPublicMessages(50,messages)){
        qWarning()<<"Failed to load public message history:"
                   <<m_messageRepository.lastError();
        return;
    }

    QJsonArray messageArray;

    for(const PublicMessage &message :messages){
        messageArray.append(QJsonObject{
            {"messageId",message.messageId},
            {"sender",message.sender},
            {"text",message.text},
            {"sentAt",message.sentAt}
        });
    }

    const QJsonObject historyMessage{
        {"version",1},
        {"type","chat.public.history"},
        {"body",QJsonObject{
                {"messages",messageArray}
            }}
    };
    sendMessage(socket,historyMessage);
}

void ChatServer::sendOnlineUserList(QTcpSocket *socket)
{
    QJsonArray users;
    const QStringList usernames=m_sessionManager.onlineUsernames();
    for(const QString &username:usernames){
        users.append(username);
    }

    const QJsonObject message{
        {"version",1},
        {"type","presence.online.list"},
        {"body",QJsonObject{
                {"users",users}
            }}
    };

    sendMessage(socket,message);
}

void ChatServer::broadcastUserPresence(const QString &username, bool online, QTcpSocket *excludedSocket)
{
    const QJsonObject message{
        {"version",1},
        {"type",online?"presence.user.online":"presence.user.offline"},
        {"body",QJsonObject{
                {"username",username}
            }}
    };
    broadcastMessage(message,excludedSocket);
}


