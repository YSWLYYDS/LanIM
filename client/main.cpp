#include "mainwindow.h"

#include <QApplication>
#include "logging.h"
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableView>
#include <QTabWidget>
#include <QFontDatabase>
#include <QDir>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR","C:/Windows")).filePath("Fonts/msyh.ttc"));
    a.setFont(QFont("Microsoft YaHei",10));
    a.setApplicationName("LanIMClient");
    Logging::install();
    MainWindow w;
    w.show();
    if(a.arguments().contains("--smoke-test")){
        QTimer::singleShot(400,&w,[&]{
            auto *login=w.findChild<QPushButton*>("loginButton");
            auto *workspace=w.findChild<QTabWidget*>("businessTabs");
            if(!login || !login->isVisible() || !workspace || workspace->isVisible()){
                a.exit(3);
                return;
            }
            if(qEnvironmentVariableIsSet("BUSINESS_LOGIN_SCREENSHOT"))
                w.grab().save(qEnvironmentVariable("BUSINESS_LOGIN_SCREENSHOT"));
            if(qEnvironmentVariableIsSet("BUSINESS_SMOKE_USER")){
                w.findChild<QLineEdit*>("usernameEdit")->setText(qEnvironmentVariable("BUSINESS_SMOKE_USER"));
                w.findChild<QLineEdit*>("passwordEdit")->setText(qEnvironmentVariable("BUSINESS_SMOKE_PASSWORD"));
                w.findChild<QPushButton*>("loginButton")->click();
            }
        });
        QTimer::singleShot(1300,&w,[&]{auto *table=w.findChild<QTableWidget*>("ticketTable");if(table&&table->rowCount()>0)table->selectRow(0);});
        QTimer::singleShot(1900,&w,[&]{
            if(qEnvironmentVariableIsSet("BUSINESS_TICKET_SCREENSHOT")){
                auto *tabs=w.findChild<QTabWidget*>("businessTabs");tabs->setCurrentIndex(1);
                w.grab().save(qEnvironmentVariable("BUSINESS_TICKET_SCREENSHOT"));tabs->setCurrentIndex(0);
            }
        });
        QTimer::singleShot(2500,&w,[&]{
            const QString path=qEnvironmentVariable("BUSINESS_SCREENSHOT");
            if(!path.isEmpty())w.grab().save(path);
            auto *report=w.findChild<QTableView*>("marketingReportTable");
            if(qEnvironmentVariableIsSet("BUSINESS_SMOKE_USER")&&(!report||!report->isVisible())){
                a.exit(4);
                return;
            }
            if(qEnvironmentVariableIsSet("BUSINESS_REPORT_URL")&&(!report||report->model()->rowCount()==0))a.exit(2);
            else a.quit();
        });
    }
    return QCoreApplication::exec();
}
