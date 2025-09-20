#ifndef CECAUDIO_H
#define CECAUDIO_H

#include <QObject>
#include <QMutex>
#include <QTimer>
#include <libcec/cec.h>

class CECLog;

class CECAudio : public QObject
{
    Q_OBJECT

private:
    CEC::ICECAdapter            *cec_adapter;
    CEC::ICECCallbacks          cec_callbacks;
    CEC::libcec_configuration   cec_config;

    CEC::cec_power_status       tv_power_;
    CEC::cec_logical_address    active_device_;
    CEC::cec_log_level          log_level_;

    int                         volume_;
    bool                        muted_;

    QTimer                      *audio_timer_;
    uint8_t                     last_audio_status_;
    mutable QMutex              audioMtx1_;
    mutable QMutex              audioMtx2_;

    CECLog                      *log_;

    uint8_t audioStatus() const;
    int sendAudioStatus(CEC::cec_logical_address destination=CEC::CECDEVICE_TV);

    void commandReceived(const CEC::cec_command* command);
    static void commandReceived(void* cbparam, const CEC::cec_command* command)
                {static_cast<CECAudio *>(cbparam)->commandReceived(command);}

    int commandHandler(const CEC::cec_command* command);
    static int commandHandler(void* cbparam, const CEC::cec_command* command)
                {return static_cast<CECAudio *>(cbparam)->commandHandler(command);}

    void logMessage(const CEC::cec_log_message* message);
    static void logMessage(void* cbparam, const CEC::cec_log_message* message)
            {static_cast<CECAudio *>(cbparam)->logMessage(message);}

    void on_keypress(const CEC::cec_keypress* msg);
    static void on_keypress(void* cbparam, const CEC::cec_keypress* msg)
            {static_cast<CECAudio *>(cbparam)->on_keypress(msg);}

    void configurationChanged(const CEC::libcec_configuration* configuration);
    static void configurationChanged(void* cbparam, const CEC::libcec_configuration* configuration)
            {static_cast<CECAudio *>(cbparam)->configurationChanged(configuration);}

    void do_alert(const CEC::libcec_alert alert, const CEC::libcec_parameter param);
    static void do_alert(void* cbparam, const CEC::libcec_alert alert, const CEC::libcec_parameter param)
            {static_cast<CECAudio *>(cbparam)->do_alert(alert, param);}

    void sourceActivated(const CEC::cec_logical_address logicalAddress, const uint8_t bActivated);
    static void sourceActivated(void* cbparam, const CEC::cec_logical_address logicalAddress, const uint8_t bActivated)
            {static_cast<CECAudio *>(cbparam)->sourceActivated(logicalAddress, bActivated);}

    CEC::cec_logical_address fromPhysical(uint16_t physical);
    uint16_t physicalFromParameters(const CEC::cec_datapacket &parameters, int offset = 0) const;

    void logResponse(const char *label, const CEC::cec_command &response);

public:
    CECAudio(CECLog *logger);
    virtual ~CECAudio();

    bool init();

    CEC::cec_power_status getTVPower() const {return cec_adapter->GetDevicePowerStatus(CEC::CECDEVICE_TV);}
    CEC::cec_logical_address getActiveAddress() const {return cec_adapter->GetActiveSource();}
    std::string getActiveName() const {return cec_adapter->GetDeviceOSDName(active_device_);}

public slots:
    CEC::cec_power_status tv_power() const;
    void setTv_power(CEC::cec_power_status newTv_power);
    CEC::cec_logical_address active_device() const;
    void setActive_device(const CEC::cec_logical_address &newActive_device);
    CEC::cec_log_level log_level() const;
    void setLog_level(CEC::cec_log_level newLog_level);

    void setVolume(int volume);
    void setMuted(bool muted);

signals:
    void tv_powerChanged(CEC::cec_power_status power);
    void active_deviceChanged(CEC::cec_logical_address logaddr, std::string name);
    void volumeUp(bool pressed);
    void volumeDown(bool pressed);
    void toggleMute();
    void triggerVolumeTimer();

private slots:
    void audio_status_timeout();
};

#endif // CECAUDIO_H
