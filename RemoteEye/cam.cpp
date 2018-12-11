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

#define BMPIMAGEOFFSET 66

const char bmp_header[BMPIMAGEOFFSET] PROGMEM =
{
		0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
		0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
		0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
		0x00, 0x00
};

const int camera_CS = 5;
bool camera_is_header = false;
bool camera_read_in_progress = false;

ArduCAM myCAM(OV2640, camera_CS);

uint8_t camera_temp = 0, camera_temp_last = 0;
uint32_t camera_length = 0;
uint32_t camera_photo_size = 0;

void _camera_prepare_fifo_read() {
	camera_temp = 0;
	camera_temp_last = 0;
	camera_length = 0;

	camera_length = camera_get_photo_size();
	if (camera_length >= MAX_FIFO_SIZE) { //512 kb
		return;
	}
	if (camera_length == 0 ) { //0 kb
		return;
	}
	myCAM.CS_LOW();
	myCAM.set_fifo_burst();
	camera_temp =  SPI.transfer(0x00);
	camera_length--;
}

uint8_t _camera_read_fifo_burst(uint8_t *buffer, uint8_t size)
{
	uint8_t b_index = 0;

	while (camera_length--) {
		camera_temp_last = camera_temp;
		camera_temp =  SPI.transfer(0x00);

		if (camera_is_header == true) {
			buffer[b_index++] = camera_temp;
		} else if ((camera_temp == 0xD8) & (camera_temp_last == 0xFF)) {
			camera_is_header = true;
			buffer[b_index++] = camera_temp_last;
			buffer[b_index++] = camera_temp;
		}

		if ((camera_temp == 0xD9) && (camera_temp_last == 0xFF)) {
			buffer[b_index++] = camera_temp;
			break;
		}

		delayMicroseconds(15);

		if (b_index == size) {
			myCAM.CS_HIGH();
			return b_index;
		}
	}

	delayMicroseconds(15);
	myCAM.CS_HIGH();
	camera_is_header = false;
	return b_index;
}

void camera_setup(byte dimension) {
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

uint32_t camera_get_photo_size() {
	if (camera_photo_size == 0) {
		camera_photo_size = myCAM.read_fifo_length();
	}
	return camera_photo_size;
}

uint8_t camera_read_captured_data(uint8_t *buffer, uint8_t size) {
	uint8_t read = 0;

	if (!camera_read_in_progress) {
		_camera_prepare_fifo_read();

		camera_read_in_progress = true;
	} else {
		myCAM.CS_LOW();
		myCAM.set_fifo_burst();
	}

	if (camera_read_in_progress) {
		read = _camera_read_fifo_burst(buffer, size);
	}

	return read;
}

void camera_set_capture_done() {
	camera_read_in_progress = false;
	myCAM.clear_fifo_flag();
	camera_photo_size = 0;
}
