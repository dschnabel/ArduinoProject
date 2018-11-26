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

  camera_setup();

  Hologram.debug();

  if (!Hologram.begin(19200)) {
	  while (1) {
		  delay(10000);
	  }
  }

  switch (Hologram.cellStrength()) {
      case 0: Serial.println("No signal");break;
      case 1: Serial.println("Very poor signal strength"); break;
      case 2: Serial.println("Poor signal strength"); break;
      case 3: Serial.println("Good signal strength"); break;
      case 4: Serial.println("Very good signal strength"); break;
      case 5: Serial.println("Excellent signal strength");
  }

//  char decoded[100];
//  strcpy(encoded, all.c_str());
//  base64_decode(decoded, encoded, strlen(encoded));
//  Serial.println(decoded);
}


void loop()
{
	byte code = _get_code();
	switch (code) {
		case 1:
			camera_capture_photo();

			byte data[64];
			byte len = sizeof(data);
			char encoded[base64_enc_len(sizeof(data))];
			encode_control control;
			control.index = 0;
			byte messageNr = 0, packetNr = 0;

			Hologram.openSocket();
			int bufferRemaining = Hologram.sendOpenConnection(0, messageNr, packetNr++, 1);
			if (bufferRemaining == -1) {
				break;
			}

			// fetch camera data, encode as base64 and send to modem
			while ((len = camera_read_captured_data(data, len)) > 0) {
				int encLen = base64_encode(&control, encoded, data, len);
				String e = String(encoded);

				if (bufferRemaining < encLen) {
					if (bufferRemaining > 0) {
						String b = e.substring(0, bufferRemaining);
						e = e.substring(bufferRemaining);
						Hologram.sendAppendData(b.c_str());
					}

					Hologram.sendSendOff();
					bufferRemaining = Hologram.sendOpenConnection(0, messageNr, packetNr++, 1);
				}
				if (bufferRemaining >= encLen) {
					bufferRemaining = Hologram.sendAppendData(e.c_str());
				}

				delayMicroseconds(15);
			}

			// encode left-over data and send to modem
			int encLen = base64_encode_finalize(&control, encoded);
			if (bufferRemaining < encLen) {
				Hologram.sendSendOff();
				bufferRemaining = Hologram.sendOpenConnection(0, messageNr, packetNr++, 1);
			}
			if (bufferRemaining >= encLen) {
				Hologram.sendAppendData(encoded);
			}
			Hologram.sendSendOff();
			Hologram.closeSocket();

			camera_set_capture_done();
			break;
	}
}
