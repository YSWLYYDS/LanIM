#ifndef MESSAGEREPOSITORY_H
#define MESSAGEREPOSITORY_H
#include <QList>
#include <QSqlDatabase>
#include <QString>

struct PublicMessage
{
    QString messageId;
    QString sender;
    QString text;
    QString sentAt;
};
struct PrivateMessage
{
    QString messageId;
    QString sender;
    QString recipient;
    QString text;
    QString sentAt;
};


class MessageRepository
{
public:
    explicit MessageRepository(const QSqlDatabase &database);

    bool savePublicMessage(const PublicMessage &message);//将一条消息插入SQLite

    bool loadRecentPublicMessages(int limit,QList<PublicMessage> &messages);//查询最近若干条消息
    QString lastError() const;

    bool savePrivateMessage(const PrivateMessage &message);//服务器收到私聊后，先插入数据库

    bool loadRecentPrivateMessages//查询 A 与 B 两人之间最近的聊天记录；不区分谁发送给谁
        (
        const QString &userA,
        const QString &userB,
        int limit,
        int offset,//决定跳过最新的多少条
        QList<PrivateMessage> &messages);
private:
    QSqlDatabase m_database;
    QString m_lastError;
};

#endif // MESSAGEREPOSITORY_H
