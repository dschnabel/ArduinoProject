#include "Arduino.h"
#include "cam.h"
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
	Serial.begin(115200);
	camera_setup();
	radio_setup_client();
}

void loop()
{
	byte code = _get_code();

	switch (code) {
	case CODE_READ_SERIAL:
	case CODE_CAPTURE_PHOTO:
		radio_message_t *m = radio_get_message();

		camera_capture_photo();

		while ((m->len = camera_read_captured_data(m->payload.data, sizeof(m->payload.data))) > 0) {
			m->payload.type = CODE_PHOTO_DATA;
			m->len++; // for type
			if (!radio_send()) {
				Serial.println("data sent failure");
			}
//			for (int i = 0; i < m->len; i++) {
//				Serial.write(m->payload.buffer[i]);
//				delayMicroseconds(15);
//			}
		}

		camera_set_capture_done();
		break;
	}
}
