/**
 * @file       HologramSIMCOM.cpp
 * @author     Ben Strahan
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2017 Ben Strahan
 * @date       Apr 2017
**/

#include "HologramSIMCOM.h"
#include "secrets.h"

/*--------------------------------------------------------
PUBLIC
---------------------------------------------------------*/

bool HologramSIMCOM::begin(const uint32_t baud) {
    _initSerial(baud);
    int timeout = 30000;
    while(_writeCommand("AT\r\n", 1, "OK", "ERROR") != 2 && timeout > 0) {
    	_stopSerial();
    	delay(2000);
    	_initSerial(baud);

    	timeout -= 2000;
    }
    if (timeout <= 0) {
        mySerial->println(F("ERROR: begin() failed at AT"));
        return false;
    }

    _MODEMSTATE = 1; // set state as available

    // RUN MODEM BEGIN AT COMMANDS

    // Synchronize baud-rate
    char baud_command[20];
    snprintf(baud_command, sizeof(baud_command), "AT+IPR=%lu\r\n", baud);
    if(_writeCommand(baud_command, 1, "OK", "ERROR") != 2) {
    	mySerial->println(F("ERROR: begin() failed at +IPR"));
    	return false;
    }

    // Check if SIM is ready
    if(_writeCommand("AT+CPIN?\r\n", 5, "OK", "ERROR") != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CPIN"));
    	return false;
    }

    timeout = 10000;
    while (cellStrength() == 0 && timeout > 0) {
    	delay(500);
    	timeout -= 500;
    }
    if (timeout <= 0) {
    	mySerial->println(F("ERROR: no signal"));
    	return false;
    }

    // set SSL version
    if(_writeCommand("AT+CSSLCFG=\"sslversion\",0,3\r\n", 5, "OK", "ERROR") != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"sslversion\""));
    	return false;
    }

    // set auth mode
    if(_writeCommand("AT+CSSLCFG=\"authmode\",0,2\r\n", 5, "OK", "ERROR") != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"authmode\""));
    	return false;
    }

    // set SSL CA
    if(_writeCommand("AT+CSSLCFG=\"cacert\",0,\"ca_cert.pem\"\r\n", 5, "OK", "ERROR") != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"cacert\",0,\"ca_cert.pem\""));
    	return false;
    }

    // set SSL Client Cert
    if(_writeCommand("AT+CSSLCFG=\"clientcert\",0,\"client_cert.pem\"\r\n", 5, "OK", "ERROR") != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"clientcert\",0,\"client_cert.pem\""));
    	return false;
    }

    // set SSL Client Key
    if(_writeCommand("AT+CSSLCFG=\"clientkey\",0,\"client_key.pem\"\r\n", 5, "OK", "ERROR") != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"clientkey\",0,\"client_key.pem\""));
    	return false;
    }

    // network registration
    timeout = 30000;
    while (_writeCommand("AT+COPS?\r\n", 1, "+COPS: 1,0,", "ERROR") != 2 && timeout > 0) {
    	_writeCommand("AT+CREG?\r\n", 1, "OK", "ERROR");
    	delay(1000);
    	timeout -= 1000;
    }
    if (timeout == 0) {
    	mySerial->println(F("ERROR: could not register network"));
    	return false;
    }

    return true;
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
                mySerial->println(F("ERROR: Server start timed out"));
                return false;
            case 1:
                mySerial->println(F("ERROR: Server start errored out"));
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
		mySerial->println(F("ERROR: Could close socket"));
		return false;
	}
	return true;
}

void HologramSIMCOM::debug() {
	if(_DEBUG == 0) {
        _DEBUG = 1;
        mySerial->println(F("DEBUG: Verbose monitoring and modem serial access enabled"));
    }

    // keep track of anything written into the MCU serial
    // collect it and write it to the modem serial
    if (mySerial->available() > 0) {
        _SERIALBUFFER = "";

        delay(100); // allow for buffer to build
        while(mySerial->available() > 0) {
            char r = mySerial->read();
            _SERIALBUFFER += r;
        }

        char write_string[_SERIALBUFFER.length()];
        _SERIALBUFFER.toCharArray(write_string, sizeof(write_string));
        _writeStringToSerial(write_string);
    }

    // normally we get debug messages when another function runs a modem command
    // but if there is not another function using the modem then we need to listen
    // MAKE SURE Arduino serial monitor is sending both NL & CR!!
    if(_MODEMSTATE == 1 && Serial.available() > 0) {
        _MODEMSTATE = 0;
        while(Serial.available() > 0) {
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

bool HologramSIMCOM::mqttConnect() {
	// start MQTT service
	if(_writeCommand("AT+CMQTTSTART\r\n", 5, "+CMQTTSTART: 0", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTSTART (start MQTT service)"));
        return false;
    }

    // acquire client that will connect to an SSL/TLS MQTT server
	if(_writeCommand("AT+CMQTTACCQ=0,\"client1\",1\r\n", 5, "OK", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTACCQ=0,\"client1\" (acquire client)"));
        return false;
    }

	// set context to be used in client connection
	if(_writeCommand("AT+CMQTTSSLCFG=0,0\r\n", 5, "OK", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTSSLCFG=0,0 (set context)"));
        return false;
    }

	// don't check UTF-8 encoding
	if(_writeCommand("AT+CMQTTCFG=\"checkUTF8\",0,0\r\n", 5, "OK", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTCFG=0,0 (disable checking for UTF-8)"));
        return false;
    }

	// connect to server
	String cmd = "AT+CMQTTCONNECT=0,\"" + String(AWS_IOT_ENDPOINT) + "\",660,1\r\n";
	if(_writeCommand(cmd.c_str(), 10, "+CMQTTCONNECT: 0,0", "ERROR") != 2) {
        mySerial->println(F("ERROR: begin() at +CMQTTCONNECT (connect to server)"));
        return false;
    }

	return true;
}

bool HologramSIMCOM::mqttDisconnect() {
	// disconnect from server
	if(_writeCommand("AT+CMQTTDISC=0,120\r\n", 10, "+CMQTTDISC: 0,0", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTDISC=0,120 (disconnect from server)"));
        //return false;
    }

	// release client
	if(_writeCommand("AT+CMQTTREL=0\r\n", 5, "OK", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTREL=0 (release client)"));
        //return false;
    }

	// stop MQTT service
	if(_writeCommand("AT+CMQTTSTOP\r\n", 5, "OK", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTSTOP (stop MQTT service)"));
        //return false;
    }

	return true;
}

int16_t HologramSIMCOM::mqttInitMessage(uint8_t client, uint8_t messageNr, uint8_t type, uint8_t packetNr, uint32_t size) {
//	_SENDBUFFER = SEND_BUFFER;
//	mySerial->print("buffer: ");mySerial->println(size < (uint32_t)_SENDBUFFER ? size : _SENDBUFFER);
//	return _SENDBUFFER;

	// set topic
	if (1) {
		String topic = String(client) + "/" + String(messageNr) + "/" + String(type) + "/" + String(packetNr);
		String cmd = "AT+CMQTTTOPIC=0," + String(topic.length()) + "\r\n";
		if(_writeCommand(cmd.c_str(), 5, ">", "ERROR") != 2) {
			mySerial->println(F("ERROR: failed at +CMQTTTOPIC (set topic)"));
			return -1;
		}

		_writeStringToSerial(topic.c_str(), true);
		delay(20);
	}

    if(_writeCommand("\r\n", 60, "OK", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed to set topic"));
        return -1;
    }

    _SENDBUFFER = SEND_BUFFER;

    // start payload
    if (1) {
    	String cmd = "AT+CMQTTPAYLOAD=0," + String(size < (uint32_t)_SENDBUFFER ? size : _SENDBUFFER) + "\r\n";
    	if(_writeCommand(cmd.c_str(), 5, ">", "ERROR") != 2) {
    		mySerial->println(F("ERROR: failed at +CMQTTPAYLOAD (start payload)"));
    		return -1;
    	}
    }

	return _SENDBUFFER;
}

int16_t HologramSIMCOM::mqttAppendPayload(const byte *payload, uint32_t len) {
	if (len > (uint32_t)_SENDBUFFER) {
		return -1;
	}
	_SENDBUFFER -= len;

	_writeBytesToSerial(payload, len, true);
//	mySerial->print("payload: ");
//	mySerial->write(payload, len);
//	mySerial->println();

	return _SENDBUFFER;
}

bool HologramSIMCOM::mqttPublish() {
//	mySerial->println("publish");
//	return true;

	// workaround: need to block here until we're ready to send
	_writeCommand("AT\r\n", 1, "OK", "ERROR");

	// publish message
	if(_writeCommand("AT+CMQTTPUB=0,0,60\r\n", 60, "+CMQTTPUB: 0,0", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTPUB=0,0,60 (publish message)"));
        return false;
    }

	return true;
}

bool HologramSIMCOM::mqttPublish(uint8_t client, uint8_t messageNr, uint8_t type, uint8_t packetNr,
		const byte *payload, uint32_t len) {
	if (mqttInitMessage(client, messageNr, type, packetNr, len) == -1) return false;
	if (mqttAppendPayload(payload, len) == -1) return false;
	return mqttPublish();
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

    if (Serial.available() > 0) {
        delay(20); // allow for buffer to build
        while ( Serial.available() > 0 ) { // move serial buffer into global String
            char r = Serial.read();
            _SERIALBUFFER += r;

            if (_SERIALBUFFER.endsWith("\n" )) { // we read the serial one line at a time
                _SERIALBUFFER.replace("\r","");
            }
        }

//        _checkIfInbound();

        if(_DEBUG == 1) {
            mySerial->print(F("DEBUG: Modem Serial Buffer = "));
            mySerial->println(_SERIALBUFFER);
            if(_MESSAGEBUFFER.length() > 0) {
                mySerial->print(F("DEBUG: Message Buffer = "));
                mySerial->println(_MESSAGEBUFFER);
            }
        }
    }
}

//void HologramSIMCOM::_checkIfInbound() {
//    // Check for inbound message and throw incoming into _MESSAGEBUFFER
//    if(_SERIALBUFFER.indexOf("+CLIENT: ") != -1) {
//
//    	int pos = _SERIALBUFFER.indexOf("+CLIENT: ") + strlen("+CLIENT: ");
//    	int link = _SERIALBUFFER.substring(pos, pos + 1).toInt();
//
//    	_MESSAGEBUFFER = _SERIALBUFFER;
//
//        // this is a little wonky, need to override _MODEMSTATE to execute
//        _MODEMSTATE = 1;
//        _sendResponse(link, "OK");
//        _MODEMSTATE = 0;
//
//    }
//}

void HologramSIMCOM::_initSerial(const uint32_t baud) {
	Serial.begin(115200);

    String b(baud);
    String s = "Configuring to " + b + " baud";
    mySerial->println(s);

    s = "AT+IPR=" + b;
    Serial.println(s.c_str()); // Set baud rate
    Serial.end();
    Serial.begin(baud);
}

void HologramSIMCOM::_stopSerial() {
	Serial.end();
}

void HologramSIMCOM::_writeStringToSerial(const char* string, bool hide) {
    // Note: this expects you to check state before calling
    // IMPORTANT: I want to tightly control writing to serialHologram,
    // this is the only function allowed to do it

    if(_DEBUG == 1 && !hide) {
        mySerial->print(F("DEBUG: Write Modem Serial = "));
        mySerial->println(string);
    }

    Serial.write(string); // send command
}

void HologramSIMCOM::_writeBytesToSerial(const byte* bytes, uint32_t len, bool hide) {
    if(_DEBUG == 1 && !hide) {
        mySerial->print(F("DEBUG: Write Modem Serial = "));
        mySerial->write(bytes, len);
        mySerial->println();
    }

    Serial.write(bytes, len); // send command
}

void HologramSIMCOM::_writeCommand(const char* command, const unsigned long timeout) {
    if(_MODEMSTATE == 1) {//check if serial is available
        unsigned start = millis();
        _MODEMSTATE = 0; // set state as busy

        _writeStringToSerial(command); // send command to modem

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

        _writeStringToSerial(command); // send command to modem

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
            mySerial->print(F("ERROR: Error resp when calling "));
            mySerial->println(command);
            return 1;
        } else { // timed out
            mySerial->print(F("ERROR: Timeout when calling "));
            mySerial->print(command);
            mySerial->print(F(" | elapsed ms = "));
            mySerial->println(timeoutTime);
            return 0;
        }
    } else {
        mySerial->println(F("ERROR: Modem not available"));
        return 1;
    }
}

bool HologramSIMCOM::_connectNetwork() {
    bool connection = false;

    while(1) {
        if(cellStrength() == 0) {
            mySerial->println(F("ERROR: no signal"));
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
            	mySerial->println(F("ERROR: failed at +NETOPEN (1)"));
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
        	mySerial->println(F("ERROR: failed at +NETOPEN (2)"));
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
        mySerial->println(F("ERROR: failed to initiaite CIPSEND"));
        return false;
    }

    // send data message to server
    _writeStringToSerial(data);
    if(_writeCommand("\r\n", 60, "OK", "ERROR") != 2) {
        mySerial->println(F("ERROR: failed to send data message"));
        return false;
    }

    return true;
}
