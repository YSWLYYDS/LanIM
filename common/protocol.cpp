#include "protocol.h"

#include <QJsonDocument>

QByteArray Protocol::pack(const QJsonObject &message)
{
    const QByteArray payload=QJsonDocument(message).toJson(QJsonDocument::Compact);//转换为紧凑 JSON 字节,payload是JSON载荷
    if(payload.size()>static_cast<qsizetype>(MaxPayloadSize)){
        return{};
    }//检查长度

    const quint32 payloadLength=static_cast<quint32>(payload.size());//将载荷大小保存为固定的 32 位无符号整数,quint32 正好是 4 字节

    QByteArray frame;
    frame.reserve(4+payload.size());//预留内存空间

    //把 32 位长度拆成 4 个字节，并按“大端字节序”放入 frame
    frame.append(static_cast<char>((payloadLength>>24)&0XFF));
    frame.append(static_cast<char>((payloadLength>>16)&0XFF));
    frame.append(static_cast<char>((payloadLength>>8)&0XFF));
    frame.append(static_cast<char>(payloadLength&0XFF));

    frame.append(payload);
    return frame;
}

Protocol::DecodeResult Protocol::unpack(QByteArray &buffer, QJsonObject &message, QString *errorMessage)
{
    if(errorMessage!=nullptr){
        errorMessage->clear();
    }

    if(buffer.size()<4)//半包等待
    {
        return DecodeResult::NeedMoreData;
    }

    const quint32 payloadLength=
        (static_cast<quint32>(static_cast<quint8>(buffer.at(0))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(buffer.at(1))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(buffer.at(2))) << 8)
        | static_cast<quint32>(static_cast<quint8>(buffer.at(3)));

    if(payloadLength>MaxPayloadSize){
        if(errorMessage!=nullptr){
            *errorMessage="Payload length exceeds the protocol limit.";
        }
        return DecodeResult::InvalidFrame;
    }

    const qsizetype frameSize=4+static_cast<qsizetype>(payloadLength);

    if(buffer.size()<frameSize){
        return DecodeResult::NeedMoreData;
    }

    const QByteArray payload=buffer.mid(4,payloadLength);//计算完整帧大小

    QJsonParseError parseError;
    const QJsonDocument document=QJsonDocument::fromJson(payload,&parseError);

    if(parseError.error!=QJsonParseError::NoError||!document.isObject())
    {
        if(errorMessage!=nullptr){
            *errorMessage="Payload is not a valid JSON object.";
        }
        return DecodeResult::InvalidJson;
    }

    message=document.object();
    buffer.remove(0,frameSize);
    return DecodeResult::Success;
}
