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
bool camera_is_header = false;
bool camera_read_in_progress = false;

//ArduCAM myCAM(OV2640, camera_CS);

uint8_t camera_temp = 0, camera_temp_last = 0;
uint32_t camera_length = 0;
uint32_t camera_photo_size = 0;

void _camera_prepare_fifo_read(ArduCAM *myCAM) {
	camera_temp = 0;
	camera_temp_last = 0;
	camera_length = 0;

	camera_length = camera_get_photo_size(myCAM);
	if (camera_length >= MAX_FIFO_SIZE) { //512 kb
		return;
	}
	if (camera_length == 0 ) { //0 kb
		return;
	}
	myCAM->CS_LOW();
	myCAM->set_fifo_burst();
	camera_temp =  SPI.transfer(0x00);
	camera_length--;
}

uint8_t _camera_read_fifo_burst(ArduCAM *myCAM, uint8_t *buffer, uint8_t size, SoftwareSerial *mySerial)
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
			mySerial->println(F("+01"));
			buffer[b_index++] = camera_temp;
			break;
		}

		delayMicroseconds(15);

		if (b_index == size) {
			myCAM->CS_HIGH();
			return b_index;
		}
	}

	delayMicroseconds(15);
	myCAM->CS_HIGH();
	camera_is_header = false;
	return b_index;
}

void camera_setup(ArduCAM *myCAM, byte dimension, SoftwareSerial *mySerial) {
	delay(100);
	uint8_t vid, pid, temp;
	Wire.begin();
	delay(100);
	pinMode(camera_CS, OUTPUT);
	digitalWrite(camera_CS, HIGH);

//	mySerial->println(F("001"));
	SPI.begin();
	delay(100);
	myCAM->write_reg(0x07, 0x80);
	delay(100);
	myCAM->write_reg(0x07, 0x00);
	delay(100);
//	mySerial->println(F("002"));
	while(1){
		myCAM->write_reg(ARDUCHIP_TEST1, 0x55);
		delay(100);
		temp = myCAM->read_reg(ARDUCHIP_TEST1);
		delay(100);
		if (temp != 0x55){
//			Serial.println(F("ACK CMD SPI interface Error! END"));
			delay(1000);
			break;
		} else {
//			Serial.println(F("ACK CMD SPI interface OK. END"));
			break;
		}
	}
//	mySerial->println(F("003"));

	while(1){
		myCAM->wrSensorReg8_8(0xff, 0x01);
		delay(100);
		myCAM->rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
		delay(100);
		myCAM->rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
		delay(100);
		if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
//			Serial.println(F("ACK CMD Can't find OV2640 module! END"));
			delay(1000);
			break;
		} else{
//			Serial.println(F("ACK CMD OV2640 detected. END"));
			break;
		}
	}
//	mySerial->println(F("004"));

	myCAM->set_format(JPEG);
//	mySerial->println(F("004b"));
	myCAM->InitCAM(mySerial);
	delay(100);

//	mySerial->println(F("005"));

	myCAM->OV2640_set_JPEG_size(dimension);

//	mySerial->println(F("006"));
	delay(1000);
	myCAM->clear_fifo_flag();
	delay(100);
//	mySerial->println(F("007"));
}

void camera_stop() {
	SPI.end();
}

bool _camera_capture_ready(ArduCAM *myCAM) {
	delay(50);
	if (myCAM->get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
		return true;
	}
	return false;
}

void camera_capture_photo(ArduCAM *myCAM) {
	myCAM->flush_fifo();
	myCAM->clear_fifo_flag();
	myCAM->start_capture();

	while (!_camera_capture_ready(myCAM));
}

uint32_t camera_get_photo_size(ArduCAM *myCAM) {
	if (camera_photo_size == 0) {
		camera_photo_size = myCAM->read_fifo_length();
	}
	return camera_photo_size;
}

void __tmp_print(SoftwareSerial *mySerial, uint8_t read) {
	char str[3];
	sprintf(str,"%02X", read);
	mySerial->print(str);
}

uint8_t camera_read_captured_data(ArduCAM *myCAM, uint8_t *buffer, uint8_t size, SoftwareSerial *mySerial) {
	uint8_t old = 0;
	uint8_t read = 0;
	uint32_t len = myCAM->read_fifo_length();
	mySerial->println(len);
	uint32_t index = 0;
	uint32_t real_len = 0;
	while (1) {
		old = read;
		read = myCAM->read_fifo();

		if (index++ >= len) {
			break;
		}

		if (index % 100 == 0) {
			__tmp_print(mySerial, read);
		}

		if ((read == 0xD9) && (old == 0xFF)) {
			mySerial->print(F("FF"));
			__tmp_print(mySerial, read);
			mySerial->print(F(" - "));
			real_len = index;
		}
	}
	mySerial->println(F(" -end1"));

	read = 0;
	index = 0;
	uint8_t flags = myCAM->read_reg(ARDUCHIP_FIFO);
	myCAM->write_reg(ARDUCHIP_FIFO, flags|FIFO_RDPTR_RST_MASK);
	while (index++ < real_len) {
		read = myCAM->read_fifo();
		if (index % 100 == 0 || index>=real_len-1) {
			__tmp_print(mySerial, read);
		}
	}
	mySerial->println(F(" -end2"));

//	if (!camera_read_in_progress) {
//		_camera_prepare_fifo_read(myCAM);
//
//		camera_read_in_progress = true;
//	} else {
//		myCAM->CS_LOW();
//		myCAM->set_fifo_burst();
//	}
//
//	if (camera_read_in_progress) {
//		read = _camera_read_fifo_burst(myCAM, buffer, size, mySerial);
//	}

	return read;
}

void camera_set_capture_done(ArduCAM *myCAM) {
	camera_read_in_progress = false;
	myCAM->clear_fifo_flag();
	camera_photo_size = 0;
}
