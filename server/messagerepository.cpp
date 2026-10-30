#include "messagerepository.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>


MessageRepository::MessageRepository(const QSqlDatabase &database)
    :m_database(database)
{

}

bool MessageRepository::savePublicMessage(const PublicMessage &message)
{
    m_lastError.clear();
    QSqlQuery query(m_database);

    query.prepare(
        "INSERT INTO public_messages "
        "(message_id, sender, text, sent_at) "
        "VALUES (:message_id, :sender, :text, :sent_at)"
        );

    query.bindValue(":message_id",message.messageId);
    query.bindValue(":sender",message.sender);
    query.bindValue(":text",message.text);
    query.bindValue(":sent_at",message.sentAt);

    if(!query.exec())
    {
        m_lastError=query.lastError().text();
        return false;
    }
    return true;
}

bool MessageRepository::loadRecentPublicMessages(int limit, QList<PublicMessage> &messages)
{
    m_lastError.clear();
    messages.clear();

    if(limit<=0)
    {
        return true;
    }

    QSqlQuery query(m_database);

    query.prepare(
        "SELECT message_id, sender, text, sent_at "
        "FROM public_messages "
        "ORDER BY sent_at DESC "
        "LIMIT :limit");

    query.bindValue(":limit",limit);

    if(!query.exec())
    {
        m_lastError=query.lastError().text();
        return false;
    }

    while (query.next())
    {
        PublicMessage message;
        message.messageId=query.value(0).toString();
        message.sender=query.value(1).toString();
        message.text=query.value(2).toString();
        message.sentAt=query.value(3).toString();

        messages.prepend(message);
    }
    return true;
}

QString MessageRepository::lastError() const
{
    return m_lastError;
}

bool MessageRepository::savePrivateMessage(const PrivateMessage &message)
{
    m_lastError.clear();
    QSqlQuery query(m_database);

    query.prepare("INSERT INTO private_messages "
                  "(message_id, sender, recipient, text, sent_at) "
                  "VALUES (:message_id, :sender, :recipient, :text, :sent_at)");

    query.bindValue(":message_id", message.messageId);
    query.bindValue(":sender", message.sender);
    query.bindValue(":recipient", message.recipient);
    query.bindValue(":text", message.text);
    query.bindValue(":sent_at", message.sentAt);

    if(!query.exec())
    {
        m_lastError=query.lastError().text();
        return false;
    }
    return true;
}

bool MessageRepository::loadRecentPrivateMessages(const QString &userA, const QString &userB, int limit,int offset, QList<PrivateMessage> &messages)
{
    m_lastError.clear();
    messages.clear();

    if(limit<=0)
    {
        return true;
    }

    QSqlQuery query(m_database);

    query.prepare("SELECT message_id, sender, recipient, text, sent_at "
                  "FROM private_messages "
                  "WHERE (sender = :user_a AND recipient = :user_b) "
                  "OR (sender = :user_b AND recipient = :user_a) "
                  "ORDER BY sent_at DESC "
                  "LIMIT :limit OFFSET :offset");

    query.bindValue(":user_a", userA);
    query.bindValue(":user_b", userB);
    query.bindValue(":limit", limit);
    query.bindValue(":offset", offset);

    if(!query.exec())
    {
        m_lastError = query.lastError().text();
        return false;
    }

    while (query.next())
    {
        PrivateMessage message;
        message.messageId = query.value(0).toString();
        message.sender = query.value(1).toString();
        message.recipient = query.value(2).toString();
        message.text = query.value(3).toString();
        message.sentAt = query.value(4).toString();

        messages.prepend(message);
    }
    return true;
}

