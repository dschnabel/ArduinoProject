/**
 * @file       HologramSIMCOM.h
 * @author     Ben Strahan
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2017 Ben Strahan
 * @date       Apr 2017
**/

#ifndef __HologramSIMCOM_H__
#define __HologramSIMCOM_H__

#include "Arduino.h"
#include <SoftwareSerial.h>

class HologramSIMCOM {
public:

    // Pre-setup - set globals -----------------------------------------
    HologramSIMCOM(SoftwareSerial *mySerial, const char* deviceKey) : mySerial(mySerial) {
        _DEVICEKEY = deviceKey;
        _DEBUG = false;
        _SERVERPORT = 0;
        _SENDBUFFER = 0;
        _SENDCHANNEL = 0;
    };
    SoftwareSerial *mySerial;

    // Setup Methods ----------------------------------------------------
    bool begin(const uint32_t baud); // Must pass baud to setup module
    bool begin(const uint32_t baud, const int port); // Passing port will also start module server
    bool openSocket();
    bool closeSocket();

    // Loop Methods ------------------------------------------------------
    int cellStrength(); // return cell reception strength [0-none,1-poor,2-good,3-great]
    void debug(); // enables manual serial and verbose monitoring

    int sendOpenConnection(uint8_t client, uint8_t messageNr, uint8_t packetNr, uint8_t type);
    bool sendCloseConnection();
    unsigned int sendAppendData(const char *data);
    bool sendSendOff();

    int availableMessage(); // checks if server message, returns message length
    String readMessage(); // returns message as String, resets server, resets message buffer

private:
    // Globals ------------------------------------------------------------
    const char* _DEVICEKEY; // Hologram's Device Key
    int _SERVERPORT; // Modem's server port
    int _MODEMSTATE = 1; // State of modem [0-busy, 1-available]
    int _DEBUG; // State of debug flag [0-false, 1-true]
    String _MESSAGEBUFFER = ""; // Where we store inbound messages (maybe make it a char array?)
    String _SERIALBUFFER = ""; // Where we store serial reads on line at a time (maybe make it a char array?)
    unsigned int _SENDBUFFER;
    byte _SENDCHANNEL;

    // General modem functions --------------------------------------------
    void _initSerial(const uint32_t baud);
    void _stopSerial();
    void _writeSerial(const char* string, bool hide = false); // send command to modem without waiting
    void _writeCommand(const char* command, const unsigned long timeout); // send command to modem and wait
    // Send command, wait response or timeout, return [0-timeout,1-error,2-success]
    int _writeCommand(const char* command, const unsigned long timeout, const String successResp, const String errorResp);
    void _readSerial(); // reads/analyze modem serial, store read/inbound in globals, set states, etc
    void _checkIfInbound(); // check modem server for inbound messages
    bool _connectNetwork(); // establish a connection to network and set cipstatus in prep to send/receive data

    // Server functions ---------------------------------------------------
    // future versions

    // Client functions
    bool _sendResponse(int link, const char* data);
};

#endif
