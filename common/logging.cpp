#include "logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>
#include <cstdlib>

namespace
{
QFile *g_logFile = nullptr;
QMutex g_logMutex;
thread_local bool g_isHandlingLog = false;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }

    return "UNKNOWN";
}

void messageHandler(
    QtMsgType type,
    const QMessageLogContext &,
    const QString &message)
{
    if (g_isHandlingLog) {
        return;
    }

    g_isHandlingLog = true;

    const QString time =
        QDateTime::currentDateTime()
            .toString("yyyy-MM-dd HH:mm:ss.zzz");

    const QString line =
        QString("[%1] [%2] %3")
            .arg(time, levelName(type), message);

    const QByteArray output =
        (line + '\n').toUtf8();

    {
        QMutexLocker locker(&g_logMutex);

        if (g_logFile != nullptr
            && g_logFile->isOpen()) {
            g_logFile->write(output);
            g_logFile->flush();
        }
    }

    std::fputs(output.constData(), stderr);
    std::fflush(stderr);

    g_isHandlingLog = false;

    if (type == QtFatalMsg) {
        std::abort();
    }
}
}

void Logging::install()
{
    const QString logDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation)
        + "/logs";

    QDir().mkpath(logDirectory);

    const QString logPath =
        logDirectory + "/"
        + QDate::currentDate()
              .toString("yyyy-MM-dd")
        + ".log";

    g_logFile = new QFile(logPath);

    if (!g_logFile->open(
            QIODevice::WriteOnly
            | QIODevice::Append
            | QIODevice::Text)) {
        delete g_logFile;
        g_logFile = nullptr;
    }

    qInstallMessageHandler(messageHandler);

    qInfo() << "Logging installed:" << logPath;
}