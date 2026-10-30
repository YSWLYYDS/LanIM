#ifndef USERREPOSITORY_H
#define USERREPOSITORY_H

#include <QSqlDatabase>
#include <QString>

class UserRepository
{
public:
    explicit UserRepository(const QSqlDatabase &database);

    bool usernameExists(const QString &username, bool &exists);//查询用户名是否存在
    bool findPasswordHash(const QString &username,QString &passwordHash,bool &found);//“查询用户密码哈希
    bool createUser(const QString &username,
                    const QString &passwordHash);

    QString lastError() const;

private:
    QSqlDatabase m_database;
    QString m_lastError;
};

#endif // USERREPOSITORY_H