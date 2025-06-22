#include "tvcec.h"
#include <QtDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

TVCEC::TVCEC(QObject *parent) : QObject{parent}, push_to_front_(false), health_(0)
{
    websocket_ = new QWebSocket("tvcec");
    connect(websocket_, &QWebSocket::connected, this, &TVCEC::ws_connected);
    connect(websocket_, &QWebSocket::disconnected, this, &TVCEC::ws_disconnected);
    connect(websocket_, &QWebSocket::pong, this, &TVCEC::ws_pong);

    cec_ = new CECAudio();
    connect(cec_, &CECAudio::tv_powerChanged, this, &TVCEC::tv_powerChanged);
    connect(cec_, &CECAudio::active_deviceChanged, this, &TVCEC::active_deviceChanged);
    connect(cec_, &CECAudio::volumeUp, this, &TVCEC::volumeUp);
    connect(cec_, &CECAudio::volumeDown, this, &TVCEC::volumeDown);
    connect(cec_, &CECAudio::toggleMute, this, &TVCEC::toggleMute);
    cec_->init();

    timer_ = new QTimer(this);
    timer_->setInterval(30000);
    connect(timer_, &QTimer::timeout, this, &TVCEC::healthCheck);
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
    if (!push_to_front_)
    {
        msg_queue_.emplaceBack(now, QString(txtmsg));
        return sendQueuedMessages();
    }
    msg_queue_.emplaceFront(now, QString(txtmsg));
    return true;
}

bool TVCEC::sendQueuedMessages()
{
    bool ret = true;

    //  Delete expired messages
    time_t expire;
    time(&expire);
    expire -= 30;
    while (!msg_queue_.isEmpty() && msg_queue_.front().queued < expire)
    {
        qDebug() << "Delete expired message" << msg_queue_.front().msg << "expired" << (expire - msg_queue_.front().queued);
        msg_queue_.pop_front();
        ret = false;
    }

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
        qDebug() << "Open websocket";
        websocket_->open(QUrl("ws://tvadapter.local"));
    }
    return ret;
}

void TVCEC::ws_connected()
{
    qDebug() << "Websocket connected";
    // Push power status and active device to front of queue
    push_to_front_ = true;
    active_deviceChanged(cec_->getActiveAddress(), cec_->getActiveName());
    tv_powerChanged(cec_->getTVPower());
    push_to_front_ = false;
    sendQueuedMessages();

    health_ = 0;
    timer_->start();
}

void TVCEC::ws_disconnected()
{
    qDebug() << "websocket disconnected";
    timer_->stop();
    health_ = 0;
}

void TVCEC::ws_pong(quint64 elapsedTime, const QByteArray &payload)
{
    health_ = 0;
    qDebug() << "ping/pong elapsed msec:" <<elapsedTime;
}

void TVCEC::healthCheck()
{
    health_++;
    if (health_ < 5)
    {
        websocket_->ping();
    }
    else
    {
        qDebug() << "Health check limit reached! Close websocket.";
        websocket_->close();
    }
}
