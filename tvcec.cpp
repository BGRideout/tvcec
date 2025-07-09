#include "tvcec.h"
#include <QtDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <iostream>

TVCEC::TVCEC(QObject *parent) : QObject{parent}, volume_(60), volCountAdj_(0), push_to_front_(false), health_(0), just_on_(false)
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
    connect(this, &TVCEC::volumeChanged, cec_, &CECAudio::setVolume);
    connect(this, &TVCEC::mutingChanged, cec_, &CECAudio::setMuted);

    timer_ = new QTimer(this);
    timer_->setInterval(30000);
    connect(timer_, &QTimer::timeout, this, &TVCEC::healthCheck);

    volTimer_ = new QTimer(this);
    volTimer_->setInterval(3000);
    volTimer_->setSingleShot(true);
}

TVCEC::~TVCEC()
{
    delete cec_;
    delete websocket_;
}

bool TVCEC::init()
{
    return cec_->init();
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
    setMuted(false);
    if (pressed)
    {
        sendButtonPress("Vol+");
        if (!volTimer_->isActive())
        {
            volTimer_->start();
            volCountAdj_ = 1;
        }
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
    setMuted(false);
    if (pressed)
    {
        sendButtonPress("Vol-");
        if (!volTimer_->isActive())
        {
            volTimer_->start();
            volCountAdj_ = 1;
        }
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
    setMuted(!muted_);
    sendButtonClick("Mute");
}

void TVCEC::adjustVolume(const QString &func, int repeat)
{
    setMuted(false);
    if (volTimer_->isActive() && volCountAdj_ > 0)
    {
        repeat -= volCountAdj_;
        if (repeat < 0) repeat = 0;
    }
    else
    {
        if (repeat == 0) repeat = 1;
    }
    int adj = (repeat + 1) / 2;
    volCountAdj_ = 0;

    if (func == "Vol+")
    {
        volume_ += adj;
    }
    else if (func == "Vol-")
    {
        volume_ -= adj;
    }
    if (volume_ < 0) volume_ = 0;
    if (volume_ > 100) volume_ = 100;

    emit volumeChanged(volume_);
    if (cec_->log_level() & CEC::CEC_LOG_NOTICE)
    {
        std::cout << std::dec << "Volume adjusted by " << adj << " (" << repeat << ") to " << volume_ << std::endl;
    }
}

void TVCEC::setMuted(bool muted)
{
    if (muted != muted_)
    {
        muted_ = muted;
        emit mutingChanged(muted_);
    }
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
    QJsonDocument json = QJsonDocument::fromJson(msg.toUtf8());
    if (cec_->log_level() & CEC::CEC_LOG_DEBUG)
    {
        qDebug() << "Received:" << msg;
    }
    QJsonValue lbl = json.object().value("label");
    if (lbl.isString())
    {
        QString label = lbl.toString();
        if (label == "Vol+" || label == "Vol-")
        {
            QJsonValue rep = json.object().value("repetitions");
            if(!rep.isUndefined())
            {
                int repeat = rep.toString().toInt();
                adjustVolume(label, repeat);
            }
        }
        else if (label == "Mute")
        {
            // Already done in response to CEC
            // setMuted(!muted_);
        }
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
