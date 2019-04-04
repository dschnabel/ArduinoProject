/**
 * @file       HologramSIMCOM.cpp
 * @author     Ben Strahan
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2017 Ben Strahan
 * @date       Apr 2017
**/

#include "HologramSIMCOM.h"
#include "secrets.h"

#define UMTS_3G 0
#define LTE_4G 1

/*--------------------------------------------------------
PUBLIC
---------------------------------------------------------*/

bool HologramSIMCOM::begin(const uint32_t baud) {
    _initSerial(baud);
    int timeout = 30000;
    while(!isOn() && timeout > 0) {
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
    if(_writeCommand(baud_command, 1, F("OK"), F("ERROR")) != 2) {
    	mySerial->println(F("ERROR: begin() failed at +IPR"));
    	return false;
    }

    // Check if SIM is ready
    if(_writeCommand(F("AT+CPIN?\r\n"), 5, F("OK"), F("ERROR")) != 2) {
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
    if(_writeCommand(F("AT+CSSLCFG=\"sslversion\",0,3\r\n"), 5, F("OK"), F("ERROR")) != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"sslversion\""));
    	return false;
    }

    // set auth mode
    if(_writeCommand(F("AT+CSSLCFG=\"authmode\",0,2\r\n"), 5, F("OK"), F("ERROR")) != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"authmode\""));
    	return false;
    }

    // set SSL CA
    if(_writeCommand(F("AT+CSSLCFG=\"cacert\",0,\"ca_cert.pem\"\r\n"), 5, F("OK"), F("ERROR")) != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"cacert\",0,\"ca_cert.pem\""));
    	return false;
    }

    // set SSL Client Cert
    if(_writeCommand(F("AT+CSSLCFG=\"clientcert\",0,\"client_cert.pem\"\r\n"), 5, F("OK"), F("ERROR")) != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"clientcert\",0,\"client_cert.pem\""));
    	return false;
    }

    // set SSL Client Key
    if(_writeCommand(F("AT+CSSLCFG=\"clientkey\",0,\"client_key.pem\"\r\n"), 5, F("OK"), F("ERROR")) != 2) {
    	mySerial->println(F("ERROR: begin() failed at +CSSLCFG=\"clientkey\",0,\"client_key.pem\""));
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
        switch(_writeCommand(server_command, 5, F("OK"), F("ERROR"))) {
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
    if(_writeCommand(F("AT+CSQ\r\n"), 1, F("+CSQ:"), F("ERROR")) == 2) {
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
	if (_writeCommand(F("AT+CMQTTCONNECT?\r\n"), 1, F("+CMQTTCONNECT: 0,"), F("ERROR")) == 2) {
		mySerial->println(F("already connected"));
		return true;
	}

	// network registration & MQTT service start
    int tries = 2;
    byte mode = UMTS_3G;
    bool registered = true;
    while((!registered || _writeCommand(F("AT+CMQTTSTART\r\n"), 5, F("+CMQTTSTART: 0"), F("ERROR")) != 2) && tries > 0) {
    	if (!_registerNetwork(mode)) {
    		registered = false;
    	} else {
    		registered = true;
    	}

    	if (mode == UMTS_3G) {
    		mode = LTE_4G;
    	} else if (mode == LTE_4G) {
    		mode = UMTS_3G;
    	}

    	tries--;
    }
    if (tries <= 0) {
        mySerial->println(F("ERROR: mqttConnect() failed, could not establish connection"));
        return false;
    }

    // acquire client that will connect to an SSL/TLS MQTT server
	if(_writeCommand(F("AT+CMQTTACCQ=0,\"client1\",1\r\n"), 5, F("OK"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTACCQ=0,\"client1\" (acquire client)"));
        return false;
    }

	// set context to be used in client connection
	if(_writeCommand(F("AT+CMQTTSSLCFG=0,0\r\n"), 5, F("OK"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTSSLCFG=0,0 (set context)"));
        return false;
    }

	// don't check UTF-8 encoding
	if(_writeCommand(F("AT+CMQTTCFG=\"checkUTF8\",0,0\r\n"), 5, F("OK"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTCFG=0,0 (disable checking for UTF-8)"));
        return false;
    }

	// connect to server
	if(_writeCommand(F("AT+CMQTTCONNECT=0,\"" AWS_IOT_ENDPOINT "\",660,1\r\n"), 10, F("+CMQTTCONNECT: 0,0"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: begin() at +CMQTTCONNECT (connect to server)"));
        return false;
    }

	return true;
}

bool HologramSIMCOM::mqttDisconnect() {
	// disconnect from server
	if(_writeCommand(F("AT+CMQTTDISC=0,120\r\n"), 10, F("+CMQTTDISC: 0,0"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTDISC=0,120 (disconnect from server)"));
        //return false;
    }

	// release client
	if(_writeCommand(F("AT+CMQTTREL=0\r\n"), 5, F("OK"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTREL=0 (release client)"));
        //return false;
    }

	// stop MQTT service
	if(_writeCommand(F("AT+CMQTTSTOP\r\n"), 5, F("OK"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTSTOP (stop MQTT service)"));
        //return false;
    }

	return true;
}

int16_t HologramSIMCOM::mqttInitMessage(uint8_t client, uint8_t messageNr, uint8_t type, uint8_t packetNr, uint32_t size) {
	// set topic
	if (1) {
		char topic[20];
		memset(topic, 0, sizeof(topic));
		strcpy(topic, "s/");

		if (1) {
			char cstr[4];
			utoa(client, cstr, 10); strcat(topic, cstr); strcat(topic, "/");
			utoa(messageNr, cstr, 10); strcat(topic, cstr); strcat(topic, "/");
			utoa(type, cstr, 10); strcat(topic, cstr); strcat(topic, "/");
			utoa(packetNr, cstr, 10); strcat(topic, cstr);
		}

		char cmd[25];
		memset(cmd, 0, sizeof(cmd));

		if (1) {
			char cstr[4];
			utoa(strlen(topic), cstr, 10);
			strcpy(cmd, "AT+CMQTTTOPIC=0,"); strcat(cmd, cstr); strcat(cmd, "\r\n");
		}

		if(_writeCommand(cmd, 5, F(">"), F("ERROR")) != 2) {
			mySerial->println(F("ERROR: failed at +CMQTTTOPIC (set topic)"));
			return -1;
		}

		_writeStringToSerial(topic, true);
		delay(20);
	}

    _SENDBUFFER = SEND_BUFFER;

    // start payload
    if (1) {
    	char cmd[30];
    	memset(cmd, 0, sizeof(cmd));

		if (1) {
			char cstr[8];
			utoa(size < (uint32_t)_SENDBUFFER ? size : _SENDBUFFER, cstr, 10);
			strcpy(cmd, "AT+CMQTTPAYLOAD=0,"); strcat(cmd, cstr); strcat(cmd, "\r\n");
		}

    	if(_writeCommand(cmd, 5, F(">"), F("ERROR")) != 2) {
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

	return _SENDBUFFER;
}

bool HologramSIMCOM::mqttPublish() {
	// workaround: need to block here until we're ready to send
	isOn();

	// publish message
	if(_writeCommand(F("AT+CMQTTPUB=0,0,60\r\n"), 60, F("+CMQTTPUB: 0,0"), F("ERROR")) != 2) {
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

bool HologramSIMCOM::mqttSubscribe(uint8_t client) {
	char topic[7];
	memset(topic, 0, sizeof(topic));
	strcpy(topic, "c/");

	if (1) {
		char cstr[3];
		utoa(client, cstr, 10); strcat(topic, cstr);
	}

	char cmd[30];
	memset(cmd, 0, sizeof(cmd));

	if (1) {
		char cstr[4];
		utoa(strlen(topic), cstr, 10);
		strcpy(cmd, "AT+CMQTTSUB=0,"); strcat(cmd, cstr); strcat(cmd, ",1\r\n");
	}

	if(_writeCommand(cmd, 10, F(">"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTSUB (subscribe to topic)"));
        return false;
    }

	_writeStringToSerial(topic);
	delay(20);

    if(_writeCommand(F("\r\n"), 60, F("+CMQTTSUB: 0,0"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed to subscribe"));
        return false;
    }

    _LISTENING = true;

	return true;
}

bool HologramSIMCOM::mqttUnsubscribe(uint8_t client) {
	char topic[7];
	memset(topic, 0, sizeof(topic));
	strcpy(topic, "c/");

	if (1) {
		char cstr[3];
		utoa(client, cstr, 10); strcat(topic, cstr);
	}

	char cmd[30];
	memset(cmd, 0, sizeof(cmd));

	_LISTENING = false;

	if (1) {
		char cstr[4];
		utoa(strlen(topic), cstr, 10);
		strcpy(cmd, "AT+CMQTTUNSUB=0,"); strcat(cmd, cstr); strcat(cmd, ",0\r\n");
	}

	if(_writeCommand(cmd, 10, F(">"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed at +CMQTTUNSUB (unsubscribe from topic)"));
        return false;
    }

	_writeStringToSerial(topic);
	delay(20);

    if(_writeCommand(F("\r\n"), 60, F("+CMQTTUNSUB: 0,0"), F("ERROR")) != 2) {
        mySerial->println(F("ERROR: failed to unsubscribe"));
        return false;
    }

	return true;
}

bool HologramSIMCOM::mqttIsListening() {
	return _LISTENING;
}

bool HologramSIMCOM::mqttBufferState(byte *state, uint16_t *reportedSize, char *buf, byte size, byte *index) {
	bool done = false;
	char c;
	if ((c = Serial.read()) != -1) {
//		mySerial->print(c);
		if (*state < 7) {
			if (*state == 0) {if (c == 'P') *state = 1; else *state = 0;}
			else if (*state == 1) {if (c == 'A') *state = 2; else *state = 0;}
			else if (*state == 2) {if (c == 'Y') *state = 3; else *state = 0;}
			else if (*state == 3) {if (c == 'L') *state = 4; else *state = 0;}
			else if (*state == 4) {if (c == 'O') *state = 5; else *state = 0;}
			else if (*state == 5) {if (c == 'A') *state = 6; else *state = 0;}
			else if (*state == 6) {if (c == 'D') *state = 7; else *state = 0;}
		} else if (*state == 7 && c == ',') {
			// proceed until we reach a comma
			*state = 8;
		} else if (*state == 8) {
			// read length of payload
			if (isDigit(c)) {
				buf[*index] = c;
				*index += 1;
			} else {
				*state = 9;
				*reportedSize = atol(buf);
				memset(buf, 0, *index);
				*index = 0;
			}
		} else if (*state == 9 && c == '\n') {
			// proceed to new line for payload
			*state = 10;
		} else if (*state == 10) {
			// read payload into buffer
			if (*index < size - 1 && *index < *reportedSize) {
				buf[*index] = c;
				*index += 1;
			} else {
				buf[*index] = '\0';
				done = true;
				*index = 0;
				*state = 0;
				_readSerial(); // this will flush the remaining bytes from the buffer
			}
		}
	}
	return done;
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

time_t HologramSIMCOM::updateTime() {
	if (_writeCommand(F("AT+CCLK?\r\n"), 2, F("+CCLK:"), F("ERROR")) != 2) {
		mySerial->println(F("ERROR: Could not get time"));
		return 0;
	}

	if (1) {
		String time = _SERIALBUFFER.substring(_SERIALBUFFER.indexOf(": ")+3).substring(0, 20);
		setTime(time.substring(9,11).toInt(), time.substring(12,14).toInt(), time.substring(15,17).toInt(),
				time.substring(6,8).toInt(), time.substring(3,5).toInt(), time.substring(0,2).toInt());
		setTimeZoneAdjustment((time.substring(17,20).toInt() / 4) * 60 * 60);
	}

	return now();
}

bool HologramSIMCOM::isOn() {
	return (_writeCommand(F("AT\r\n"), 1, F("OK"), F("ERROR")) == 2);
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
//    //   Check for inbound message and throw incoming into _MESSAGEBUFFER
//	if(_SERIALBUFFER.indexOf("+CMQTTRXSTART: ") != -1) {
//
//    	int pos = _SERIALBUFFER.indexOf("+CLIENT: ") + strlen("+CLIENT: ");
//    	int link = _SERIALBUFFER.substring(pos, pos + 1).toInt();
//
//    	_MESSAGEBUFFER = _SERIALBUFFER;
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
    if(_DEBUG == 1 && !hide) {
        mySerial->print(F("DEBUG: Write Modem Serial = "));
        mySerial->println(string);
    }

    Serial.write(string); // send command
}

void HologramSIMCOM::_writeStringToSerial(const __FlashStringHelper *string, bool hide) {
	_writeStringToSerial(reinterpret_cast<PGM_P>(string), hide);
}

void HologramSIMCOM::_writeBytesToSerial(const byte* bytes, uint32_t len, bool hide) {
    if(_DEBUG == 1 && !hide) {
        mySerial->print(F("DEBUG: Write Modem Serial = "));
        mySerial->write(bytes, len);
        mySerial->println();
    }

    Serial.write(bytes, len); // send command
}

int HologramSIMCOM::_writeCommand(const char* command, const unsigned long timeout,
		const __FlashStringHelper *successResp, const __FlashStringHelper *errorResp) {
	char ok[20]; _pgm_get(successResp, ok, sizeof(ok));
	char error[20]; _pgm_get(errorResp, error, sizeof(error));

    unsigned int timeoutTime;
    if(_MODEMSTATE == 1) {//check if serial is available
    	_readSerial(); // flush any old messages

        unsigned long start = millis();
        _MODEMSTATE = 0; // set state as busy

        _writeStringToSerial(command); // send command to modem

        while(timeout * 1000 > millis() - start) { // wait for timeout to complete
            _readSerial();
            if(strstr(_SERIALBUFFER.c_str(), ok)  != NULL || strstr(_SERIALBUFFER.c_str(), error)  != NULL) {
                break;
            }
        }

        if(timeout * 1000 > millis() - start){
            timeoutTime = (millis() - start) / 1000;
        } else {
            timeoutTime = 0;
        }

        _MODEMSTATE = 1; // set state as available

        if(strstr(_SERIALBUFFER.c_str(), ok)  != NULL) {
            return 2;
        } else if(strstr(_SERIALBUFFER.c_str(), error)  != NULL) {
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

int HologramSIMCOM::_writeCommand(const __FlashStringHelper *command, const unsigned long timeout,
		const __FlashStringHelper *successResp, const __FlashStringHelper *errorResp) {
	char buf[100];
	_pgm_get(command, buf, sizeof(buf));
	return _writeCommand(buf, timeout, successResp, errorResp);
}

bool HologramSIMCOM::_registerNetwork(byte mode) {
	_writeCommand(F("AT+COPS?\r\n"), 1, F("+COPS: 1,0,"), F("ERROR"));
	_writeCommand(F("AT+CREG?\r\n"), 1, F("OK"), F("ERROR"));
	_writeCommand(F("AT+CEREG?\r\n"), 1, F("OK"), F("ERROR"));

	// force deregister
	_writeCommand(F("AT+COPS=2\r\n"), 1, F("OK"), F("ERROR"));
	delay(3);

	if (mode == UMTS_3G) {
		if(_writeCommand(F("AT+COPS=1,2,\"302720\",2\r\n"), 60, F("OK"), F("+CME ERROR")) == 2) {
			delay(3000);
			return true;
		}
	} else if (mode == LTE_4G) {
		if(_writeCommand(F("AT+COPS=1,2,\"302720\",7\r\n"), 60, F("OK"), F("+CME ERROR")) == 2) {
			delay(3000);
			return true;
		}
	}

	if (mode == UMTS_3G) {
		mySerial->println(F("ERROR: Could not connect to 3G network"));
	} else if (mode == LTE_4G) {
		mySerial->println(F("ERROR: Could not connect to 4G network"));
	}

	return false;
}

size_t HologramSIMCOM::_pgm_get(const __FlashStringHelper *str, char *buf, size_t size) {
	  PGM_P p = reinterpret_cast<PGM_P>(str);
	  size_t n = 0;
	  while (n < size) {
	    unsigned char c = pgm_read_byte(p++);
	    if (c == 0) break;
	    buf[n++] = c;
	  }
	  buf[n] = '\0';
	  return n;
}
