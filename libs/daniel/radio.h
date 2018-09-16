#ifndef RADIO_H_
#define RADIO_H_

#include "RH_RF95.h"

#define RADIO_MAX_PAYLOAD_LEN 63
#define RADIO_USE_AES 1

typedef struct radio_payload {
	byte type;
	byte data[RADIO_MAX_PAYLOAD_LEN];
} radio_payload_t;

typedef struct radio_message {
	radio_payload_t payload;
	byte len;
} radio_message_t;

void radio_setup_server();
void radio_setup_client();
byte radio_available();
bool radio_send();
void radio_set_message(byte type, byte len = 0, byte* buffer = NULL);
radio_message_t* radio_get_message();

#endif /* RADIO_H_ */
