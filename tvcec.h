#ifndef TVCEC_H
#define TVCEC_H

#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QWebSocket>
#include "cecaudio.h"
#include <time.h>

class TVCEC : public QObject
{
    Q_OBJECT

private:
    CECAudio            *cec_;                  // CEC Audio device

    QWebSocket          *websocket_;            // Websocket to control device
    bool sendToWebsocket(const QJsonObject &msg);

    struct MsgQueEntry
    {
        time_t          queued;
        QString         msg;

        MsgQueEntry(const time_t &t, const QString &m) : queued(t), msg(m) {}
    };
    QList<MsgQueEntry>  msg_queue_;

public:
    explicit TVCEC(QObject *parent = nullptr);
    virtual ~TVCEC();

public slots:
    void tv_powerChanged(CEC::cec_power_status power);
    void active_deviceChanged(CEC::cec_logical_address logaddr, std::string name);
    void volumeUp(bool pressed);
    void volumeDown(bool pressed);
    void toggleMute();

private slots:
    bool sendQueuedMessages();

signals:

};

#endif // TVCEC_H
