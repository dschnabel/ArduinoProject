#include "Arduino.h"
#include <ArduCAM.h>
#include "cam.h"
#include "HologramSIMCOM.h"
#include "Time.h"

typedef struct {
	time_t timestamps[5];
} configuration;

#define TX_PIN 3
#define RX_PIN 2
SoftwareSerial mySerial(TX_PIN,RX_PIN);

HologramSIMCOM Hologram(&mySerial);
configuration config;

uint16_t mqttReportedTextSize = 0;
char mqttResponseString[sizeof(configuration) + 1];
byte r_index = 0;
byte state = 0;

#define CLIENT 0
#define SIM_SWITCH 9

#define ACTION_CONNECT 1
#define ACTION_DISCONNECT 2
#define ACTION_PHOTO 3
#define ACTION_TEST 4
#define ACTION_SUBSCRIBE 5
#define ACTION_UNSUBSCRIBE 6
#define ACTION_SIM_ON 7
#define ACTION_SIM_OFF 8
#define ACTION_PRINT_CONFIG 9

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
		case '9': return ACTION_SIM_ON;
		case '0': return ACTION_SIM_OFF;
		case '!': return ACTION_PRINT_CONFIG;
		}
	}

	return 0;
}

void _action_sim_on() {
	mySerial.println(F("SIM ON"));
	digitalWrite(SIM_SWITCH, HIGH);

	if (!Hologram.begin(57600)) {
		return;
	}

	switch (Hologram.cellStrength()) {
	case 0: mySerial.println(F("No signal"));break;
	case 1: mySerial.println(F("Very poor signal strength")); break;
	case 2: mySerial.println(F("Poor signal strength")); break;
	case 3: mySerial.println(F("Good signal strength")); break;
	case 4: mySerial.println(F("Very good signal strength")); break;
	case 5: mySerial.println(F("Excellent signal strength"));
	}
}

void _action_mqtt_connect() {
	Hologram.mqttConnect();
}

void _action_mqtt_disconnect() {
	Hologram.mqttDisconnect();
}

void _action_sim_off() {
	mySerial.println(F("SIM OFF"));
	digitalWrite(SIM_SWITCH, LOW);
}

void _action_take_photo() {
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
		if (bufferRemaining == -1) return;

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
//					mySerial.println(photo_size);
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
//			mySerial.print(F("padding: "));mySerial.println(photo_size);
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

void _action_mqtt_subscribe() {
	// note: a bug in SIMCOM module prevents a message from being received if we publish a message
	// AFTER subscribing to a topic. Do not publish between subscribing and unsubscribing.
	Hologram.mqttSubscribe(CLIENT);
}

void _action_mqtt_unsubscribe() {
	Hologram.mqttUnsubscribe(CLIENT);
}

void _action_update_config() {
	bool done = Hologram.mqttBufferState(&state, &mqttReportedTextSize,
			mqttResponseString, sizeof(mqttResponseString), &r_index);

	if (done) {
		memset(&config, 0, sizeof(configuration));
		memcpy(&config, mqttResponseString, sizeof(configuration));
		memset(mqttResponseString, 0, sizeof(mqttResponseString));
		mySerial.println(F("update done"));
	}
}

void _input_action() {
	byte code = _get_code();
	switch (code) {
	case ACTION_TEST: {
		// add code here
		break;
	}
	case ACTION_SIM_ON:
		_action_sim_on();
		break;
	case ACTION_SIM_OFF:
		_action_sim_off();
		break;
	case ACTION_CONNECT:
		_action_mqtt_connect();
		break;
	case ACTION_DISCONNECT:
		_action_mqtt_disconnect();
		break;
	case ACTION_PHOTO:
		_action_take_photo();
		break;
	case ACTION_SUBSCRIBE:
		_action_mqtt_subscribe();
		break;
	case ACTION_UNSUBSCRIBE:
		_action_mqtt_unsubscribe();
		break;
	case ACTION_PRINT_CONFIG:
		mySerial.println(config.timestamps[0]);
		mySerial.println(config.timestamps[1]);
		mySerial.println(config.timestamps[2]);
		mySerial.println(config.timestamps[3]);
		mySerial.println(config.timestamps[4]);
		break;
	}
}

//#####################################################################

void setup() {

  mySerial.begin(57600);
  while(!mySerial);

  pinMode(SIM_SWITCH, OUTPUT);

  Hologram.debug();

  camera_setup(OV2640_160x120);
//  camera_setup(OV2640_640x480);

  _action_sim_on();

  memset(mqttResponseString, 0, sizeof(mqttResponseString));
  mySerial.print(F("Free RAM: "));
  mySerial.println(freeRam());
}

void loop()
{
	_input_action();

	if (Hologram.mqttIsListening()) {
		_action_update_config();
	}
}
