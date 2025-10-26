# CEC Interface for WiFi Universal Remote

This application can be installed as a service on a Raspberry Pi 4 or 5 connected
to a TV to pick up audio system commands such as power on/off, active device
selection, volume change and mute from CEC (Consumer Electronics Control) messages
on the HDMI connection. It then sends a message via websocket to the
WiFi remote to translate these messages into IR commands to a legacy audio system.

On Bookworm release of the Pi OS, you will need to build libcec separately as this
program needs libcec v7.0.0 or later, The Trixie OS release has the required version
when installed by apt.

The use of this application is described in the user manual for the WiFI Universal
Remote (webremote).

To install as a service, edit the tvcec.service file for directory and user names
and then install using the commands:

    sudo cp tvcec.service /etc/systemd/system
    sudo systemctl enable tvcec
    sudo systemctl start tvcec

