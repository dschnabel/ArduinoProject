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

bool HologramSIMCOM::begin(const uint32_t baud) {
    bool initiated = false;

    _initSerial(baud);
    int timeout = 30000;
    while(_writeCommand("AT\r\n", 1, "OK", "ERROR") != 2 && timeout > 0) {
    	_stopSerial();
    	delay(2000);
    	_initSerial(baud);

    	timeout -= 2000;
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
        snprintf(baud_command, sizeof(baud_command), "AT+IPR=%lu\r\n", baud);
        if(_writeCommand(baud_command, 1, "OK", "ERROR") != 2) {
            Serial.println(F("ERROR: begin() failed at +IPR"));
            break;
        }

        // Check if SIM is ready
        if(_writeCommand("AT+CPIN?\r\n", 5, "OK", "ERROR") != 2) {
            Serial.println(F("ERROR: begin() failed at +CPIN"));
            break;
        }

        int timeout = 10000;
        while (cellStrength() == 0 && timeout > 0) {
        	delay(500);
        	timeout -= 500;
        }
        if (timeout <= 0) {
            Serial.println(F("ERROR: no signal"));
            break;
        }

        // Set SMS Functionality to text
        if(_writeCommand("AT+CMGF=1\r\n", 10, "OK", "ERROR") != 2) {
            Serial.println(F("ERROR: begin() failed at +CMGF"));
            break;
        }

//        // connect to network
//        if(!_connectNetwork()) {
//            break;
//        }

        initiated = true;
        break;
    }

    return initiated;
}

bool HologramSIMCOM::begin(const uint32_t baud, const int port) {
    bool initiated = begin(baud);

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

bool HologramSIMCOM::openSocket() {
	if(!_connectNetwork()) {
		return false;
	}
	return true;
}

bool HologramSIMCOM::closeSocket() {
	if(_writeCommand("AT+NETCLOSE\r\n", 5, "OK", "ERROR") != 2) {
		Serial.println(F("ERROR: Could close socket"));
		return false;
	}
	return true;
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

int HologramSIMCOM::sendOpenConnection(uint8_t client, uint8_t messageNr, uint8_t packetNr, uint8_t type) {
	_SENDCHANNEL = _SENDCHANNEL < 9 ? _SENDCHANNEL+1 : 0;

    // Start TCP connection
	String cmd = "AT+CIPOPEN=" + String(_SENDCHANNEL) + ",\"TCP\",\"23.253.146.203\",9999\r\n";
    if(_writeCommand(cmd.c_str(), 75, "+CIPOPEN:", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to start TCP connection"));
        return -1;
    }

    cmd = "AT+CIPSEND=" + String(_SENDCHANNEL) + ",\r\n";
    if(_writeCommand(cmd.c_str(), 5, ">", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to initiaite CIPSEND"));
        return -1;
    }

    _SENDBUFFER = 1500 - 8; // 8 = strlen(fin) + "\x1a", see sendSendOff()

	String message = "{";
	if (client > 0) message += "\"c\":" + String(client) + ",";
	if (messageNr > 0) message += "\"m\":" + String(messageNr) + ",";
	if (packetNr > 0) message += "\"p\":" + String(packetNr) + ",";
	message += "\"t\":" + String(type) + ",\"d\":\"";

	message.replace("\"","\\\"");
	message = "{\"k\":\""+ String(_DEVICEKEY)
           + "\",\"d\":\"" + message;

	return sendAppendData(message.c_str());
}

bool HologramSIMCOM::sendCloseConnection() {
	String cmd = "AT+CIPCLOSE=" + String(_SENDCHANNEL) + "\r\n";
    if(_writeCommand(cmd.c_str(), 5, "OK", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to stop TCP connection"));
        return false;
    }

	return true;
}

unsigned int HologramSIMCOM::sendAppendData(const char *data) {
	uint8_t len = strlen(data);
	if (len > _SENDBUFFER) {
		return -1;
	}
	_SENDBUFFER -= len;

	_writeSerial(data, true);
//	delay(5);

	return _SENDBUFFER;
}

bool HologramSIMCOM::sendSendOff() {
	const char *fin = "\\\"}\"}\r\n";
	_writeSerial(fin);

	_writeCommand("\x1a", 0, "", "");

    // wait for the connection to close before returning
    if(_writeCommand("", 10, "+IPCLOSE:", "ERROR") != 2) {
        Serial.println(F("ERROR: failed to send data message"));
        return false;
    }

    return true;
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

void HologramSIMCOM::_initSerial(const uint32_t baud) {
    serialHologram.begin(115200);

    String b(baud);
    String s = "Configuring to " + b + " baud";
    Serial.println(s);

    s = "AT+IPR=" + b;
    serialHologram.println(s.c_str()); // Set baud rate
    serialHologram.begin(baud);
}

void HologramSIMCOM::_stopSerial() {
	serialHologram.end();
}

void HologramSIMCOM::_writeSerial(const char* string, bool hide) {
    // Note: this expects you to check state before calling
    // IMPORTANT: I want to tightly control writing to serialHologram,
    // this is the only function allowed to do it

    if(_DEBUG == 1 && !hide) {
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

        long timeout = 60000;
        while (timeout > 0) {
        	int timeout2 = 30000;
            while (_writeCommand("AT+NETOPEN\r\n", 5, "+NETOPEN: 0", "+NETOPEN: 1") != 2 && timeout2 > 0) {
            	// Close socket
            	_writeCommand("AT+NETCLOSE\r\n", 5, "", "");
            	delay(3000);
            	timeout2 -= 3000;
            	timeout -= 3000;
            }
            if (timeout2 <= 0) {
            	Serial.println(F("ERROR: failed at +NETOPEN (1)"));
            	break;
            }
            int timeout3 = 5000;
            while (_writeCommand("AT+NETOPEN?\r\n", 5, "1", "0") != 2 && timeout3 > 0) {
            	delay(500);
            	timeout3 -= 500;
            	timeout -= 500;
            }
            if (timeout3 > 0) {
            	break;
            }
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
