/**
 * @file       HologramSIMCOM.cpp
 * @author     Ben Strahan
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2017 Ben Strahan
 * @date       Apr 2017
**/

#include "HologramSIMCOM.h"

/*--------------------------------------------------------
PUBLIC
---------------------------------------------------------*/

bool HologramSIMCOM::begin() {
    bool initiated = false;

    serialHologram.begin(115200);
//    while(!serialHologram); // wait for Serial to be ready

    Serial.println(F("Configuring to 9600 baud"));
    serialHologram.println("AT+IPR=9600"); // Set baud rate
    serialHologram.begin(9600);

    int timeout = 5000;
    while(_writeCommand("AT\r\n", 1, "OK", "ERROR") != 2 && timeout > 0) {
    	delay(500);
    	timeout -= 500;
    }
    if (timeout <= 0) {
        Serial.println(F("ERROR: begin() failed at AT"));
        return false;
    }

    _MODEMSTATE = 1; // set state as available

    // RUN MODEM BEGIN AT COMMANDS
    while(1) {
        // Synchronize baud-rate
        char baud_command[20];
        snprintf(baud_command, sizeof(baud_command), "AT+IPR=%i\r\n", 9600);
        if(_writeCommand(baud_command, 1, "OK", "ERROR") != 2) {
            Serial.println(F("ERROR: begin() failed at +IPR"));
            break;
        }

        // Check if SIM is ready
        if(_writeCommand("AT+CPIN?\r\n", 5, "OK", "ERROR") != 2) {
            Serial.println(F("ERROR: begin() failed at +CPIN"));
            break;
        }

        if(cellStrength() == 0) {
            Serial.println(F("ERROR: no signal"));
            break;
        }

        // Set SMS Functionality to text
        if(_writeCommand("AT+CMGF=1\r\n", 10, "OK", "ERROR") != 2) {
            Serial.println(F("ERROR: begin() failed at +CMGF"));
            break;
        }

        // connect to network
        if(!_connectNetwork()) {
            break;
        }

        initiated = true;
        break;
    }

    return initiated;
}

bool HologramSIMCOM::begin(const int port) {
    bool initiated = begin();

    if (initiated) {
        _SERVERPORT = port;

        // Start server on provided port
        char server_command[30];

        snprintf(server_command, sizeof(server_command), "AT+SERVERSTART=%i,1\r\n", _SERVERPORT);
        delay(500);
        switch(_writeCommand(server_command, 5, "OK", "ERROR")) {
            case 0:
                Serial.println(F("ERROR: Server start timed out"));
                return false;
            case 1:
                Serial.println(F("ERROR: Server start errored out"));
                return false;
            case 2:
                return true;
        }
    }
    return false;
}

void HologramSIMCOM::debug() {
    if(_DEBUG == 0) {
        _DEBUG = 1;
        Serial.println(F("DEBUG: Verbose monitoring and modem serial access enabled"));
    }

    // keep track of anything written into the MCU serial
    // collect it and write it to the modem serial
    if (Serial.available() > 0) {
        _SERIALBUFFER = "";

        delay(100); // allow for buffer to build
        while(Serial.available() > 0) {
            char r = Serial.read();
            _SERIALBUFFER += r;
        }

        char write_string[_SERIALBUFFER.length()];
        _SERIALBUFFER.toCharArray(write_string, sizeof(write_string));
        _writeSerial(write_string);
    }

    // normally we get debug messages when another function runs a modem command
    // but if there is not another function using the modem then we need to listen
    // MAKE SURE Arduino serial monitor is sending both NL & CR!!
    if(_MODEMSTATE == 1 && serialHologram.available() > 0) {
        _MODEMSTATE = 0;
        while(serialHologram.available() > 0) {
            _readSerial();
        }
        _MODEMSTATE = 1;
    }
}

bool HologramSIMCOM::cellService() {
    // check GPRS Status
    bool connection;

    // need to check each of these commands depending on how connection is lost
    int gprs = _writeCommand("AT+CGATT?\r\n", 10, "+CGATT: 1", "ERROR");
    int pdp = _writeCommand("AT+CIPSTATUS?\r\n", 10, "IP", "DEACT");
    int mux = _writeCommand("AT+CIPMUX?\r\n", 10, "+CIPMUX: 1", "+CIPMUX: 0");
    int ip = _writeCommand("AT+CIFSR\r\n", 1, ".", "ERROR");
    int sig = cellStrength();

    if(gprs == 2 && pdp == 2 && mux == 2 && ip == 2 && sig > 0) {
        connection = true;
    } else {
        // if no connection, try reconnecting
        if(!_connectNetwork()) {
            Serial.println(F("ERROR: unable to connect to cellular network"));
            connection = false;
        }
        connection = true;
    }

    return connection;
}


int HologramSIMCOM::cellStrength() {
    if(_writeCommand("AT+CSQ\r\n", 1, "+CSQ:", "ERROR") == 2) {
    	int strength = _SERIALBUFFER.substring(_SERIALBUFFER.indexOf(": ")+2,_SERIALBUFFER.indexOf(",")).toInt();

        if(strength == 99 || strength == 0) {
            return 0;
        } else if(strength > 24) {
            return 5;
        } else if(strength > 16) {
            return 4;
        } else if(strength > 11) {
            return 3;
        } else if(strength > 6) {
            return 2;
        } else {
            return 1;
        }
    } else {
        return -1;
    }
}

bool HologramSIMCOM::send(String data) {
    // modify data for TCP transmittal
    data.replace("\"","\\\"");
    data = "{\"k\":\""+ String(_DEVICEKEY)
           + "\",\"d\":\"" + data + "\"}\r\n";

    bool sent = _sendMessage(data);
    return sent;
}

bool HologramSIMCOM::send(uint8_t client, uint8_t messageNr, uint8_t packetNr, uint8_t type, const char* data) {
	String message = "{\"c\":" + String(client) +
			",\"m\":" + String(messageNr) +
			",\"p\":" + String(packetNr) +
			",\"t\":" + String(type) +
			",\"d\":\"" + String(data) +
			"\"}";

    bool sent = send(message);
    return sent;
}

bool HologramSIMCOM::sendSMS(const String phoneNum, String message) {
    // modify data for TCP transmittal
    message = "S" + String(_DEVICEKEY)
              + phoneNum + " " + message + "\r\n";

    bool sent = _sendMessage(message);
    return sent;
}

bool HologramSIMCOM::sendSMS(const char* phoneNum, const char* message) {
    bool sent = sendSMS(String(phoneNum), String(message));
    return sent;
}

int HologramSIMCOM::availableMessage() {
    int l = _MESSAGEBUFFER.length();

    if(l < 1) { // if no _MESSAGEBUFFER check serial
        _MODEMSTATE = 0;
        _readSerial(); // lets pull a new message if any
        _MODEMSTATE = 1;
        l = _MESSAGEBUFFER.length(); // recheck length
    }

    return l;
}

String HologramSIMCOM::readMessage() {
    String returnMessage = _MESSAGEBUFFER;
    _MESSAGEBUFFER = ""; // wipe _MESSAGEBUFFER
    return returnMessage;
}

/*--------------------------------------------------------
PRIVATE
---------------------------------------------------------*/

void HologramSIMCOM::_readSerial() {
    // IMPORTANT: I want to tightly control reading from serialHologram,
    // this is the only function allowed to do it
    _SERIALBUFFER = "";

    if (serialHologram.available() > 0) {
        delay(20); // allow for buffer to build
        while ( serialHologram.available() > 0 ) { // move serial buffer into global String
            char r = serialHologram.read();
            _SERIALBUFFER += r;

            if (_SERIALBUFFER.endsWith("\n" )) { // we read the serial one line at a time
                _SERIALBUFFER.replace("\r","");
            }
        }

        _checkIfInbound();

        if(_DEBUG == 1) {
            Serial.print(F("DEBUG: Modem Serial Buffer = "));
            Serial.println(_SERIALBUFFER);
            if(_MESSAGEBUFFER.length() > 0) {
                Serial.print(F("DEBUG: Message Buffer = "));
                Serial.println(_MESSAGEBUFFER);
            }
        }
    }
}

void HologramSIMCOM::_checkIfInbound() {
    // Check for inbound message and throw incoming into _MESSAGEBUFFER
    if(_SERIALBUFFER.indexOf("+CLIENT: ") != -1) {

    	int pos = _SERIALBUFFER.indexOf("+CLIENT: ") + strlen("+CLIENT: ");
    	int link = _SERIALBUFFER.substring(pos, pos + 1).toInt();

    	_MESSAGEBUFFER = _SERIALBUFFER;

        // this is a little wonky, need to override _MODEMSTATE to execute
        _MODEMSTATE = 1;
        _sendResponse(link, "OK");
        _MODEMSTATE = 0;

    }
}

void HologramSIMCOM::_writeSerial(const char* string) {
    // Note: this expects you to check state before calling
    // IMPORTANT: I want to tightly control writing to serialHologram,
    // this is the only function allowed to do it

    if(_DEBUG == 1) {
        Serial.print(F("DEBUG: Write Modem Serial = "));
        Serial.println(string);
    }

    serialHologram.write(string); // send command
}

void HologramSIMCOM::_writeCommand(const char* command, const unsigned long timeout) {
    if(_MODEMSTATE == 1) {//check if serial is available
        unsigned start = millis();
        _MODEMSTATE = 0; // set state as busy

        _writeSerial(command); // send command to modem

        while(timeout * 1000 > millis() - start) { // wait for timeout to complete
            // only break if there is a response
            while(_SERIALBUFFER.length() == 0) {
                _readSerial();
                if(_SERIALBUFFER.length() > 0) {
                    break;
                }
            }
        }

        _MODEMSTATE = 1; // set state as available
    }
}

int HologramSIMCOM::_writeCommand(const char* command, const unsigned long timeout,
		const String successResp, const String errorResp) {
    unsigned int timeoutTime;
    if(_MODEMSTATE == 1) {//check if serial is available
        unsigned long start = millis();
        _MODEMSTATE = 0; // set state as busy

        _writeSerial(command); // send command to modem

        while(timeout * 1000 > millis() - start) { // wait for timeout to complete
            _readSerial();
            if(_SERIALBUFFER.indexOf(successResp) != -1 || _SERIALBUFFER.indexOf(errorResp) != -1) {
                break;
            }
        }

        if(timeout * 1000 > millis() - start){
            timeoutTime = (millis() - start) / 1000;
        } else {
            timeoutTime = 0;
        }

        _MODEMSTATE = 1; // set state as available

        if(_SERIALBUFFER.indexOf(successResp) != -1) {
            return 2;
        } else if(_SERIALBUFFER.indexOf(errorResp) != -1) {
            Serial.print(F("ERROR: Error resp when calling "));
            Serial.println(command);
            return 1;
        } else { // timed out
            Serial.print(F("ERROR: Timeout when calling "));
            Serial.print(command);
            Serial.print(F(" | elapsed ms = "));
            Serial.println(timeoutTime);
            return 0;
        }
    } else {
        Serial.println(F("ERROR: Modem not available"));
        return 1;
    }
}

bool HologramSIMCOM::_connectNetwork() {
    bool connection = false;

    while(1) {
        if(cellStrength() == 0) {
            Serial.println(F("ERROR: no signal"));
            break;
        }

        int timeout = 10000;
        while (_writeCommand("AT+NETOPEN\r\n", 5, "OK", "ERROR") != 2 && timeout > 0) {
        	// Close socket
        	_writeCommand("AT+NETCLOSE\r\n", 5, "", "");
        	delay(500);
        	timeout -= 500;
        }
        if (timeout <= 0) {
        	Serial.println(F("ERROR: failed at +NETOPEN (1)"));
        	break;
        }

        timeout = 10000;
        while (_writeCommand("AT+NETOPEN?\r\n", 5, "1", "0") != 2 && timeout > 0) {
        	delay(500);
        	timeout -= 500;
        }
        if (timeout <= 0) {
        	Serial.println(F("ERROR: failed at +NETOPEN (2)"));
        	break;
        }

        connection = true;
        break;
    }

    return connection;
}

bool HologramSIMCOM::_sendMessage(const String data) {
    // Start TCP connection
    if(_writeCommand("AT+CIPOPEN=1,\"TCP\",\"23.253.146.203\",9999\r\n", 75, "+CIPOPEN: 1,0", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to start TCP connection"));
        return false;
    }

    // Determine message length
    char cipsend_command[22];
    snprintf(cipsend_command, sizeof(cipsend_command), "AT+CIPSEND=1,%i\r\n", data.length());
    if(_writeCommand(cipsend_command, 5, ">", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to initiaite CIPSEND"));
        return false;
    }

    // send data message to server
    char dataChar[data.length()];
    data.toCharArray(dataChar, data.length());
    _writeSerial(dataChar);
    if(_writeCommand("\r\n", 60, "OK", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to send data message"));
        return false;
    }

    // wait for the connection to close before returning
    _writeCommand("", 10, "CLOSED", "ERROR");

    return true;
}

bool HologramSIMCOM::_sendResponse(int link, const char* data) {
    // Determine message length
    char cipsend_command[22];
    snprintf(cipsend_command, sizeof(cipsend_command), "AT+CIPSEND=%i,%i\r\n", link, sizeof(data));
    if(_writeCommand(cipsend_command, 5, ">", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to initiaite CIPSEND"));
        return false;
    }

    // send data message to server
    _writeSerial(data);
    if(_writeCommand("\r\n", 60, "OK", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to send data message"));
        return false;
    }

    return true;
}
