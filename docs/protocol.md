# LanIM 通信协议 v1

## 1. 为什么需要协议

LanIM 使用 TCP长连接通信。

TCP 提供的是连续字节流，不保证一次发送对应一次接收。
一条消息可能被拆成多次接收，也可能与下一条消息一起接收。

因此，LanIM需要自行定义每条消息的边界。

## 2. 数据帧格式

每条应用层消息由固定长度头和JSON载荷组成：

|字段|长度|说明|
|---|---:|---|
|payloadLength|4字节|JSON载荷的字节数，使用无符号整数和大端字节序 |
|payload | N字节 | UTF-8 编码的 JSON 文本 |

payloadLength 不包含前面自身的 4 个字节。

允许最大payload 长度为 1048576 字节，即 1 MiB。
超过最大长度的消息视为非法消息，服务端应断开连接。

## 3. JSON 消息格式

每条消息都是一个 UTF-8 编码的JSON对象：

```json
{
    "version":1,
    "type": "消息类型",
    "requestId":"请求唯一标识",
    "body": {}
}

```

字段说明：

- `version`：协议版本，当前固定为 `1`。
- `type`：消息类型，用于决定如何处理消息。
- `requestId`：一次请求的唯一标识；服务端响应时原样返回。
- `body`：具体业务数据。

## 4. 第一批消息类型

| type | 方向 | 用途 |
|---|---|---|
| `system.hello` | 客户端 -> 服务端 | 客户端连接后的协议握手 |
| `system.welcome`|服务端 -> 客户端 | 服务端确认握手成功 |
| `system.error` |服务端 -> 客户端 | 返回协议或业务错误 |
| `auth.register` | 客户端 -> 服务端 | 后续用于注册账号 |
| `auth.login` | 客户端 -> 服务端 | 后续用于登录账号 |

## 5. 握手示例

客户端发送：

```json
{
    "version":1,
    "type": "system.hello",
    "requestId":"request-001",
    "body": {
        "clientName": "LanIMClient"    
    }
}
```

服务端成功响应：

```json
{
    "version":1,
    "type": "system.welcome",
    "requestId":"request-001",
    "body": {
        "message": "Welcome to LanIM"    
    }
}
```





