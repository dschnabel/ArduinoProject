#include "Arduino.h"
#include <ArduCAM.h>
#include "cam.h"
#include "HologramSIMCOM.h"
#include "Time.h"

#define TX_PIN 3
#define RX_PIN 2
SoftwareSerial mySerial(TX_PIN,RX_PIN);

HologramSIMCOM Hologram(&mySerial);

uint16_t mqttReportedTextSize = 0;
char mqttResponseString[100]; // TODO adjust size as needed (e.g. max struct size)
byte r_index = 0;
byte state = 0;

typedef struct {
	time_t timestamps[5];
} configuration;

#define CLIENT 0

#define ACTION_CONNECT 1
#define ACTION_DISCONNECT 2
#define ACTION_PHOTO 3
#define ACTION_TEST 4
#define ACTION_SUBSCRIBE 5
#define ACTION_UNSUBSCRIBE 6

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

byte _get_code() {
	if (mySerial.available()) {
		int c = mySerial.read();
		switch (c) {
		case 'c': return ACTION_CONNECT;
		case 'd': return ACTION_DISCONNECT;
		case 'p': return ACTION_PHOTO;
		case 't': return ACTION_TEST;
		case 's': return ACTION_SUBSCRIBE;
		case 'u': return ACTION_UNSUBSCRIBE;
		}
	}

	return 0;
}

void setup() {

  mySerial.begin(57600);
  while(!mySerial);

  camera_setup(OV2640_160x120);
//  camera_setup(OV2640_640x480);

  Hologram.debug();

  if (!Hologram.begin(57600)) {
	  while (1) {
		  delay(10000);
	  }
  }

  switch (Hologram.cellStrength()) {
      case 0: mySerial.println(F("No signal"));break;
      case 1: mySerial.println(F("Very poor signal strength")); break;
      case 2: mySerial.println(F("Poor signal strength")); break;
      case 3: mySerial.println(F("Good signal strength")); break;
      case 4: mySerial.println(F("Very good signal strength")); break;
      case 5: mySerial.println(F("Excellent signal strength"));
  }

  memset(mqttResponseString, 0, sizeof(mqttResponseString));
  mySerial.print(F("Free RAM: "));
  mySerial.println(freeRam());
}


void loop()
{
	byte code = _get_code();
	switch (code) {
	case ACTION_CONNECT: {
		Hologram.mqttConnect();
		break;
	}
	case ACTION_DISCONNECT: {
		Hologram.mqttDisconnect();
	}
	break;
	case ACTION_TEST: {
		// add code here
	}
	break;
	case ACTION_PHOTO: {
		camera_capture_photo();
		byte messageNr = 0, packetNr = 0;

		// get UNIX timestamp
		time_t timestamp = Hologram.getTimestamp();

		// put as many variables as possible in new context to prevent stack from being exhausted too soon
		if (1) {

			byte data[32];
			byte len = sizeof(data);
			int32_t photo_size = camera_get_photo_size()* 0.80;
			int32_t sent = 0;

			int bufferRemaining = Hologram.mqttInitMessage(CLIENT, messageNr, 1, packetNr++, photo_size);
			if (bufferRemaining == -1) break;

			// fetch camera data and send to modem
			while ((len = camera_read_captured_data(data, len)) > 0) {
				byte index = 0;

				if (bufferRemaining < len) {
					if (bufferRemaining > 0) {
						Hologram.mqttAppendPayload(&data[index], bufferRemaining);
						photo_size -= bufferRemaining;
						len -= bufferRemaining;
						index = bufferRemaining;
						sent += bufferRemaining;
					}

					if (photo_size < SEND_BUFFER) {
						photo_size = (camera_get_photo_size() - sent) * 0.5;
						if (photo_size < len) photo_size = len;
					}

					if (!Hologram.mqttPublish()) break;
					bufferRemaining = Hologram.mqttInitMessage(CLIENT, messageNr, 1, packetNr++, photo_size);
					if (bufferRemaining == -1) break;
				}

				if (photo_size < len) {
					if (photo_size > 0) {
						Hologram.mqttAppendPayload(&data[index], photo_size);
						len -= photo_size;
						index = photo_size;
						sent += photo_size;
					}

					if (!Hologram.mqttPublish()) break;
					photo_size = (camera_get_photo_size() - sent) * 0.5;
					if (photo_size < len) photo_size = len;
					bufferRemaining = Hologram.mqttInitMessage(CLIENT, messageNr, 1, packetNr++, photo_size);
					if (bufferRemaining == -1) break;
					mySerial.println(photo_size);
				}

				if (bufferRemaining >= len) {
					bufferRemaining = Hologram.mqttAppendPayload(&data[index], len);
					if (bufferRemaining == -1) break;
					photo_size -= len;
					sent += len;
				}

				len = sizeof(data);

				delayMicroseconds(15);
			}

			// padding to fill up send buffer
			mySerial.print(F("padding: "));mySerial.println(photo_size);
			while (photo_size > 0) {
				len = photo_size < sizeof(data) ? photo_size : sizeof(data);

				uint8_t i;
				for (i = 0; i < len; i++) data[i] = 0xFF;
				Hologram.mqttAppendPayload(data, len);

				photo_size -= len;
			}

		} // end of context block

		Hologram.mqttPublish();

		// notify done
		Hologram.mqttPublish(CLIENT, messageNr, 2, packetNr++, (const byte*)&timestamp, sizeof(time_t));

		camera_set_capture_done();
	}
	break;
	case ACTION_SUBSCRIBE: {
		// note: a bug in SIMCOM module prevents a message from being received if we publish a message
		// AFTER subscribing to a topic. Do not publish between subscribing and unsubscribing.
		Hologram.mqttSubscribe(CLIENT);
	}
	break;
	case ACTION_UNSUBSCRIBE: {
		Hologram.mqttUnsubscribe(CLIENT);
	}
	break;
	}

	if (Hologram.mqttIsListening()) {
		bool done = Hologram.mqttBufferState(&state, &mqttReportedTextSize,
				mqttResponseString, sizeof(mqttResponseString), &r_index);

		if (done) {
			configuration c;
			memset(&c, 0, sizeof(configuration));
			memcpy(&c, mqttResponseString, sizeof(configuration));
			mySerial.println(c.timestamps[0]);
			mySerial.println(c.timestamps[1]);
			mySerial.println(c.timestamps[2]);
			mySerial.println(c.timestamps[3]);
			mySerial.println(c.timestamps[4]);
			memset(mqttResponseString, 0, sizeof(mqttResponseString));
		}
	}
}
