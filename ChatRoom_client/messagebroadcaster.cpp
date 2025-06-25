QList<QByteArray> MessageBroadcaster::createMessagePacket(MessageType type, const QString &data)
{
    return createMessagePacket(type, data.toUtf8());
} 