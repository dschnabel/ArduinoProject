#include "Arduino.h"
#include "RH_RF95.h"
#include "radio.h"
#include "common.h"

uint8_t _get_code() {
	if (Serial.available()) {
		Serial.read();
		return CODE_READ_SERIAL;
	}

	uint8_t radio_code = radio_available();
	if (radio_code) {
		return radio_code;
	}
	return 0;
}

void setup()
{
	Serial.begin(115200);
	radio_setup_server();
}

void loop()
{
	uint8_t code = _get_code();

	switch (code) {
	case CODE_READ_SERIAL:
		radio_set_message(CODE_CAPTURE_PHOTO);
		if (!radio_send()) {
			Serial.println("notify failure");
		}
		break;
	case CODE_PHOTO_DATA:
		radio_message_t *m = radio_get_message();
		for (int i = 0; i < m->len-1; i++) {
			Serial.write(m->payload.data[i]);
			delayMicroseconds(15);
		}
		break;
	}
}
