/*
 * cam.h
 *
 *  Created on: Sep 10, 2018
 *      Author: Jack
 */

#ifndef CAM_H_
#define CAM_H_

void camera_setup();
void camera_capture_photo();
uint8_t camera_read_captured_data(uint8_t *buffer, uint8_t size);
void camera_set_capture_done();

#endif /* CAM_H_ */
