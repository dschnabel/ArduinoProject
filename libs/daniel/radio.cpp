#include <RHReliableDatagram.h>
#include <RH_RF95.h>

#include "radio.h"

#define CLIENT_ADDRESS 212
#define SERVER_ADDRESS 65

RH_RF95 radio_driver;
RHReliableDatagram radio_manager(radio_driver, 1);

radio_message_t radio_message;
byte partner_address;

#if RADIO_USE_AES
#include <AES.h>
#include "secrets.h"
AES radio_aes;
byte radio_aes_key[] = RADIO_AES_KEY;
unsigned long long int radio_aes_iv = RADIO_AES_IV;
#endif

void _radio_setup(byte addr1, byte addr2) {
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

byte radio_available() {
	byte ret = 0;
	if (radio_manager.available()) {
		byte from;
		radio_message.len = sizeof(radio_payload_t);
		if (radio_manager.recvfromAck((byte*) &radio_message.payload, &radio_message.len, &from)) {
			if (from == partner_address) {

#if RADIO_USE_AES
				// only decrypt if we have payload data
				if (radio_message.len > 1) {
					byte check[radio_message.len];
					radio_aes.set_IV(radio_aes_iv);
					radio_aes.do_aes_decrypt((byte*) &radio_message.payload, radio_message.len, check, radio_aes_key, 128);
					memcpy(&radio_message.payload, check, radio_message.len);
				}
#endif

				ret = radio_message.payload.type;
			} else {
				memset(&radio_message, 0, sizeof(radio_message));
			}
		} else {
			Serial.println("radio_listen recvfromAck error");
		}
	}
	return ret;
}

bool radio_send() {
#if RADIO_USE_AES
	// only encrypt if we have payload data
	if (radio_message.len > 1) {
		byte paddedLength;
		byte mod = radio_message.len % N_BLOCK;
		if (mod == 0) {
			paddedLength = radio_message.len;
		} else {
			paddedLength = radio_message.len + N_BLOCK - mod;
		}
		byte cipher[paddedLength];
		radio_aes.set_IV(radio_aes_iv);
		radio_aes.do_aes_encrypt((byte*) &radio_message.payload, (int)radio_message.len, cipher, radio_aes_key, 128);
		memcpy(&radio_message.payload, cipher, paddedLength);
		radio_message.len = paddedLength;
	}
#endif

	if (radio_manager.sendtoWait((byte*) &radio_message.payload, radio_message.len, partner_address)) {
		return true;
	}
	return false;
}

void radio_set_message(byte type, byte len, byte* buffer) {
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
