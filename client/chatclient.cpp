#include "chatclient.h"

#include <QAbstractSocket>
#include <QUuid>
#include "protocol.h"
ChatClient::ChatClient(QObject *parent)
    :QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected,
            this, [this]() {
        m_reconnectTimer.stop();
        m_lastPongTimer.start();
        m_heartbeatTimer.start();

        emit connected();
            });

    connect(&m_socket, &QTcpSocket::disconnected,
            this, [this]() {
        m_heartbeatTimer.stop();
        emit disconnected();
        scheduleReconnect();
            });
    connect(&m_socket,&QTcpSocket::errorOccurred,this,
            [this](QAbstractSocket::SocketError){
                emit connectionError(m_socket.errorString());
                scheduleReconnect();//异常断线或连接失败时，自动安排下一次重连
    });
    connect(&m_socket,&QTcpSocket::readyRead,this,&ChatClient::onReadyRead);
    m_heartbeatTimer.setInterval(10 * 1000);//CP 建连成功：启动 10 秒定时器

    connect(&m_heartbeatTimer, &QTimer::timeout,
            this, &ChatClient::sendHeartbeat);

    //每次只等待 3 秒并尝试一次；如果仍然失败，断开/错误信号会再次安排下一次重试
    m_reconnectTimer.setInterval(3 * 1000);
    m_reconnectTimer.setSingleShot(true);

    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &ChatClient::reconnectToServer);
}

void ChatClient::connectToServer(const QHostAddress &address, quint16 port)
{
    m_serverAddress = address;
    m_serverPort = port;
    m_shouldReconnect = true;

    m_reconnectTimer.stop();
    m_receiveBuffer.clear();

    if (m_socket.state()
        == QAbstractSocket::UnconnectedState) {
        m_socket.connectToHost(
            m_serverAddress, m_serverPort);
    }
}

void ChatClient::disconnectFromServer()
{

    m_shouldReconnect = false;
    m_reconnectTimer.stop();
    m_heartbeatTimer.stop();
    if(m_socket.state()!=QAbstractSocket::UnconnectedState)//判断是否已经断开
    {
        m_socket.disconnectFromHost();
    }
}

bool ChatClient::sendMessage(const QJsonObject &message)
{
    if(!isConnected()){
        return false;
    }
    const QByteArray frame=Protocol::pack(message);

    if(frame.isEmpty()){
        return false;
    }

    return m_socket.write(frame)==frame.size();
}

bool ChatClient::isConnected() const
{
    return m_socket.state()==QAbstractSocket::ConnectedState;
}

void ChatClient::onReadyRead()
{
    m_receiveBuffer.append(m_socket.readAll());
    while(true){
        QJsonObject message;
        QString errorMessage;

        const Protocol::DecodeResult result=
            Protocol::unpack(m_receiveBuffer,message,&errorMessage);

        if(result==Protocol::DecodeResult::NeedMoreData)//如果缓冲区中的字节还不足以组成一条完整消息，立即结束本次处理。
        {
            return;
        }

        if(result != Protocol::DecodeResult::Success){
            emit protocolError(errorMessage);
            m_socket.disconnectFromHost();
            return;
        }
        if(message.value("type").toString() == "system.pong")//`pong` 是连接层内部消息，不需要交给 MainWindow 的聊天、登录、私聊分支处理
        {
            m_lastPongTimer.restart();
            continue;
        }

        emit messageReceived(message);
    }
}

void ChatClient::sendHeartbeat()
{
    if (!isConnected()) {
        return;
    }
    if (m_lastPongTimer.isValid()
        && m_lastPongTimer.elapsed() > 30 * 1000)
    {
        emit connectionError("心跳超时，准备重新连接。");
        m_socket.abort();
        return;
    }

    const QJsonObject ping{
        {"version", 1},
        {"type", "system.ping"},
        {"requestId",
         QUuid::createUuid().toString(
             QUuid::WithoutBraces)},
        {"body", QJsonObject{}}
    };

    if (!sendMessage(ping))
    {
        m_socket.disconnectFromHost();
    }
}

void ChatClient::scheduleReconnect()
{
    if (!m_shouldReconnect
        || m_serverPort == 0
        || m_reconnectTimer.isActive()
        || m_socket.state()
               != QAbstractSocket::UnconnectedState) {
        return;
    }

    m_reconnectTimer.start();
}

void ChatClient::reconnectToServer()
{
    if (!m_shouldReconnect
        || m_socket.state()
               != QAbstractSocket::UnconnectedState) {
        return;
    }

    m_receiveBuffer.clear();

    m_socket.connectToHost(
        m_serverAddress, m_serverPort);
}



