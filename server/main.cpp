#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "chatserver.h"
#include "databasemanager.h"
#include "userrepository.h"
#include "messagerepository.h"
#include "logging.h"
#include "ticketrepository.h"
#include <QCommandLineParser>
#include <QFileInfo>
int main(int argc, char *argv[])
{
    QCoreApplication app(argc,argv);
    app.setApplicationName("LanIMServer");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"port","TCP port","port","45454"});
    parser.addOption({"database","SQLite file","path",QDir(QCoreApplication::applicationDirPath()).filePath("lanim.db")});
    parser.addOption({"support","Server-authorized support usernames, comma separated","names"});
    parser.process(app);
    Logging::install();
    const QString databasePath=
        QFileInfo(parser.value("database")).absoluteFilePath();

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
    TicketRepository tickets(databaseManager.database());
    if(!tickets.initialize()){qCritical()<<tickets.lastError();return 1;}
    QStringList supportUsers;
    for(const QString &name:parser.value("support").split(',',Qt::SkipEmptyParts))supportUsers.append(name.trimmed());
    supportUsers.removeAll(QString());supportUsers.removeDuplicates();
    ChatServer server(userRepository, messageRepository,tickets,supportUsers);
    bool portOk=false;const int port=parser.value("port").toInt(&portOk);
    if(!portOk||port<1||port>65535){qCritical()<<"Invalid port";return 1;}
    const quint16 serverPort=static_cast<quint16>(port);
    qInfo()<<"Support accounts (server configured):"<<supportUsers;
    if(!server.start(serverPort)){
        return 1;
    }
    return app.exec();
}
