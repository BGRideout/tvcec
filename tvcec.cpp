#include "tvcec.h"
#include <QtDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

TVCEC::TVCEC(QObject *parent) : QObject{parent}
{
    websocket_ = new QWebSocket("tvcec");
    connect(websocket_, &QWebSocket::connected, this, &TVCEC::sendQueuedMessages);

    cec_ = new CECAudio();
    connect(cec_, &CECAudio::tv_powerChanged, this, &TVCEC::tv_powerChanged);
    connect(cec_, &CECAudio::active_deviceChanged, this, &TVCEC::active_deviceChanged);
    connect(cec_, &CECAudio::volumeUp, this, &TVCEC::volumeUp);
    connect(cec_, &CECAudio::volumeDown, this, &TVCEC::volumeDown);
    connect(cec_, &CECAudio::toggleMute, this, &TVCEC::toggleMute);
    cec_->init();
}

TVCEC::~TVCEC()
{
    delete cec_;
    delete websocket_;
}

void TVCEC::tv_powerChanged(CEC::cec_power_status power)
{
    qDebug() << "Slot tv_powerChanged" << power;
    QJsonObject msg;
    msg.insert("function", QJsonValue("tv_powerChanged"));
    msg.insert("power", QJsonValue(power));
    sendToWebsocket(msg);
}

void TVCEC::active_deviceChanged(CEC::cec_logical_address logaddr, std::string name)
{
    qDebug() << "Slot active_deviceChanged" << logaddr << name.c_str();
    QJsonObject msg;
    msg.insert("function", QJsonValue("active_deviceChanged"));
    msg.insert("address", QJsonValue(logaddr));
    msg.insert("osdname", QJsonValue(name.c_str()));
    sendToWebsocket(msg);
}

void TVCEC::volumeUp(bool pressed)
{
    qDebug() << "Slot volumeUp" << pressed;
    QJsonObject msg;
    msg.insert("function", QJsonValue("volumeUp"));
    msg.insert("pressed", QJsonValue(pressed));
    sendToWebsocket(msg);
}

void TVCEC::volumeDown(bool pressed)
{
    qDebug() << "Slot volumeDown" << pressed;
    QJsonObject msg;
    msg.insert("function", QJsonValue("volumeDown"));
    msg.insert("pressed", QJsonValue(pressed));
    sendToWebsocket(msg);
}

void TVCEC::toggleMute()
{
    qDebug() << "Slot toggleMute";
    QJsonObject msg;
    msg.insert("function", QJsonValue("toggleMute"));
    sendToWebsocket(msg);
}

bool TVCEC::sendToWebsocket(const QJsonObject &msg)
{
    QJsonDocument doc(msg);
    QByteArray txtmsg = doc.toJson(QJsonDocument::Compact);
    qDebug() << txtmsg;
    time_t now;
    time(&now);
    msg_queue_.emplaceBack(now, QString(txtmsg));
    return sendQueuedMessages();
}

bool TVCEC::sendQueuedMessages()
{
    bool ret = true;
    if (websocket_->isValid())
    {
        while (!msg_queue_.isEmpty())
        {
            qint64 sts = websocket_->sendTextMessage(msg_queue_.front().msg);
            qDebug() << "Sent" << sts << "bytes of" << msg_queue_.front().msg.size();
            msg_queue_.pop_front();
        }
    }
    else
    {
        time_t expire;
        time(&expire);
        expire -= 30;
        while (!msg_queue_.isEmpty() && msg_queue_.front().queued < expire)
        {
            msg_queue_.pop_front();
            ret = false;
        }
        websocket_->open(QUrl("ws://tvadapter.local"));
    }
    return ret;
}
