#ifndef CECAUDIO_H
#define CECAUDIO_H

#include <QObject>
#include <libcec/cec.h>

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

    void commandReceived(const CEC::cec_command* command);
    static void commandReceived(void* cbparam, const CEC::cec_command* command)
                {static_cast<CECAudio *>(cbparam)->commandReceived(command);}

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

public:
    CECAudio();
    virtual ~CECAudio();

    bool init();

    CEC::cec_power_status getTVPower() const {return tv_power_;}
    CEC::cec_logical_address getActiveAddress() const {return active_device_;}
    std::string getActiveName() const {return cec_adapter->GetDeviceOSDName(active_device_);}

public slots:
    CEC::cec_power_status tv_power() const;
    void setTv_power(CEC::cec_power_status newTv_power);
    CEC::cec_logical_address active_device() const;
    void setActive_device(const CEC::cec_logical_address &newActive_device);
    CEC::cec_log_level log_level() const;
    void setLog_level(CEC::cec_log_level newLog_level);

signals:
    void tv_powerChanged(CEC::cec_power_status power);
    void active_deviceChanged(CEC::cec_logical_address logaddr, std::string name);
    void volumeUp(bool pressed);
    void volumeDown(bool pressed);
    void toggleMute();

private slots:

};

#endif // CECAUDIO_H
