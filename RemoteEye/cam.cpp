/*
 * cam.cpp
 *
 *  Created on: Sep 10, 2018
 *      Author: Jack
 */

#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>

#include "cam.h"
#include "memorysaver.h"

const byte camera_CS = 5;

ArduCAM myCAM(OV2640, camera_CS);

void camera_setup(byte dimension) {
	delay(500);

	uint8_t vid, pid, temp;
	Wire.begin();

	pinMode(camera_CS, OUTPUT);
	digitalWrite(camera_CS, HIGH);

	SPI.begin();

	myCAM.write_reg(0x07, 0x80);
	delay(100);
	myCAM.write_reg(0x07, 0x00);
	delay(100);
	while(1){
		myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
		temp = myCAM.read_reg(ARDUCHIP_TEST1);
		if (temp != 0x55){
//			Serial.println(F("ACK CMD SPI interface Error! END"));
			delay(1000);
			continue;
		} else {
//			Serial.println(F("ACK CMD SPI interface OK. END"));
			break;
		}
	}

	while(1){
		myCAM.wrSensorReg8_8(0xff, 0x01);
		myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
		myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
		if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
//			Serial.println(F("ACK CMD Can't find OV2640 module! END"));
			delay(1000);
			continue;
		} else{
//			Serial.println(F("ACK CMD OV2640 detected. END"));
			break;
		}
	}

	myCAM.set_format(JPEG);
	myCAM.InitCAM();

	myCAM.OV2640_set_JPEG_size(dimension);

	delay(1000);
	myCAM.clear_fifo_flag();
}

void camera_stop() {
	SPI.end();
}

bool _camera_capture_ready() {
	delay(50);
	if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
		return true;
	}
	return false;
}

void camera_capture_photo() {
	myCAM.flush_fifo();
	myCAM.clear_fifo_flag();
	myCAM.start_capture();

	while (!_camera_capture_ready());
}

void _camera_reset_read() {
	uint8_t flags = myCAM.read_reg(ARDUCHIP_FIFO);
	myCAM.write_reg(ARDUCHIP_FIFO, flags|FIFO_RDPTR_RST_MASK);
}

uint32_t camera_get_photo_size() {
	_camera_reset_read();

	uint8_t old = 0;
	uint8_t read = 0;
	uint32_t len = myCAM.read_fifo_length();
	uint32_t real_len = 0;

	uint32_t i;
	for (i = 0; i < len; i++) {
		old = read;
		read = myCAM.read_fifo();
		if ((read == 0xD9) && (old == 0xFF)) {
			real_len = i+1;
			break;
		}
	}

	if (i == len) {
		real_len = 0;
	}

	_camera_reset_read();

	return real_len;
}

void camera_read_captured_data(uint8_t *buffer, uint8_t size) {
	for (int i = 0; i < size; i++) {
		buffer[i] = myCAM.read_fifo();
	}
}

void camera_set_capture_done() {
	myCAM.clear_fifo_flag();
}
