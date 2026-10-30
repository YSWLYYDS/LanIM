#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "chatserver.h"
#include "databasemanager.h"
#include "userrepository.h"
#include "messagerepository.h"
#include "logging.h"
int main(int argc, char *argv[])
{
    QCoreApplication app(argc,argv);
    app.setApplicationName("LanIMServer");
    Logging::install();
    const QString databasePath=
        QDir(QCoreApplication::applicationDirPath()).filePath("lanim.db");

    DatabaseManager databaseManager(databasePath);
    if(!databaseManager.open()){
        qCritical()<<"Failed to open SQLite database:"
                    <<databaseManager.lastError();
        return 1;
    }

    if(!databaseManager.initializeSchema()){
        qCritical()<<"Failed to initialize database schema:"
                    <<databaseManager.lastError();
        return 1;
    }

    qInfo() << "SQLite database ready:" << databasePath;
    UserRepository userRepository(databaseManager.database());
    MessageRepository messageRepository(databaseManager.database());
    ChatServer server(userRepository, messageRepository);
    constexpr quint16 serverPort=45454;
    if(!server.start(serverPort)){
        return 1;
    }
    return app.exec();
}
