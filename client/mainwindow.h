#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include "chatclient.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QDockWidget;
class QListWidget;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    ChatClient m_chatClient;
    void sendRegisterRequest();//读取输入框，发送 auth.register
    void sendLoginRequest();//读取输入框，发送 auth.login
    void sendPublicMessage();//集中处理“从输入框取文本、打包、发送、清空输入框”的逻辑
    void sendLogoutRequest();//退出请求
    void sendPrivateMessage();//组装并发送 chat.private.send
    void setPrivateChatPeer(const QString &username);//切换当前私聊对象
    void requestPrivateHistory(int offset);
    void appendPrivateMessage(const QString &sender,
                              const QString &recipient,
                              const QString &text,
                              const QString &sentAt);//把收到的私聊显示到消息框
    void appendPublicMessage(const QString &sender,
                             const QString &text,
                             const QString &sentAt);//把“显示实时消息”和“显示历史消息”的重复代码合并
    void setOnlineUsers(const QStringList &users);//收到服务端完整名单时，替换本地在线用户名单
    void refreshOnlineUserList();//把内存中的名单刷新到右侧 UI 列表。


    QString m_pendingUsername;//保存“正在请求登录”的用户名
    QString m_currentUsername;//保存已经被服务器确认登录成功的用户名。

    bool m_isHandshakeComplete = false;//只有收到服务器欢迎消息后，才允许用户点击注册或登录。
    bool m_isLoggedIn = false;//记录当前客户端是否已登录

    QString m_disconnectReason;//暂存断开原因，等 disconnected 信号到来时再显示出来。

    QDockWidget *m_onlineUsersDock = nullptr;//右侧可停靠面板。
    QListWidget *m_onlineUserList = nullptr;//面板中的列表控件。
    QStringList m_onlineUsers;//客户端当前保存的在线用户数据

    QString m_selectedPrivateUser;//为空表示公共聊天室；有值表示当前发送内容应发给这个用户名
    int m_privateHistoryOffset = 0;//记录已加载多少条私聊消息
    bool m_hasMorePrivateHistory = false;//服务端告诉客户端是否还有更早消息
    QPushButton *m_loadEarlierButton = nullptr;//点击后请求下一页
};
#endif // MAINWINDOW_H
