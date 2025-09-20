#ifndef TVCEC_H
#define TVCEC_H

#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QTimer>
#include <QWebSocket>
#include "cecaudio.h"
#include "ceclog.h"
#include <time.h>

class TVCEC : public QObject
{
    Q_OBJECT

private:
    CECAudio            *cec_;                  // CEC Audio device
    CECLog              *log_;                  // Logger

    QString             remote_;                // Remote IP address
    QWebSocket          *websocket_;            // Websocket to control device

    QTimer              *volTimer_;             // Volume key timer
    int                 volume_;                // Volume
    int                 volCountAdj_;           // Volume count adjustment
    bool                muted_;                 // Sound muted
    void adjustVolume(const QString &func, int repeat);

    bool sendToWebsocket(const QJsonObject &msg);
    bool sendButtonClick(const char *label);
    bool sendButtonPress(const char *label);
    bool sendButtonRelease(const char *label);

    struct MsgQueEntry
    {
        time_t          queued;
        QString         msg;

        MsgQueEntry(const time_t &t, const QString &m) : queued(t), msg(m) {}
    };
    QList<MsgQueEntry>  msg_queue_;
    bool                push_to_front_;         // Flag to push to front of queue

    QTimer              *timer_;                // Timer for health check
    int                 health_;                // Health counter

public:
    explicit TVCEC(QObject *parent = nullptr);
    virtual ~TVCEC();

    bool init();

    void setRemote(const QString &remote) {remote_ = remote;}
    void setLogLevel(CEC::cec_log_level level) {cec_->setLog_level(level);}
    void setLogFile(const char *filename) {log_->setLogFile(filename);}

public slots:
    void tv_powerChanged(CEC::cec_power_status power);
    void active_deviceChanged(CEC::cec_logical_address logaddr, std::string name);
    void volumeUp(bool pressed);
    void volumeDown(bool pressed);
    void setMuted(bool muted);
    void toggleMute();

private slots:
    void healthCheck();
    bool sendQueuedMessages();
    void textMessage(const QString &msg);
    void ws_connected();
    void ws_disconnected();
    void ws_pong(quint64 elapsedTime, const QByteArray &payload);

signals:
    void volumeChanged(int volume);
    void mutingChanged(bool muted);
};

#endif // TVCEC_H
