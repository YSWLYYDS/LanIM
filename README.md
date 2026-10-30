# LanIM

LanIM 是一个基于 Qt 6、C++17、qmake 开发的局域网即时通信系统。

项目采用客户端 / 服务端架构，使用 TCP 长连接、自定义长度头 JSON 协议和 SQLite 持久化，支持公共聊天室、在线状态、账号系统、实时私聊、消息历史、心跳保活与自动重连。

## 技术栈

- Qt 6
- C++17
- qmake
- Qt Widgets
- Qt Network
- Qt SQL
- SQLite
- TCP Socket
- JSON

## 功能列表

- 用户注册、登录、退出登录。
- 同账号重复登录互踢。
- TCP 长连接与应用层握手。
- 自定义 4 字节大端长度头协议。
- TCP 半包、粘包拆包处理。
- 公共聊天室实时广播。
- 公共消息 SQLite 持久化与历史加载。
- 在线用户列表、用户上线和下线同步。
- 点击在线用户进行实时私聊。
- 私聊 SQLite 持久化。
- 私聊最近消息加载与简化分页。
- 客户端定时心跳 ping/pong。
- 服务端无活动连接超时检测。
- 客户端断线后每 3 秒自动重连。
- 文件日志、时间戳、日志级别与密码脱敏。

## 项目结构

```text
LanIM/
├─ common/                 # 客户端、服务端共享代码
│  ├─ protocol.h/.cpp      # 长度头 JSON 协议
│  ├─ logging.h/.cpp       # 文件日志模块
│  └─ common.pri
├─ server/                 # 服务端
│  ├─ chatserver.h/.cpp    # 网络监听、业务消息路由
│  ├─ sessionmanager.h/.cpp# 在线会话管理
│  ├─ databasemanager.h/.cpp
│  ├─ userrepository.h/.cpp
│  └─ messagerepository.h/.cpp
├─ client/                 # Qt Widgets 客户端
│  ├─ mainwindow.h/.cpp/.ui
│  └─ chatclient.h/.cpp
├─ docs/                   # 协议等文档
└─ LanIM.pro
```

## 架构

```text
Qt Widgets Client
        │
        │ TCP + 4 字节长度头 + JSON
        ▼
ChatServer
        │
        ├─ SessionManager
        │   ├─ socket → username
        │   └─ username → socket
        │
        ├─ UserRepository
        └─ MessageRepository
                │
                ▼
             SQLite
```

## 通信协议

每一帧网络数据格式：

```text
[4 字节 payload 长度，大端序][JSON payload]
```

常用消息类型：

```text
system.hello
system.welcome
system.ping
system.pong

auth.register
auth.register.result
auth.login
auth.login.result
auth.logout
auth.logout.result
auth.kicked

presence.online.list
presence.user.online
presence.user.offline

chat.public.send
chat.public.message
chat.public.history

chat.private.send
chat.private.send.result
chat.private.message
chat.private.history.request
chat.private.history
```

## 运行方式

1. 使用 Qt Creator 打开根目录 `LanIM.pro`。
2. 选择 Qt 6 MinGW Kit。
3. 先运行 `LanIMServer`。
4. 再运行一个或多个 `LanIMClient`。
5. 注册不同账号并登录。
6. 点击右侧在线用户可进入私聊模式。
7. 再次点击当前私聊用户可返回公共聊天室。

## 数据库

服务端启动后会自动在可执行文件目录创建：

```text
lanim.db
```

主要数据表：

```text
users
public_messages
private_messages
```

## 日志

客户端和服务端都会生成按日期划分的日志文件。

日志包含：

- 启动与监听状态。
- 客户端连接、断开、登录、退出。
- 心跳和协议消息。
- 数据库异常。
- 网络与协议错误。

账号密码会自动脱敏：

```text
"password":"***"
```

## 测试场景

- 注册新账号。
- 登录成功与密码错误。
- 两个客户端公共聊天室互发消息。
- 同账号在第二个客户端登录，旧客户端被互踢。
- 用户上线、退出、断线时在线列表同步。
- 选择在线用户发送私聊。
- 重启客户端后加载私聊历史。
- 私聊消息超过 30 条时加载更早消息。
- 停止服务端后，客户端自动重连。
- 检查日志文件中不存在明文密码。

## 当前范围

本项目聚焦局域网文本即时通信。
