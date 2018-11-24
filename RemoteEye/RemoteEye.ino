#include "Arduino.h"
#include "cam.h"
#include "base64.h"
#include "HologramSIMCOM.h"
#include "secrets.h"

#define TX_PIN 2
#define RX_PIN 3

HologramSIMCOM Hologram(TX_PIN, RX_PIN, HOLOGRAM_KEY);

bool runOnce = true;

byte _get_code() {
	if (Serial.available()) {
		int c = Serial.read();
		if (c == 'f' || c == 0x10) {
			return 1;
		}
	}

	return 0;
}

void setup() {

  Serial.begin(115200);
  while(!Serial);

//  Hologram.debug();
//
//  if (!Hologram.begin(8888)) {
//	  while (1) {
//		  delay(10000);
//	  }
//  }
//
//  switch (Hologram.cellStrength()) {
//      case 0: Serial.println("No signal");break;
//      case 1: Serial.println("Very poor signal strength"); break;
//      case 2: Serial.println("Poor signal strength"); break;
//      case 3: Serial.println("Good signal strength"); break;
//      case 4: Serial.println("Very good signal strength"); break;
//      case 5: Serial.println("Excellent signal strength");
//  }

//  Hologram.send(0, 0, 0, 1, "arduino");

//  char decoded[100];
//  strcpy(encoded, all.c_str());
//  base64_decode(decoded, encoded, strlen(encoded));
//  Serial.println(decoded);

  camera_setup();

}


void loop()
{
	byte code = _get_code();
	switch (code) {
		case 1:
			camera_capture_photo();

			byte data[128];
			char encoded[base64_enc_len(sizeof(data))];
			byte len;
			encode_control control;
			control.index = 0;

			while ((len = camera_read_captured_data(data, sizeof(data))) > 0) {
				base64_encode(&control, encoded, data, sizeof(data));
				Serial.println(encoded);
				delayMicroseconds(15);
			}
			base64_encode_finalize(&control, encoded);
			Serial.println(encoded);

			camera_set_capture_done();
			break;
	}
}
