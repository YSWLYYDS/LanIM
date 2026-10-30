#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H
#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    explicit DatabaseManager(const QString &databasePath);

    bool open();
    bool initializeSchema();//创建数据表

    QString lastError() const;
    QSqlDatabase database() const;//让 Repository 获取已经打开的数据库连接
private:
    QString m_databasePath;//SQLite 文件路径
    QString m_lastError;//保存最近一次数据库操作失败原因
    QSqlDatabase m_database;//Qt 对 SQLite 数据库连接的封装
};

#endif // DATABASEMANAGER_H
