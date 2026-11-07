#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHostAddress>
#include <QStatusBar>
#include <QJsonObject>
#include <QUuid>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QJsonArray>
#include <QGroupBox>
#include <QDateTime>
#include <QAbstractItemView>
#include <QDockWidget>
#include <QListWidget>
#include <QTextCursor>
#include <QVBoxLayout>
#include "ticketpanel.h"
#include <QSettings>
#include <QInputDialog>
#include <QMenuBar>
#include <QTabWidget>
#include <QStackedWidget>
#include "reportpanel.h"
#include "businessstyle.h"

namespace {
constexpr quint16 ServerPort=45454;
}
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("营销数据报表与问题协同客户端");
    QWidget *chatPage=takeCentralWidget();
    m_loginPage=new QWidget;
    m_loginPage->setObjectName("loginPage");
    ui->titleLabel->setText("账户与沟通");
    auto *loginLayout=new QVBoxLayout(m_loginPage);
    ui->authGroupBox->setParent(m_loginPage);
    ui->authGroupBox->setFixedWidth(390);
    loginLayout->addWidget(ui->authGroupBox,0,Qt::AlignCenter);
    ui->loginButton->setProperty("primary",true);
    connect(ui->passwordEdit,&QLineEdit::returnPressed,ui->loginButton,&QPushButton::click);
    m_businessTabs=new QTabWidget(this);
    m_businessTabs->setObjectName("businessTabs");
    auto *reports = new ReportPanel(m_businessTabs);
    m_businessTabs->addTab(reports,"营销数据报表（HTTP）");
    m_ticketPanel=new TicketPanel(&m_chatClient,m_businessTabs);
    m_businessTabs->addTab(m_ticketPanel,"营销问题工单（TCP）");
    m_businessTabs->addTab(chatPage,"账户与沟通");
    connect(reports, &ReportPanel::issueRequested, this,
            [this](const QString &title, const QString &context, const QString &description) {
      if (m_currentUsername.isEmpty()) {
        showLoginPage();
        statusBar()->showMessage("请先登录，再返回报表提交问题", 8000);
        return;
      }
      m_businessTabs->setCurrentIndex(1);
      m_ticketPanel->createFromReport(title, context, description);
    });
    m_workspacePage=BusinessUi::shell(m_businessTabs,"营销工作台","投放数据分析 · 业务问题协同",{"投放数据报表","业务问题工单","账户与沟通"});
    m_rootPages=new QStackedWidget(this);
    m_rootPages->addWidget(m_loginPage);
    m_rootPages->addWidget(m_workspacePage);
    setCentralWidget(m_rootPages);
    qApp->setStyleSheet(BusinessUi::stylesheet());
    resize(620,460);
    auto *settingsAction=menuBar()->addAction("服务器设置（重启生效）");
    connect(settingsAction,&QAction::triggered,this,[this]{
        QSettings settings;bool ok=false;
        const QString ip=QInputDialog::getText(this,"服务器","IPv4地址",QLineEdit::Normal,settings.value("server/ip","127.0.0.1").toString(),&ok);
        if(!ok)return;QHostAddress address;if(!address.setAddress(ip)){statusBar()->showMessage("IP地址无效");return;}
        int port=QInputDialog::getInt(this,"服务器","端口",settings.value("server/port",45454).toInt(),1,65535,1,&ok);if(!ok)return;
        settings.setValue("server/ip",ip);settings.setValue("server/port",port);statusBar()->showMessage("配置已保存，重启客户端生效");
    });

    m_onlineUsersDock = new QDockWidget("在线用户", this);//创建一个标题为“在线用户”的停靠窗口
    m_onlineUsersDock->setObjectName("onlineUsersDock");

    auto *dockContent = new QWidget(m_onlineUsersDock);

    auto *dockLayout = new QVBoxLayout(dockContent);
    dockLayout->setContentsMargins(4, 4, 4, 4);

    m_loadEarlierButton =
        new QPushButton("加载更早消息", dockContent);

    m_loadEarlierButton->setVisible(false);
    m_loadEarlierButton->setEnabled(false);

    dockLayout->addWidget(m_loadEarlierButton);

    m_onlineUserList = new QListWidget(dockContent);
    m_onlineUserList->setSelectionMode(
        QAbstractItemView::SingleSelection);

    dockLayout->addWidget(m_onlineUserList);

    m_onlineUsersDock->setWidget(dockContent);

    addDockWidget(Qt::RightDockWidgetArea, m_onlineUsersDock);//addDockWidget 让它自动停靠在主窗口右侧；
    m_onlineUsersDock->hide();//初始隐藏，登录成功才显示
    connect(m_businessTabs,&QTabWidget::currentChanged,this,[this](int index){m_onlineUsersDock->setVisible(index==2&&!m_currentUsername.isEmpty());});
    //点击其他用户：进入与该用户的私聊发送模式;再次点击当前私聊对象：回到公共聊天室模式;点击自己：不允许私聊自己。
    connect(m_onlineUserList, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *item) {
                const QString username = item->text();

                if (username == m_currentUsername) {
                    statusBar()->showMessage(
                        "不能选择自己作为私聊对象。");
                    m_onlineUserList->clearSelection();
                    return;
                }

                if (username == m_selectedPrivateUser) {
                    setPrivateChatPeer(QString());
                    m_onlineUserList->clearSelection();
                    return;
                }

                setPrivateChatPeer(username);
            });

    ui->authGroupBox->setEnabled(false);
    ui->userPanel->setVisible(false);
    ui->messageEdit->setEnabled(false);
    ui->sendButton->setEnabled(false);

    connect(ui->registerButton, &QPushButton::clicked,
            this, [this]() {
                sendRegisterRequest();
            });

    connect(ui->loginButton, &QPushButton::clicked,
            this, [this]() {
                sendLoginRequest();
            });


    connect(ui->sendButton, &QPushButton::clicked,
            this, [this]() {
                if (m_selectedPrivateUser.isEmpty()) {
                    sendPublicMessage();
                } else {
                    sendPrivateMessage();
                }
            });


    connect(ui->logoutButton, &QPushButton::clicked,
            this, [this]() {
                sendLogoutRequest();
            });
    connect(ui->messageEdit, &QLineEdit::returnPressed,
            this, [this]() {
                if (m_selectedPrivateUser.isEmpty()) {
                    sendPublicMessage();
                } else {
                    sendPrivateMessage();
                }
            });//输入框回车发送



    connect(&m_chatClient,&ChatClient::connected,this,[this](){
        const QJsonObject helloMessage{
            {"version",1},
            {"type","system.hello"},
            {
                "requestId",
                QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"body",QJsonObject{
            {"clientName","LanIMClient"}
            }}
        };
        if(!m_chatClient.sendMessage(helloMessage)){
            statusBar()->showMessage("已连接,但发送握手消息失败");
            return;
        }
        statusBar()->showMessage("已连接到LanIM服务端,握手消息已发送");
    });

    connect(&m_chatClient,& ChatClient::disconnected,this,[this](){
        m_isHandshakeComplete = false;
        m_isLoggedIn=false;

        m_pendingUsername.clear();
        m_currentUsername.clear();
        m_ticketPanel->setSession({});

        ui->currentUserLabel->setText("当前未登录");
        ui->userPanel->setVisible(false);

        showLoginPage();
        ui->authGroupBox->setVisible(true);
        ui->authGroupBox->setEnabled(false);
        //退出、断线、被互踢时，旧在线名单不能留在界面上。
        m_onlineUsers.clear();
        refreshOnlineUserList();
        m_onlineUsersDock->hide();

        ui->messageEdit->setEnabled(false);
        ui->sendButton->setEnabled(false);

        const QString disconnectMessage =
            m_disconnectReason.isEmpty()
                ? "与 LanIM 服务端的连接已断开"
                : m_disconnectReason;

        m_disconnectReason.clear();
        statusBar()->showMessage(disconnectMessage);
    });

    connect(&m_chatClient,&ChatClient::connectionError,this,[this](const QString &message){
        statusBar()->showMessage("连接失败："+message);
    });
    connect(&m_chatClient,&ChatClient::messageReceived,this,[this](const QJsonObject &message){
        const QString type=message.value("type").toString();
        if(type=="system.welcome")//网络连接和应用层握手完成
        {
            const QJsonObject body =
                message.value("body").toObject();

            const QString welcomeText =
                body.value("message").toString();

            m_isHandshakeComplete = true;
            ui->authGroupBox->setEnabled(true);

            statusBar()->showMessage(
                "握手完成：" + welcomeText
                + "。请输入账号后注册或登录。");
        }
        else if (type == "auth.register.result")//显示注册结果，由用户自行点击登录。
        {
            const QJsonObject body =
                message.value("body").toObject();

            const bool success =
                body.value("success").toBool();
            const QString resultText =
                body.value("message").toString();

            if (success) {
                statusBar()->showMessage(
                    "注册成功，请点击登录。");
            } else {
                statusBar()->showMessage(
                    "注册失败：" + resultText);
            }
        }
        else if(type=="auth.login.result")//登录成功表示服务器已把当前 TCP 连接标记为该用户；随后客户端可以发送公共聊天消息。失败则显示“用户名或密码错误”等提示。
        {
            const QJsonObject body =message.value("body").toObject();
            const bool success=body.value("success").toBool();
            const QString resultText=body.value("message").toString();
            if (success) {
                m_isLoggedIn = true;
                m_currentUsername = m_pendingUsername;
                m_ticketPanel->setSession(m_currentUsername);
                m_pendingUsername.clear();

                ui->passwordEdit->clear();
                ui->authGroupBox->setVisible(false);

                ui->currentUserLabel->setText(
                    "当前用户：" + m_currentUsername);
                ui->userPanel->setVisible(true);
                showWorkspace();
                m_onlineUsersDock->setVisible(m_businessTabs->currentIndex()==2);
                ui->logoutButton->setEnabled(true);

                ui->messageEdit->setEnabled(true);
                ui->sendButton->setEnabled(true);
                ui->messageEdit->setFocus();//光标直接进入输入框，用户可以马上输入

                statusBar()->showMessage("登录成功，已进入营销工作台。");
            } else {
                m_pendingUsername.clear();
                statusBar()->showMessage("登录失败：" + resultText);
            }
        }
        else if(type=="chat.public.message")//接收公共消息分支
        {
            const QJsonObject body=message.value("body").toObject();
            const QString sender = body.value("sender").toString();
            const QString text = body.value("text").toString();
            const QString sentAt = body.value("sentAt").toString();

            appendPublicMessage(sender, text, sentAt);
        }
        else if(type == "chat.public.history")//客户端显示历史消息
        {
            const QJsonObject body=message.value("body").toObject();
            const QJsonArray messages=body.value("messages").toArray();

            for(const QJsonValue &value :messages){
                const QJsonObject historyItem=value.toObject();

                const QString sender=
                    historyItem.value("sender").toString();
                const QString text=
                    historyItem.value("text").toString();

                ui->chatView->append(
                    QString("%1: %2")
                    .arg(sender.toHtmlEscaped(),
                             text.toHtmlEscaped()));
            }
        }
        else if (type == "auth.logout.result")
        {
            const QJsonObject body =
                message.value("body").toObject();

            const bool success = body.value("success").toBool();
            const QString resultText =
                body.value("message").toString();

            if (!success) {
                ui->logoutButton->setEnabled(true);
                statusBar()->showMessage("退出登录失败：" + resultText);
                return;
            }
            //清空本地登录状态
            m_isLoggedIn = false;
            m_pendingUsername.clear();
            m_currentUsername.clear();
            m_ticketPanel->setSession({});

            //退出、断线、被互踢时，旧在线名单不能留在界面上
            m_onlineUsers.clear();
            refreshOnlineUserList();
            m_onlineUsersDock->hide();

            ui->chatView->clear();
            ui->messageEdit->clear();
            ui->messageEdit->setEnabled(false);
            ui->sendButton->setEnabled(false);

            ui->currentUserLabel->setText("当前未登录");
            ui->userPanel->setVisible(false);

            showLoginPage();
            ui->authGroupBox->setVisible(true);
            ui->authGroupBox->setEnabled(true);
            ui->usernameEdit->setFocus();

            statusBar()->showMessage("退出登录成功，请重新登录。");
        }
        else if (type == "auth.kicked")
        {
            const QJsonObject body =
                message.value("body").toObject();

            m_disconnectReason =
                body.value("message").toString();

            if (m_disconnectReason.isEmpty()) {
                m_disconnectReason =
                    "该账号已在其他客户端登录，当前连接已断开。";
            }
        }
        else if (type == "presence.online.list")//登录后一次性同步完整名单
        {
            const QJsonObject body =
                message.value("body").toObject();

            const QJsonArray users =
                body.value("users").toArray();

            QStringList usernames;

            for (const QJsonValue &value : users) {
                const QString username = value.toString();

                if (!username.isEmpty()) {
                    usernames.append(username);
                }
            }

            setOnlineUsers(usernames);
        }
        else if (type == "presence.user.online")//某用户新上线，追加到列表
        {
            const QString username =
                message.value("body")
                    .toObject()
                    .value("username")
                    .toString();

            if (!username.isEmpty()
                && !m_onlineUsers.contains(username)) {
                m_onlineUsers.append(username);
                m_onlineUsers.sort();
                refreshOnlineUserList();
            }
        }
        else if (type == "presence.user.offline")//某用户下线，从列表移除
        {
            const QString username =
                message.value("body")
                    .toObject()
                    .value("username")
                    .toString();

            if (!username.isEmpty())
            {
                m_onlineUsers.removeAll(username);
                refreshOnlineUserList();

                if (username == m_selectedPrivateUser)
                {
                    setPrivateChatPeer(QString());
                    m_onlineUserList->clearSelection();

                    statusBar()->showMessage(
                        "对方已下线，已切换到公共聊天室。");
                }
            }
        }
        else if (type == "chat.private.message")
        {
            const QJsonObject body =
                message.value("body").toObject();

            appendPrivateMessage(
                body.value("sender").toString(),
                body.value("recipient").toString(),
                body.value("text").toString(),
                body.value("sentAt").toString());
        }
        else if (type == "chat.private.send.result")
        {
            const QJsonObject body =
                message.value("body").toObject();

            const bool success =
                body.value("success").toBool();

            if (!success) {
                statusBar()->showMessage(
                    "私聊发送失败："
                    + body.value("message").toString());
            }
        }
        else if (type == "chat.private.history")
        {
            const QJsonObject body =
                message.value("body").toObject();

            const QString peer =
                body.value("peer").toString();

            const int offset =
                body.value("offset").toInt();

            if (peer != m_selectedPrivateUser) {
                return;
            }

            const QJsonArray messages =
                body.value("messages").toArray();

            if (offset == 0) {
                ui->chatView->clear();
            }

            if (offset == 0) {
                for (const QJsonValue &value : messages) {
                    const QJsonObject item = value.toObject();

                    appendPrivateMessage(
                        item.value("sender").toString(),
                        item.value("recipient").toString(),
                        item.value("text").toString(),
                        item.value("sentAt").toString());
                }
            } else {
                QString earlierText;

                for (const QJsonValue &value : messages) {
                    const QJsonObject item = value.toObject();

                    const QString sender =
                        item.value("sender").toString();

                    const QString recipient =
                        item.value("recipient").toString();

                    const QString text =
                        item.value("text").toString();

                    const QString sentAt =
                        item.value("sentAt").toString();

                    const QDateTime utcTime =
                        QDateTime::fromString(
                            sentAt, Qt::ISODateWithMs);

                    QString timeText;

                    if (utcTime.isValid()) {
                        timeText = utcTime.toLocalTime()
                        .toString("HH:mm:ss");
                    }

                    const QString direction =
                        sender == m_currentUsername
                            ? "你 → " + recipient
                            : sender + " → 你";

                    QString prefix =
                        "[私聊] " + direction;

                    if (!timeText.isEmpty()) {
                        prefix = "[" + timeText + "] " + prefix;
                    }

                    earlierText +=
                        prefix + "：" + text + "\n";
                }

                QTextCursor cursor =
                    ui->chatView->textCursor();

                cursor.movePosition(QTextCursor::Start);
                cursor.insertText(earlierText);
            }

            m_privateHistoryOffset += messages.size();

            m_hasMorePrivateHistory =
                body.value("hasMore").toBool();

            m_loadEarlierButton->setEnabled(
                m_hasMorePrivateHistory);

            statusBar()->showMessage(
                "已加载与 " + peer + " 的 "
                + QString::number(m_privateHistoryOffset)
                + " 条私聊记录。");
        }

    });

    connect(&m_chatClient,&ChatClient::protocolError,
            this,[this](const QString &message){
        statusBar()->showMessage("协议错误："+message);
    });
    connect(m_loadEarlierButton, &QPushButton::clicked,
            this, [this]()//连接加载按钮
            {
                if (!m_hasMorePrivateHistory
                    || m_selectedPrivateUser.isEmpty()) {
                    return;
                }

                requestPrivateHistory(m_privateHistoryOffset);
            });

    statusBar()->showMessage("正在连接 LanIM 服务端...");
    QSettings settings;
    const bool smoke=QCoreApplication::arguments().contains("--smoke-test");
    const QString host=smoke?"127.0.0.1":settings.value("server/ip","127.0.0.1").toString();
    const quint16 port=smoke&&qEnvironmentVariableIsSet("BUSINESS_SMOKE_PORT")?static_cast<quint16>(qEnvironmentVariableIntValue("BUSINESS_SMOKE_PORT")):static_cast<quint16>(settings.value("server/port",ServerPort).toUInt());
    m_chatClient.connectToServer(QHostAddress(host),port);
}

void MainWindow::sendPublicMessage()
{
    if(!m_isLoggedIn){
        statusBar()->showMessage("请等待登录完成。");
        return;
    }

    const QString text=ui->messageEdit->text().trimmed();

    if(text.isEmpty()){
        return;
    }

    const QJsonObject message{
        {"version",1},
        {"type","chat.public.send"},
        {"requestId",
             QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"body",QJsonObject{
             {"text",text}
        }}

        };

    if(!m_chatClient.sendMessage(message)){

        statusBar()->showMessage("发送消息失败。");
        return;
    }

    ui->messageEdit->clear();
}

void MainWindow::sendLogoutRequest()
{
    if (!m_isLoggedIn) {
        statusBar()->showMessage("当前尚未登录。");
        return;
    }

    const QJsonObject message{
        {"version", 1},
        {"type", "auth.logout"},
        {"requestId",
         QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"body", QJsonObject{}}
    };

    if (!m_chatClient.sendMessage(message)) {
        statusBar()->showMessage("退出登录请求发送失败。");
        return;
    }

    ui->logoutButton->setEnabled(false);
    statusBar()->showMessage("正在退出登录...");
}

void MainWindow::sendPrivateMessage()
{
    if (!m_isLoggedIn) {
        statusBar()->showMessage("请等待登录完成。");
        return;
    }

    const QString text =
        ui->messageEdit->text().trimmed();

    if (text.isEmpty()) {
        return;
    }

    const QJsonObject message{
        {"version", 1},
        {"type", "chat.private.send"},
        {"requestId",
         QUuid::createUuid().toString(
             QUuid::WithoutBraces)},
        {"body", QJsonObject{
                     {"recipient", m_selectedPrivateUser},
                     {"text", text}
                 }}
    };

    if (!m_chatClient.sendMessage(message)) {
        statusBar()->showMessage("私聊消息发送失败。");
        return;
    }

    ui->messageEdit->clear();

    statusBar()->showMessage(
        "正在向 " + m_selectedPrivateUser + " 发送私聊...");
}

void MainWindow::setPrivateChatPeer(const QString &username)
{
    m_selectedPrivateUser = username;
    m_privateHistoryOffset = 0;

    //换一个私聊对象时，旧对象的分页状态不能继续沿用
    m_hasMorePrivateHistory = false;
    m_loadEarlierButton->setVisible(
        !m_selectedPrivateUser.isEmpty());
    m_loadEarlierButton->setEnabled(false);


    if (m_selectedPrivateUser.isEmpty()) {
        statusBar()->showMessage(
            "已切换到公共聊天室。");
        return;
    }

    statusBar()->showMessage(
        "当前私聊对象：" + m_selectedPrivateUser
        + "。再次点击该用户可回到公共聊天室。");
    requestPrivateHistory(0);
}

void MainWindow::requestPrivateHistory(int offset)
{
    if (!m_isLoggedIn
        || m_selectedPrivateUser.isEmpty()) {
        return;
    }

    const QJsonObject request{
        {"version", 1},
        {"type", "chat.private.history.request"},
        {"requestId",
         QUuid::createUuid().toString(
             QUuid::WithoutBraces)},
        {"body", QJsonObject{
                     {"peer", m_selectedPrivateUser},
                     {"limit", 30},
                     {"offset", offset}
                 }}
    };

    if (!m_chatClient.sendMessage(request)) {
        statusBar()->showMessage(
            "私聊历史请求发送失败。");
        return;
    }

    statusBar()->showMessage(
        "正在加载与 " + m_selectedPrivateUser
        + " 的私聊记录...");
}

void MainWindow::appendPrivateMessage(const QString &sender, const QString &recipient, const QString &text, const QString &sentAt)
{
    const QDateTime utcTime =
        QDateTime::fromString(
            sentAt, Qt::ISODateWithMs);

    QString timeText;

    if (utcTime.isValid()) {
        timeText =
            utcTime.toLocalTime().toString("HH:mm:ss");
    }

    const QString direction =
        sender == m_currentUsername
            ? "你 → " + recipient
            : sender + " → 你";

    QString prefix = "[私聊] " + direction;

    if (!timeText.isEmpty()) {
        prefix = "[" + timeText + "] " + prefix;
    }

    ui->chatView->append(
        QString("%1：%2")
            .arg(prefix.toHtmlEscaped(),
                 text.toHtmlEscaped()));
}

void MainWindow::appendPublicMessage(const QString &sender, const QString &text, const QString &sentAt)
{
    const QDateTime utcTime =
        QDateTime::fromString(sentAt, Qt::ISODateWithMs);//把服务器传来的字符串时间转换成 Qt 的 QDateTime 对象

    QString timeText;
    if (utcTime.isValid()) {
        timeText = utcTime.toLocalTime().toString("HH:mm:ss");
    }//服务器使用 UTC 时间，客户端显示时转成本机时间。

    QString prefix = sender;
    if (!timeText.isEmpty()) {
        prefix = "[" + timeText + "] " + sender;
    }

    ui->chatView->append(
        QString("%1：%2")
            .arg(prefix.toHtmlEscaped(),
                 text.toHtmlEscaped()));
}

void MainWindow::setOnlineUsers(const QStringList &users)
{
    m_onlineUsers = users;
    m_onlineUsers.sort();

    refreshOnlineUserList();
}

void MainWindow::refreshOnlineUserList()
{
    m_onlineUserList->clear();
    m_onlineUserList->addItems(m_onlineUsers);
}

void MainWindow::showLoginPage()
{
    if (!m_rootPages || !m_loginPage)
        return;
    m_rootPages->setCurrentWidget(m_loginPage);
    m_onlineUsersDock->hide();
    resize(620, 460);
}

void MainWindow::showWorkspace()
{
    if (!m_rootPages || !m_loginPage)
        return;
    m_rootPages->setCurrentWidget(m_workspacePage);
    m_businessTabs->setCurrentIndex(0);
    resize(1440, 960);
}

MainWindow::~MainWindow()
{
    m_chatClient.disconnectFromServer();
    delete ui;
}

void MainWindow::sendRegisterRequest()
{
    if(!m_isHandshakeComplete){
        statusBar()->showMessage("请等待服务器握手完成。");
        return;
    }

    const QString username=ui->usernameEdit->text().trimmed();
    const QString password=ui->passwordEdit->text();

    if(username.isEmpty() || password.isEmpty()){
        statusBar()->showMessage("用户名和密码不能为空。");
        return;
    }

    const QJsonObject message{
        {"version",1},
        {"type","auth.register"},
        {
            "requestId",
            QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"body", QJsonObject{
                {"username", username},
                {"password", password}
            }}
    };

    if(!m_chatClient.sendMessage(message)){
        statusBar()->showMessage("注册请求发送失败。");
        return;
    }
    statusBar()->showMessage("正在注册："+username);
}

void MainWindow::sendLoginRequest()
{
    if(!m_isHandshakeComplete){
        statusBar()->showMessage("请等待服务器握手完成。");
        return;
    }

    const QString username=ui->usernameEdit->text().trimmed();
    const QString password=ui->passwordEdit->text();

    if(username.isEmpty() || password.isEmpty()){
        statusBar()->showMessage("用户名和密码不能为空。");
        return;
    }

    const QJsonObject message{
        {"version",1},
        {"type","auth.login"},
        {
         "requestId",
         QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"body", QJsonObject{
                     {"username", username},
                     {"password", password}
                 }}
    };

    if(!m_chatClient.sendMessage(message)){
        statusBar()->showMessage("登录请求发送失败。");
        return;
    }
    m_pendingUsername = username;
    statusBar()->showMessage("正在登录："+username);
}


