/*
 * cam.h
 *
 *  Created on: Sep 10, 2018
 *      Author: Jack
 */

#ifndef CAM_H_
#define CAM_H_

#include <SoftwareSerial.h>
void camera_setup(ArduCAM *myCAM, byte dimension, SoftwareSerial *mySerial);
void camera_stop();
void camera_capture_photo(ArduCAM *myCAM);
uint32_t camera_get_photo_size(ArduCAM *myCAM);
uint8_t camera_read_captured_data(ArduCAM *myCAM, uint8_t *buffer, uint8_t size, SoftwareSerial *mySerial);
void camera_set_capture_done(ArduCAM *myCAM);

#endif /* CAM_H_ */
