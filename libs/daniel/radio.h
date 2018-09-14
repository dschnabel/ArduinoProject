#ifndef RADIO_H_
#define RADIO_H_

#include "RH_RF95.h"

#define RADIO_MAX_PAYLOAD_LEN 150 // do not go higher than RH_RF95_MAX_MESSAGE_LEN - 1

typedef struct radio_payload {
	uint8_t type;
	uint8_t data[RADIO_MAX_PAYLOAD_LEN];
} radio_payload_t;

typedef struct radio_message {
	radio_payload_t payload;
	uint8_t len;
} radio_message_t;

void radio_setup_server();
void radio_setup_client();
uint8_t radio_available();
bool radio_send();
void radio_set_message(uint8_t type, uint8_t len = 0, uint8_t* buffer = NULL);
radio_message_t* radio_get_message();

#endif /* RADIO_H_ */
