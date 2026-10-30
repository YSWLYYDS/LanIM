#ifndef PROTOCOL_H
#define PROTOCOL_H

#include<QByteArray>
#include<QJsonObject>
#include<QString>
#include <QtGlobal>

class Protocol//将消息变成字节通过TCP传输后将字节还原成消息。
{
public:
    static constexpr quint32 MaxPayloadSize=1024 * 1024;//限制单条消息最多 1 MiB

    enum class DecodeResult//拆包结果
    {
        NeedMoreData,//数据不足 4 字节，或不足完整载荷
        Success,//成功取出一条完整 JSON 消息
        InvalidFrame,//长度非法
        InvalidJson//不是合法 JSON 对象
    };

    static QByteArray pack(const QJsonObject &message);//封包
    static DecodeResult unpack(QByteArray &buffer,QJsonObject &message,QString *errorMessage=nullptr);//拆包
};
#endif // PROTOCOL_H
