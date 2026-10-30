#include "databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

DatabaseManager::DatabaseManager(const QString &databasePath)
    :m_databasePath(databasePath){

}

bool DatabaseManager::open()
{
    m_lastError.clear();

    const QString connectionName = "lanim_server";

    if(QSqlDatabase::contains(connectionName))//防止重名
    {
        m_database=QSqlDatabase::database(connectionName);
    }
    else
    {
        m_database=QSqlDatabase::addDatabase("QSQLITE",connectionName);
    }

    m_database.setDatabaseName(m_databasePath);

    if(!m_database.open()){
        m_lastError=m_database.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::initializeSchema()
{
    if(!m_database.isOpen())
    {
        m_lastError="database is not open.";
        return false;
    }

    QSqlQuery query(m_database);

    const QString createUsersTable=R"(
        CREATE TABLE IF NOT EXISTS users(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if(!query.exec(createUsersTable))
    {
        m_lastError=query.lastError().text();
        return false;
    }

    const QString createPublicMessageTable=R"(
    CREATE TABLE IF NOT EXISTS public_messages(
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                message_id TEXT NOT NULL UNIQUE,
                sender TEXT NOT NULL,
                text TEXT NOT NULL,
                sent_at TEXT NOT NULL
            )
    )";

    if(!query.exec(createPublicMessageTable))
    {
        m_lastError = query.lastError().text();
        return false;
    }

    const QString createMessageTimeIndex = R"(
        CREATE INDEX IF NOT EXISTS idx_public_messages_sent_at
        ON public_messages(sent_at DESC)
    )";

    if(!query.exec(createMessageTimeIndex))
    {
        m_lastError=query.lastError().text();
        return false;
    }


    const QString createPrivateMessageTable = R"(
    CREATE TABLE IF NOT EXISTS private_messages(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        message_id TEXT NOT NULL UNIQUE,
        sender TEXT NOT NULL,
        recipient TEXT NOT NULL,
        text TEXT NOT NULL,
        sent_at TEXT NOT NULL,
        delivered INTEGER NOT NULL DEFAULT 0
    )
)";

    if (!query.exec(createPrivateMessageTable))
    {
        m_lastError = query.lastError().text();
        return false;
    }

    const QString createPrivateMessageIndex = R"(
    CREATE INDEX IF NOT EXISTS idx_private_messages_sender_recipient_time
    ON private_messages(sender, recipient, sent_at DESC)
)";

    if (!query.exec(createPrivateMessageIndex))
    {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

QSqlDatabase DatabaseManager::database() const
{
    return m_database;
}