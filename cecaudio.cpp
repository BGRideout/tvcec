#include "cecaudio.h"
#include "ceclog.h"
#include <algorithm>
#include <array>
#include <QMutexLocker>
#include <QtDebug>

// cecloader.h uses std::cout _without_ including iosfwd or iostream
// Furthermore is uses cout and not std::cout
#include <iostream>
using std::cout;
using std::endl;
#include <libcec/cecloader.h>

CECAudio::CECAudio(CECLog *logger) : log_(logger), tv_power_(CEC::CEC_POWER_STATUS_STANDBY), active_device_(CEC::CECDEVICE_UNKNOWN),
    log_level_(CEC::CEC_LOG_ERROR), volume_(60), muted_(false)
{
    cec_config.Clear();
    cec_callbacks.Clear();

    const std::string devicename("TVCECAudio");
    devicename.copy(cec_config.strDeviceName, std::min(devicename.size(), static_cast<std::string::size_type>(LIBCEC_OSD_NAME_SIZE)) );

    cec_config.clientVersion       = CEC::LIBCEC_VERSION_CURRENT;
    cec_config.bActivateSource     = 0;
    cec_config.callbacks           = nullptr;
    cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_AUDIO_SYSTEM);

    cec_callbacks.commandReceived   = &commandReceived;
    cec_callbacks.commandHandler    = &commandHandler;
    cec_callbacks.keyPress          = &on_keypress;
    cec_callbacks.logMessage        = &logMessage;
    cec_callbacks.alert             = &do_alert;
    cec_callbacks.configurationChanged = &configurationChanged;
    cec_callbacks.sourceActivated   = &sourceActivated;

    audio_timer_ = new QTimer(this);
    audio_timer_->setInterval(500);
    audio_timer_->setSingleShot(true);
    connect(audio_timer_, &QTimer::timeout, this, &CECAudio::audio_status_timeout);
    connect(this, &CECAudio::triggerVolumeTimer, audio_timer_, qOverload<>(&QTimer::start));
}

CECAudio::~CECAudio()
{
    // Close down and cleanup
    if (cec_adapter)
    {
        cec_adapter->Close();
        UnloadLibCec(cec_adapter);
    }
}

bool CECAudio::init()
{
    // Get a cec adapter by initialising the cec library
    cec_adapter = LibCecInitialise(&cec_config);
    if( !cec_adapter )
    {
        std::cerr << "Failed loading libcec.so\n";
        return false;
    }

    //  Enable callbacks
    cec_adapter->SetCallbacks(&cec_callbacks, this);

    // Try to automatically determine the CEC devices
    std::array<CEC::cec_adapter_descriptor,10> devices;
    int8_t devices_found = cec_adapter->DetectAdapters(devices.data(), devices.size(), nullptr, true /*quickscan*/);
    if( devices_found <= 0)
    {
        std::cerr << "Could not automatically determine the cec adapter devices\n";
        UnloadLibCec(cec_adapter);
        return false;
    }

    // Open a connection to the zeroth CEC device
    if( !cec_adapter->Open(devices[0].strComName) )
    {
        std::cerr << "Failed to open the CEC device on port " << devices[0].strComName << std::endl;
        UnloadLibCec(cec_adapter);
        return false;
    }

    //  Get the power and active source
    tv_power_ = getTVPower();
    active_device_ = getActiveAddress();
    log_->print("CECAudio initialized. tv_power_ %d, active_device_ %d", tv_power_, active_device_);
    return true;
}


CEC::cec_power_status CECAudio::tv_power() const
{
    return tv_power_;
}

void CECAudio::setTv_power(CEC::cec_power_status newTv_power)
{
    if (tv_power_ == newTv_power)
        return;
    tv_power_ = newTv_power;
    if (log_level_ & CEC::CEC_LOG_DEBUG)
    {
        std::cout << "TV Power = " << cec_adapter->ToString(tv_power_) << " (" << tv_power_ << ")" << endl;
    }
    emit tv_powerChanged(tv_power_);
}

CEC::cec_logical_address CECAudio::active_device() const
{
    return active_device_;
}

void CECAudio::setActive_device(const CEC::cec_logical_address &newActive_device)
{
    if (active_device_ == newActive_device)
        return;
    active_device_ = newActive_device;
    std::string osdname = cec_adapter->GetDeviceOSDName(active_device_);
    if (tv_power_ != CEC::CEC_POWER_STATUS_ON)
    {
        CEC::cec_power_status tvpower = cec_adapter->GetDevicePowerStatus(CEC::CECDEVICE_TV);
        setTv_power(tvpower);
    }
    if (log_level_ & CEC::CEC_LOG_DEBUG)
    {
        std::cout << "Active device = " << active_device_ << " name = " << osdname << endl;
    }
    emit active_deviceChanged(active_device_, osdname);
}

CEC::cec_log_level CECAudio::log_level() const
{
    return log_level_;
}

void CECAudio::setLog_level(CEC::cec_log_level newLog_level)
{
    log_level_ = newLog_level;
}

void CECAudio::setVolume(int volume)
{
    QMutexLocker lk(&audioMtx1_);
    volume_ = volume;
}

void CECAudio::setMuted(bool muted)
{
    QMutexLocker lk(&audioMtx1_);
    muted_ = muted;
}

void CECAudio::audio_status_timeout()
{
    uint8_t status = audioStatus();
    if (status != last_audio_status_)
    {
        sendAudioStatus();
    }
}

uint8_t CECAudio::audioStatus() const
{
    QMutexLocker lk(&audioMtx1_);
    uint8_t ret = 0;
    int vol = volume_;
    if (vol < CEC::CEC_AUDIO_VOLUME_MIN)
    {
        vol = CEC::CEC_AUDIO_VOLUME_MIN;
    }
    if (vol > CEC::CEC_AUDIO_VOLUME_MAX)
    {
        vol = CEC::CEC_AUDIO_VOLUME_MAX;
    }
    ret |= static_cast<uint8_t>(vol) & CEC::CEC_AUDIO_VOLUME_STATUS_MASK;
    if (muted_)
    {
        ret |= CEC::CEC_AUDIO_MUTE_STATUS_MASK;
    }
    return ret;
}

int CECAudio::sendAudioStatus(CEC::cec_logical_address destination)
{
    QMutexLocker lk(&audioMtx2_);
    last_audio_status_ = audioStatus();
    CEC::cec_command response;
    response.Format(response, CEC::CECDEVICE_AUDIOSYSTEM, destination, CEC::CEC_OPCODE_REPORT_AUDIO_STATUS);
    response.PushBack(last_audio_status_);
    int ret = cec_adapter->Transmit(response);
    emit triggerVolumeTimer();
    logResponse("sendAudioStatus", response);
    return ret;
}

void CECAudio::commandReceived(const CEC::cec_command *command)
{
    //  Moved to commandHandler for libcec v7
}

int CECAudio::commandHandler(const CEC::cec_command *command)
{
    int ret = 0;

    if (log_level() & CEC::CEC_LOG_NOTICE)
    {
        std::cout << "***** commandHandler " << command->initiator << " " << command->destination << std::hex <<
                     "  opcode=" << command->opcode << " (" << cec_adapter->ToString(command->opcode) << ")";
        if (command->parameters.size > 0)
        {
            std::cout << " data[" << (uint)command->parameters.size << "]";
            for (int ii = 0; ii < command->parameters.size; ii++)
            {
                std::cout << " " << std::hex << (uint)command->parameters.data[ii];
            }
        }
        std::cout << endl;
    }

    switch (command->opcode)
    {
    case CEC::CEC_OPCODE_REPORT_POWER_STATUS:
        if (command->initiator == CEC::CECDEVICE_TV)
        {
            setTv_power(static_cast<CEC::cec_power_status>(command->parameters.At(0)));
        }
        break;

    case CEC::CEC_OPCODE_STANDBY:
        setTv_power(CEC::CEC_POWER_STATUS_STANDBY);
        setActive_device(CEC::CECDEVICE_UNKNOWN);
        break;

    case CEC::CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
        if (command->initiator == CEC::CECDEVICE_TV && tv_power_ != CEC::CEC_POWER_STATUS_ON)
        {
            CEC::cec_power_status tvpower = cec_adapter->GetDevicePowerStatus(CEC::CECDEVICE_TV);
            setTv_power(tvpower);
        }
        break;

    case::CEC::CEC_OPCODE_ACTIVE_SOURCE:
        setActive_device(fromPhysical(physicalFromParameters(command->parameters)));
        break;

    case CEC::CEC_OPCODE_SET_STREAM_PATH:
        setActive_device(fromPhysical(physicalFromParameters(command->parameters)));
        break;

    case CEC::CEC_OPCODE_ROUTING_CHANGE:
        setActive_device(fromPhysical(physicalFromParameters(command->parameters, 2)));
        break;

    case CEC::CEC_OPCODE_GIVE_AUDIO_STATUS:
        ret = sendAudioStatus(command->initiator);
        break;

    default:
        break;
    }

    return ret;
}

void CECAudio::logMessage(const CEC::cec_log_message *message)
{
    if (message->level & log_level_)
    {
        std::cout << "Message (mask=" << std::hex << message->level << ")" << endl;
        std::cout << message->message << endl;
    }
}

void CECAudio::on_keypress(const CEC::cec_keypress *msg)
{
    if (log_level_ & CEC::CEC_LOG_DEBUG)
    {
        std::cout << endl << "***** on_keypress: " << std::hex << msg->keycode << " duration " << std::dec << msg->duration << endl;
    }

    switch (msg->keycode)
    {
    case CEC::CEC_USER_CONTROL_CODE_VOLUME_UP:
        emit volumeUp(msg->duration == 0);
        if (log_level_ & CEC::CEC_LOG_DEBUG)
        {
            std::cout << "!!!!! volume up " << (msg->duration == 0 ? "pressed" : "released") << endl;
        }
        break;

    case CEC::CEC_USER_CONTROL_CODE_VOLUME_DOWN:
        emit volumeDown(msg->duration == 0);
        if (log_level_ & CEC::CEC_LOG_DEBUG)
        {
            std::cout << "!!!!! volume down " << (msg->duration == 0 ? "pressed" : "released") << endl;
        }
        break;

    case CEC::CEC_USER_CONTROL_CODE_MUTE:
        if (msg->duration == 0)
        {
            emit toggleMute();
            if (log_level_ & CEC::CEC_LOG_DEBUG)
            {
                std::cout << "!!!!! toggle mute" << endl;
            }
        }
        break;

    default:
        break;
    }
}

void CECAudio::configurationChanged(const CEC::libcec_configuration* configuration)
{
    if (log_level_ & CEC::CEC_LOG_DEBUG)
    {
        std::cout << endl << "  *****  Configuration changed  *****" << endl;
    }
}

void CECAudio::do_alert(const CEC::libcec_alert alert, const CEC::libcec_parameter param)
{
    std::cout << endl << "  ***** Alert!!!  *****" << endl;
}

void CECAudio::sourceActivated(const CEC::cec_logical_address logicalAddress, const uint8_t bActivated)
{
    if (log_level_ & CEC::CEC_LOG_DEBUG)
    {
        std::cout << endl << "###### Source " << logicalAddress << (bActivated ? " activated " : " deactivated") << endl;
    }
    if (bActivated)
    {
        setActive_device(logicalAddress);
    }
}

CEC::cec_logical_address CECAudio::fromPhysical(uint16_t physical)
{
    CEC::cec_logical_address ret = CEC::CECDEVICE_UNKNOWN;
    for (int ii = 0; ii < 15; ii++)
    {
        CEC::cec_logical_address logaddr = static_cast<CEC::cec_logical_address>(ii);
        if (cec_adapter->GetDevicePhysicalAddress(logaddr) == physical)
        {
            ret = logaddr;
            break;
        }
    }
    return ret;
}

uint16_t CECAudio::physicalFromParameters(const CEC::cec_datapacket &parameters, int offset) const
{
    return (parameters.At(0 + offset) << 8) + parameters.At(1 + offset);
}

void CECAudio::logResponse(const char *label, const CEC::cec_command &response)
{
    if ((log_level() & CEC::CEC_LOG_NOTICE) != 0)
    {
        std::cout << "***** " << label << " response ***** from " << response.initiator << " to " << response.destination << std::hex <<
                     "  opcode=" << response.opcode << " (" << cec_adapter->ToString(response.opcode) << ")";
        if (response.parameters.size > 0)
        {
            std::cout << " data[" << (uint)response.parameters.size << "]";
            for (int ii = 0; ii < response.parameters.size; ii++)
            {
                std::cout << " " << std::hex << (uint)response.parameters.data[ii];
            }
        }
        std::cout << endl;
    }
}
