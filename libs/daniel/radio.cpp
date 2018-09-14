#include <RHReliableDatagram.h>
#include <RH_RF95.h>

#include "radio.h"

#define CLIENT_ADDRESS 212
#define SERVER_ADDRESS 65

RH_RF95 radio_driver;
RHReliableDatagram radio_manager(radio_driver, 1);

radio_message_t radio_message;
uint8_t partner_address;

void _radio_setup(uint8_t addr1, uint8_t addr2) {
	if (!radio_manager.init()) {
		Serial.println("radio init failed");
	} else {
		Serial.println("radio init success");
	}
	radio_manager.setThisAddress(addr1);
	partner_address = addr2;
	memset(&radio_message, 0, sizeof(radio_message));
}

void radio_setup_server() {
	_radio_setup(SERVER_ADDRESS, CLIENT_ADDRESS);
}

void radio_setup_client() {
	_radio_setup(CLIENT_ADDRESS, SERVER_ADDRESS);
}

uint8_t radio_available() {
	uint8_t ret = 0;
	if (radio_manager.available()) {
		uint8_t from;
		radio_message.len = sizeof(radio_payload_t);
		if (radio_manager.recvfromAck((uint8_t*) &radio_message.payload, &radio_message.len, &from)) {
			if (from == partner_address) {
				ret = radio_message.payload.type;
			}
		} else {
			Serial.println("radio_listen recvfromAck error");
		}
	}
	return ret;
}

bool radio_send() {
	if (radio_manager.sendtoWait((uint8_t*) &radio_message.payload, radio_message.len, partner_address)) {
		return true;
	}
	return false;
}

void radio_set_message(uint8_t type, uint8_t len, uint8_t* buffer) {
	radio_message.payload.type = type;
	if (len > 0 && buffer != NULL) {
		memcpy(radio_message.payload.data, buffer, len);
		radio_message.len = len;
	} else {
		radio_message.len = 1;
	}
}

radio_message_t* radio_get_message() {
	return &radio_message;
}
