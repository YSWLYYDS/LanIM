#include "userrepository.h"

#include <QSqlError>
#include <QSqlQuery>
UserRepository::UserRepository(const QSqlDatabase &database)
    : m_database(database)
{
}

bool UserRepository::usernameExists(const QString &username, bool &exists)
{
    m_lastError.clear();
    exists = false;

    QSqlQuery query(m_database);

    query.prepare(
        "SELECT 1 FROM users "
        "WHERE username = :username "
        "LIMIT 1");

    query.bindValue(":username", username);

    if (!query.exec()) {
        m_lastError = query.lastError().text();//取得数据库错误文字并保存到 m_lastError
        return false;
    }

    exists = query.next();
    return true;
}

bool UserRepository::findPasswordHash(const QString &username, QString &passwordHash, bool &found)
{
    m_lastError.clear();
    passwordHash.clear();
    found=false;

    QSqlQuery query(m_database);

    query.prepare(
        "SELECT password_hash FROM users "
        "WHERE username = :username "
        "LIMIT 1"
        );
    query.bindValue(":username",username);//将用户输入作为 SQL 参数绑定，而不是手动拼接 SQL 字符串

    if(!query.exec()){
        m_lastError=query.lastError().text();
        return false;
    }

    if(!query.next()){
        return true;
    }

    passwordHash=query.value(0).toString();
    found=true;
    return true;
}

bool UserRepository::createUser(const QString &username, const QString &passwordHash)
{
    m_lastError.clear();
    QSqlQuery query(m_database);
    query.prepare(
        "INSERT INTO users (username,password_hash) "
        "VALUES (:username,:password_hash)");

    query.bindValue(":username",username);
    query.bindValue(":password_hash",passwordHash);

    if(!query.exec()){
        m_lastError=query.lastError().text();
        return false;
    }
    return true;
}

QString UserRepository::lastError() const
{
    return m_lastError;
}

