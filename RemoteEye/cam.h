/*
 * cam.h
 *
 *  Created on: Sep 10, 2018
 *      Author: Jack
 */

#ifndef CAM_H_
#define CAM_H_

void camera_setup(byte dimension);
void camera_stop();
void camera_capture_photo();
uint32_t camera_get_photo_size();
void camera_read_captured_data(uint8_t *buffer, uint8_t size);
void camera_set_capture_done();

#endif /* CAM_H_ */
