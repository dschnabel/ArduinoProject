#include "Arduino.h"
#include <avr/wdt.h>
#include <ArduCAM.h>
#include "cam.h"
#include "HologramSIMCOM.h"
#include "Time.h"
#include "LowPower.h"

typedef struct {
	time_t timestamp;
} configuration;

#define TX_PIN 3
#define RX_PIN 2
SoftwareSerial mySerial(TX_PIN,RX_PIN);

HologramSIMCOM Hologram(&mySerial);
configuration config;
time_t last_config_update = 0;
byte loop_count = 0;
byte time_adjust_count = 0;
float voltage;

#define CLIENT 0
#define MODULES_SWITCH 9
#define VOLTAGE_READ_ENABLE_PIN 4
#define VOLTAGE_READ_PIN A3
#define LED_PIN 7
#define PHOTO_MAX_PUBLISH_COUNT 12 	// workaround to break out of loop if there's a problem with the camera

#define ACTION_CONNECT 1
#define ACTION_DISCONNECT 2
#define ACTION_PHOTO 3
#define ACTION_TEST 4
#define ACTION_SUBSCRIBE 5
#define ACTION_UNSUBSCRIBE 6
#define ACTION_MODULES_ON 7
#define ACTION_MODULES_OFF 8
#define ACTION_PRINT_CONFIG 9
#define ACTION_SEND_VOLTAGE 10

#define MSG_TYPE_CLIENT_SUBSCRIBED 0
#define MSG_TYPE_PHOTO_DATA 1
#define MSG_TYPE_PHOTO_DONE 2
#define MSG_TYPE_NEW_CONFIG 3
#define MSG_TYPE_VOLTAGE 4

void(* resetFunc) (void) = 0;  // declare reset fuction at address 0

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
		case '9': return ACTION_MODULES_ON;
		case '0': return ACTION_MODULES_OFF;
		case '!': return ACTION_PRINT_CONFIG;
		case 'v': return ACTION_SEND_VOLTAGE;
		}
	}

	return 0;
}

bool _action_modules_on() {
	if (!Hologram.isOn()) {
		digitalWrite(MODULES_SWITCH, LOW);
		delay(50);

		mySerial.println(F("MODULES ON"));
		digitalWrite(MODULES_SWITCH, HIGH);

		//camera_setup(OV2640_160x120);
		camera_setup(OV2640_640x480);

		if (!Hologram.begin(57600)) {
			return false;
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

	return true;
}

void _action_modules_off() {
	mySerial.println(F("MODULES OFF"));
	digitalWrite(MODULES_SWITCH, LOW);
}

bool _action_mqtt_connect() {
	return Hologram.mqttConnect();
}

void _action_mqtt_disconnect() {
	Hologram.mqttDisconnect();
}

bool _action_take_photo() {
	camera_capture_photo();
	byte messageNr = 0, packetNr = 0;

	// get UNIX timestamp
	time_t timestamp = nowAtCurrentTimezone();

	// put as many variables as possible in new context to prevent stack from being exhausted too soon
	if (1) {

		byte data[32];
		byte len = sizeof(data);
		int32_t photo_size = camera_get_photo_size()* 0.80;
		int32_t sent = 0;

		int bufferRemaining = Hologram.mqttInitMessage(CLIENT, messageNr, MSG_TYPE_PHOTO_DATA, packetNr++, photo_size);
		if (bufferRemaining == -1) return false;

		// fetch camera data and send to modem
		byte publishCount = 0;
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

				if (publishCount++ > PHOTO_MAX_PUBLISH_COUNT || !Hologram.mqttPublish()) return false;
				bufferRemaining = Hologram.mqttInitMessage(CLIENT, messageNr, MSG_TYPE_PHOTO_DATA, packetNr++, photo_size);
				if (bufferRemaining == -1) return false;
			}

			if (photo_size < len) {
				if (photo_size > 0) {
					Hologram.mqttAppendPayload(&data[index], photo_size);
					len -= photo_size;
					index = photo_size;
					sent += photo_size;
				}

				if (publishCount++ > PHOTO_MAX_PUBLISH_COUNT || !Hologram.mqttPublish()) return false;
				photo_size = (camera_get_photo_size() - sent) * 0.5;
				if (photo_size < len) photo_size = len;
				bufferRemaining = Hologram.mqttInitMessage(CLIENT, messageNr, MSG_TYPE_PHOTO_DATA, packetNr++, photo_size);
				if (bufferRemaining == -1) return false;
//					mySerial.println(photo_size);
			}

			if (bufferRemaining >= len) {
				bufferRemaining = Hologram.mqttAppendPayload(&data[index], len);
				if (bufferRemaining == -1) return false;
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

	if (!Hologram.mqttPublish()) return false;

	// notify done
	bool ret = Hologram.mqttPublish(CLIENT, messageNr, MSG_TYPE_PHOTO_DONE, packetNr++, (const byte*)&timestamp, sizeof(time_t));
	if (!ret) return false;

	camera_set_capture_done();

	// give module enough time to send data
	delay(3000);

	return true;
}

bool _action_mqtt_subscribe() {
	// note: a bug in SIMCOM module prevents a message from being received if we publish a message
	// AFTER subscribing to a topic. Do not publish between subscribing and unsubscribing.
	return Hologram.mqttSubscribe(CLIENT);
}

void _action_mqtt_unsubscribe() {
	Hologram.mqttUnsubscribe(CLIENT);
}

bool _action_update_config(byte *state, uint16_t *reportedSize, char *responseString,
		uint8_t responseStringLen, byte *index) {

	bool done = Hologram.mqttBufferState(state, reportedSize,
			responseString, responseStringLen, index);

	if (done) {
		memset(&config, 0, sizeof(configuration));
		memcpy(&config, responseString, sizeof(configuration));
		memset(responseString, 0, responseStringLen);
		mySerial.println(F("update done"));
	}

	return done;
}

bool _action_retrieve_config() {
	uint16_t reportedSize = 0;
	char responseString[sizeof(configuration) + 1];
	byte index = 0;
	byte state = 0;
	memset(responseString, 0, sizeof(responseString));

	if (!_action_mqtt_subscribe()) {
		return false;
	}

	bool ret = true;
	time_t t = now();

	wdt_enable(WDTO_8S);
	while (Hologram.mqttIsListening()) {
		wdt_reset();
		if (_action_update_config(&state, &reportedSize, responseString, sizeof(responseString), &index)) {
			_action_mqtt_unsubscribe();
		}
		if (now() - t > 10) {
			mySerial.println(F("no config received in 10 seconds. Giving up."));
			_action_mqtt_unsubscribe();
			ret = false;
		}
	}
	wdt_disable();

	last_config_update = Hologram.updateTime();
	if (last_config_update == 0) {
		ret = false;
	}

	return ret;
}

void _action_led_ok() {
	for (int i = 0; i < 5; i++) {
		digitalWrite(LED_PIN, HIGH);
		delay(100);
		digitalWrite(LED_PIN, LOW);
		delay(70);
	}
}

void _action_led_error() {
	while (1) {
		digitalWrite(LED_PIN, HIGH);
		delay(100);
		digitalWrite(LED_PIN, LOW);
		delay(70);
		digitalWrite(LED_PIN, HIGH);
		delay(400);
		digitalWrite(LED_PIN, LOW);
		delay(70);
	}
}

bool _action_startup_and_connect_modules() {
	if (!_action_modules_on()) {
		_action_modules_off();
		return false;
	}

	if (!_action_mqtt_connect()) {
		_action_modules_off();
		return false;
	}

	return true;
}

void _action_disconnect_and_shutdown_modules() {
	_action_mqtt_disconnect();
	_action_modules_off();
}

bool _action_time_for_photo(time_t now) {
	if (config.timestamp > 0 && config.timestamp <= now) {
		config.timestamp = 0;
		return true;
	}

	return false;
}

void _action_retry_later(unsigned int delay_sec) {
	config.timestamp = now() + delay_sec;
}

void _action_update_voltage() {
	digitalWrite(VOLTAGE_READ_ENABLE_PIN, HIGH);
	delay(200);

	long analog = 0;

	// to counter noise, collect a few values and average out
	for (int i=0;i<100;i++) {
		analog += analogRead(VOLTAGE_READ_PIN);
	}
	analog /= 100;

	digitalWrite(VOLTAGE_READ_ENABLE_PIN, LOW);

	int adjustment = 8; // change to match multimeter value
	voltage = 0;
	if (analog > 0) {
		voltage = ((analog + adjustment) * 3.3 / 1023.0) * (10070+4700) / 4700;
	}
}

bool _action_send_voltage() {
	return Hologram.mqttPublish(CLIENT, 0, MSG_TYPE_VOLTAGE, 0, (const byte*)&voltage, sizeof(float));
}

void _input_action() {
	byte code = _get_code();
	switch (code) {
	case ACTION_TEST: {
		// add code here
		break;
	}
	case ACTION_MODULES_ON:
		_action_modules_on();
		break;
	case ACTION_MODULES_OFF:
		_action_modules_off();
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
		mySerial.println(config.timestamp);
		mySerial.print(F("Current time: "));mySerial.println(now());
		break;
	case ACTION_SEND_VOLTAGE:
		_action_update_voltage();
		_action_send_voltage();
	}
}

//#####################################################################

void setup() {

  mySerial.begin(57600);
  while(!mySerial);

  pinMode(MODULES_SWITCH, OUTPUT);
  pinMode(VOLTAGE_READ_ENABLE_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // sleep a few seconds to allow for flashing
  digitalWrite(LED_PIN, HIGH);
  delay(3000);
  digitalWrite(LED_PIN, LOW);

  Hologram.debug();

  _action_update_voltage();

  if (!_action_startup_and_connect_modules()) {
	  resetFunc();
//	  _action_led_error();
  }

  if (!_action_send_voltage()) {
	  resetFunc();
//	  _action_led_error();
  }

  if (!_action_retrieve_config()) {
	  resetFunc();
//	  _action_led_error();
  }

  _action_disconnect_and_shutdown_modules();

  mySerial.print(F("Free RAM: ")); mySerial.println(freeRam());

  _action_led_ok();
}

void loop() {
//	_input_action();

	//-------- sleep here to save energy -----------
//	delay(8000);
	LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
	if (time_adjust_count++ < 11) {
		adjustTime(8);
	} else {
		time_adjust_count = 0;

		// we're losing a second every now and then, so compensate for that
		adjustTime(9);
	}
	//----------------------------------------------

	// only check every 32 seconds
	if (loop_count++ >= 3) {
		loop_count = 0;

		time_t n = now();

		if (_action_time_for_photo(n)) {
			_action_update_voltage();
			if (_action_startup_and_connect_modules()) {
				if (_action_send_voltage() && _action_retrieve_config() && _action_take_photo()) {
					_action_disconnect_and_shutdown_modules();
				} else {
					_action_modules_off();
					_action_retry_later(300); // retry in 5 min
				}
			} else {
				_action_retry_later(300); // retry in 5 min
				mySerial.println(F("Could not startup/connect SIM. No photo taken."));
			}

			//mySerial.print(F("Photo time: "));mySerial.println(now());
		}

		// make sure we get settings at least once a day (24h = 86400s)
		if (n > last_config_update + 86400) {
			_action_update_voltage();
			if (_action_startup_and_connect_modules()) {
				if (_action_send_voltage() && _action_retrieve_config()) {
					_action_disconnect_and_shutdown_modules();
				} else {
					_action_modules_off();
					last_config_update += 300; // retry in 5 min
					mySerial.println(F("Could not update config."));
				}
			} else {
				last_config_update += 300; // retry in 5 min
				mySerial.println(F("Could not startup/connect SIM. Config not updated."));
			}

			//mySerial.print(F("Config update time: "));mySerial.println(now());
		}
	}
}
