# 营销数据运营与协同客户端（LanIM）

基于 **Qt 6 / C++17** 开发的客户端—服务端桌面系统。项目将营销数据报表、问题工单与即时通信整合到同一工作台：客户端通过 HTTP 查询或导入营销报表，通过 TCP 长连接完成账号认证、在线协作、聊天和工单流转；服务端使用 SQLite 持久化业务数据。

## 项目亮点

- 同时覆盖 HTTP 报表查询与 TCP 实时协作两类典型客户端通信场景。
- 自定义“4 字节大端长度头 + JSON”应用层协议，正确处理 TCP 半包与粘包。
- 实现心跳保活、超时检测、自动重连、重复登录互踢和在线状态同步。
- 报表请求支持超时、取消、有限重试、响应大小限制、离线 JSON 导入及后台解析。
- 使用 `QAbstractTableModel` 承载报表数据，支持排序、分页、指标汇总与可视化分析。
- 不依赖第三方 Excel 库，直接生成符合 OOXML/ZIP 结构的 `.xlsx` 文件。
- 工单流转由服务端校验身份和状态，支持认领、解决、关闭、退回及历史留痕。
- 账号、聊天记录、工单、留言和事件全部持久化到 SQLite。

## 系统架构

```text
                  HTTP / JSON
营销报表接口  ───────────────────────┐
                                    ▼
                            Qt Widgets Client
                                    │
                   TCP + 4-byte length + JSON
                                    │
                                    ▼
                               ChatServer
                         ┌──────────┼──────────┐
                         ▼          ▼          ▼
                   SessionManager  Repository  TicketRepository
                                    │          │
                                    └────┬─────┘
                                         ▼
                                       SQLite
```

## 功能模块

### 营销数据报表（HTTP）

- 按日期、渠道和投放项目查询报表。
- 展示曝光量、点击量、消耗金额、转化数和点击率。
- 支持列排序、前后分页和请求记录查看。
- 展示曝光、点击、消耗、转化等汇总指标。
- 绘制每日消耗趋势及渠道消耗对比。
- 支持自动刷新、手动取消请求和错误重试。
- 限制单次响应为 5 MiB、单页为 5000 条，避免异常数据拖垮客户端。
- 支持拖放或选择离线 JSON 报表，并在后台完成读取、校验与解析。
- 将当前页导出为 `.xlsx`。
- 可从选中的报表记录直接创建问题工单并自动带入上下文。
- 默认本地接口不可用时，可自动加载内置演示数据，便于独立展示客户端。

### 营销问题工单（TCP）

- 普通用户可创建业务问题并查看自己提交的工单。
- 服务端通过 `--support` 参数配置支持人员，权限不由客户端自行声明。
- 支持人员可认领待处理工单并提交解决说明。
- 创建者可确认关闭，或将已解决工单退回重新处理。
- 支持工单留言、状态筛选、列表分页和详情历史分页。
- 保存工单事件与留言，用户离线后再次登录仍可查看。
- 使用版本号进行状态更新校验，避免并发操作覆盖新状态。
- 工单变化通过 TCP 实时通知有访问权限的在线用户。

工单状态流转：

```text
待处理 ──支持人员认领──> 处理中 ──提交解决说明──> 已解决
  ▲                                              │     │
  └──────────────创建者退回重处理─────────────────┘     └──创建者确认──> 已关闭
```

### 即时通信与账号系统

- 用户注册、登录和退出。
- 同账号重复登录时使旧会话下线。
- 公共聊天室、实时私聊与在线用户列表。
- 公共消息和私聊消息持久化及历史加载。
- 客户端每 10 秒发送心跳，30 秒未收到响应则主动重连。
- 断线后每 3 秒自动尝试重新连接。
- 客户端和服务端记录文件日志，并对密码字段脱敏。

## 技术栈

- C++17
- Qt 6：Widgets、Network、SQL、Concurrent、Core、Gui
- SQLite
- TCP Socket、HTTP、JSON
- `QAbstractTableModel`
- OOXML / ZIP 文件结构
- qmake

## 通信协议

TCP 每帧由固定长度头和 JSON 载荷组成：

```text
[4 字节 payload 长度（大端序）][UTF-8 JSON payload]
```

统一消息结构：

```json
{
  "version": 1,
  "type": "消息类型",
  "requestId": "请求唯一标识",
  "body": {}
}
```

协议限制单帧最大 1 MiB。客户端和服务端都维护接收缓冲区，循环解码完整帧；数据不足时等待下一次读取，非法长度或非法 JSON 会触发断开。

主要消息类型包括：

```text
system.hello / system.welcome / system.ping / system.pong
auth.register / auth.login / auth.logout / auth.kicked
presence.online.list / presence.user.online / presence.user.offline
chat.public.send / chat.public.message / chat.public.history
chat.private.send / chat.private.message / chat.private.history
ticket.list / ticket.create / ticket.detail / ticket.message
ticket.claim / ticket.resolve / ticket.close / ticket.reopen
ticket.result / ticket.changed
```

更多说明见 `docs/protocol.md`。

## 报表接口格式

客户端默认请求：

```text
GET http://127.0.0.1:45455/api/reports
```

可携带日期、渠道、投放项目、`offset` 和 `limit` 查询参数，也可配置 Bearer Token。响应格式示例：

```json
{
  "total": 1,
  "items": [
    {
      "date": "2026-11-01",
      "campaign": "双十一会员拉新",
      "channel": "信息流",
      "impressions": 120000,
      "clicks": 3600,
      "spend": 8250.50,
      "conversions": 286
    }
  ]
}
```

离线 JSON 使用相同结构，且 `total` 必须与 `items` 数量一致。

## 数据库

服务端默认在可执行文件目录创建 `lanim.db`，主要数据表包括：

| 表 | 用途 |
| --- | --- |
| `users` | 用户账号 |
| `public_messages` | 公共聊天记录 |
| `private_messages` | 私聊记录 |
| `tickets` | 工单主体、状态、处理人与版本号 |
| `ticket_messages` | 工单留言 |
| `ticket_events` | 工单状态变更记录 |

可通过服务端参数指定数据库文件：

```powershell
LanIMServer.exe --database D:\data\lanim.db
```

## 构建环境

- Qt 6.x Desktop Kit
- 支持 C++17 的 MinGW 或 MSVC 编译器
- Qt Creator，或可用的 `qmake` 命令行环境

## 构建

### Qt Creator

1. 使用 Qt Creator 打开根目录 `LanIM.pro`。
2. 选择 Qt 6 Desktop Kit。
3. 构建项目，得到 `LanIMServer` 和 `LanIMClient`。

### 命令行（MinGW）

```powershell
qmake LanIM.pro
mingw32-make -j
```

## 运行

先启动服务端：

```powershell
LanIMServer.exe --port 45454 --database lanim.db --support support01
```

再启动一个或多个客户端：

```powershell
LanIMClient.exe
```

建议准备一个普通账号和一个通过 `--support` 配置的支持账号，以完整演示工单创建、认领、解决、退回和关闭流程。报表服务不是登录和工单功能的前置条件；默认本地报表接口未启动时，客户端会使用内置演示数据。

## 项目结构

```text
LanIM/
├─ LanIM.pro                    # 客户端/服务端子项目入口
├─ common/
│  ├─ protocol.*                # 长度头 JSON 协议编解码
│  ├─ logging.*                 # 文件日志与敏感字段脱敏
│  └─ common.pri
├─ server/
│  ├─ chatserver.*              # 连接管理、认证、聊天及消息路由
│  ├─ sessionmanager.*          # socket 与用户名双向会话映射
│  ├─ databasemanager.*         # SQLite 连接与基础表初始化
│  ├─ userrepository.*          # 用户仓储
│  ├─ messagerepository.*       # 公共/私聊消息仓储
│  └─ ticketrepository.*        # 工单、留言、事件与状态机
├─ client/
│  ├─ chatclient.*              # TCP 客户端、拆包、心跳和重连
│  ├─ mainwindow.*              # 登录、工作台和聊天界面
│  ├─ reportpanel.*             # HTTP 查询、分页、重试和离线导入
│  ├─ reportmodel.*             # 报表模型、校验和排序
│  ├─ reportanalytics.*         # 趋势与渠道分析图
│  ├─ xlsxexporter.*            # 原生 OOXML/ZIP 导出
│  └─ ticketpanel.*             # 工单列表、详情和操作界面
├─ docs/protocol.md             # 协议说明
└─ demo-data/                   # 演示数据库
```

## 建议测试场景

- 注册、正常登录、错误密码和重复登录互踢。
- 多客户端公共聊天、私聊、上下线通知和历史消息加载。
- 停止并重启服务端，观察客户端心跳超时和自动重连。
- 查询、取消、重试、分页、离线导入和 Excel 导出报表。
- 从报表行创建工单，并由支持账号完成认领和解决。
- 普通账号关闭或退回工单，重新登录后检查历史和留言。
- 检查日志文件中不包含明文密码。

## 当前范围

- 项目定位为桌面端业务系统演示，当前不包含生产级 TLS、密码哈希迁移、集中式权限平台和服务端集群部署。
- 报表 HTTP 服务作为外部数据源，不包含在本仓库的客户端/协同服务端构建产物中。
