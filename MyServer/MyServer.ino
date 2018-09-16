#include "Arduino.h"
#include "RH_RF95.h"
#include "radio.h"
#include "common.h"

byte _get_code() {
	if (Serial.available()) {
		int c = Serial.read();
		if (c == 'f' || c == 0x10) {
			return CODE_READ_SERIAL;
		}
	}

	byte radio_code = radio_available();
	if (radio_code) {
		return radio_code;
	}
	return 0;
}

void setup()
{
	Serial.begin(921600);
	radio_setup_server();
}

void loop()
{
	byte code = _get_code();

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
