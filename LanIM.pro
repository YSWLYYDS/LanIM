TEMPLATE = subdirs

SUBDIRS += \
    server \
    client

server.file = server/server.pro
client.file = client/client.pro