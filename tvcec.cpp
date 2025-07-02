#include "tvcec.h"
#include <QtDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

TVCEC::TVCEC(QObject *parent) : QObject{parent}, push_to_front_(false), health_(0), just_on_(false)
{
    remote_ = "tvremote.local";
    websocket_ = new QWebSocket("tvcec");
    connect(websocket_, &QWebSocket::connected, this, &TVCEC::ws_connected);
    connect(websocket_, &QWebSocket::disconnected, this, &TVCEC::ws_disconnected);
    connect(websocket_, &QWebSocket::textFrameReceived, this, &TVCEC::textMessage);
    connect(websocket_, &QWebSocket::pong, this, &TVCEC::ws_pong);

    cec_ = new CECAudio();
    connect(cec_, &CECAudio::tv_powerChanged, this, &TVCEC::tv_powerChanged, Qt::QueuedConnection);
    connect(cec_, &CECAudio::active_deviceChanged, this, &TVCEC::active_deviceChanged, Qt::QueuedConnection);
    connect(cec_, &CECAudio::volumeUp, this, &TVCEC::volumeUp, Qt::QueuedConnection);
    connect(cec_, &CECAudio::volumeDown, this, &TVCEC::volumeDown, Qt::QueuedConnection);
    connect(cec_, &CECAudio::toggleMute, this, &TVCEC::toggleMute, Qt::QueuedConnection);
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
    if (power == CEC::CEC_POWER_STATUS_ON)
    {
        sendButtonClick(("TVOn"));
        just_on_ = true;
    }
    else if (power == CEC::CEC_POWER_STATUS_STANDBY)
    {
        sendButtonClick(("TVOff"));
    }
}

void TVCEC::active_deviceChanged(CEC::cec_logical_address logaddr, std::string name)
{
    qDebug() << "Slot active_deviceChanged" << logaddr << name.c_str() << "just on" << just_on_;
    if (!just_on_ || logaddr == CEC::CECDEVICE_TV)
    {
        QJsonObject msg;
        msg.insert("func", QJsonValue("input_select"));
        msg.insert("path", QJsonValue("/tvadapter"));
        msg.insert("address", QJsonValue(logaddr));
        msg.insert("osdname", QJsonValue(name.c_str()));
        sendToWebsocket(msg);
    }
    else
    {
        //  Make TV the active source so audio gets directed properly when
        //  selecting app on TV
        cec_->setActive_device(CEC::CECDEVICE_TV);
    }
    just_on_ = false;
}

void TVCEC::volumeUp(bool pressed)
{
    if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
    {
        qDebug() << "Slot volumeUp" << pressed;
    }
    if (pressed)
    {
        sendButtonPress("Vol+");
    }
    else
    {
        sendButtonRelease("Vol+");
    }
}

void TVCEC::volumeDown(bool pressed)
{
    if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
    {
        qDebug() << "Slot volumeDown" << pressed;
    }
    if (pressed)
    {
        sendButtonPress("Vol-");
    }
    else
    {
        sendButtonRelease("Vol-");
    }
}

void TVCEC::toggleMute()
{
    if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
    {
        qDebug() << "Slot toggleMute";
    }
    sendButtonClick("Mute");
}

bool TVCEC::sendToWebsocket(const QJsonObject &msg)
{
    QJsonDocument doc(msg);
    QByteArray txtmsg = doc.toJson(QJsonDocument::Compact);
    if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
    {
        qDebug() << txtmsg;
    }
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

bool TVCEC::sendButtonClick(const char *label)
{
    QJsonObject msg;
    msg.insert("func", QJsonValue("tv_btn_click"));
    msg.insert("path", QJsonValue("/tvadapter"));
    msg.insert("button", QJsonValue(label));
    return sendToWebsocket(msg);
}

bool TVCEC::sendButtonPress(const char *label)
{
    QJsonObject msg;
    msg.insert("func", QJsonValue("tv_btn_press"));
    msg.insert("path", QJsonValue("/tvadapter"));
    msg.insert("button", QJsonValue(label));
    return sendToWebsocket(msg);
}

bool TVCEC::sendButtonRelease(const char *label)
{
    QJsonObject msg;
    msg.insert("func", QJsonValue("tv_btn_release"));
    msg.insert("path", QJsonValue("/tvadapter"));
    msg.insert("button", QJsonValue(label));
    return sendToWebsocket(msg);
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
            if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
            {
                qDebug() << "Sent" << sts << "bytes of" << msg_queue_.front().msg.size();
            }
            msg_queue_.pop_front();
        }
    }
    else
    {
        qDebug() << "Open websocket";
        websocket_->open(QUrl("ws://" + remote_ + "/tvcec"));
    }
    return ret;
}

void TVCEC::textMessage(const QString &msg)
{
    if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
    {
        qDebug() << "Received:" << msg;
    }
}

void TVCEC::ws_connected()
{
    qDebug() << "Websocket connected" << websocket_->requestUrl();
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
    if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
    {
        qDebug() << "ping/pong elapsed msec:" <<elapsedTime;
    }
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
