#include "Arduino.h"
#include <HologramSIMCOM.h>
#include <base64.h>

#include "secrets.h"

#define TX_PIN 2
#define RX_PIN 3

HologramSIMCOM Hologram(TX_PIN, RX_PIN, HOLOGRAM_KEY);

bool runOnce = true;

void setup() {

  Serial.begin(115200);
  while(!Serial);

//  Hologram.debug();
//
//  if (!Hologram.begin(8888)) {
//	  while (1) {
//		  delay(10000);
//	  }
//  }
//
//  switch (Hologram.cellStrength()) {
//      case 0: Serial.println("No signal");break;
//      case 1: Serial.println("Very poor signal strength"); break;
//      case 2: Serial.println("Poor signal strength"); break;
//      case 3: Serial.println("Good signal strength"); break;
//      case 4: Serial.println("Very good signal strength"); break;
//      case 5: Serial.println("Excellent signal strength");
//  }

//  Hologram.send(0, 0, 0, 1, "arduino");

  encode_control control;
  control.index = 0;
  char encoded[100];
  char input1[] = "abc ";
  char input2[] = "def";
  char input3[] = " ghi";
  String all;

  base64_encode(&control, encoded, input1, strlen(input1));
  Serial.print(encoded);
  all += encoded;
  delay(1000);

  base64_encode(&control, encoded, input2, strlen(input2));
  Serial.print(encoded);
  all += encoded;
  delay(1000);

  base64_encode(&control, encoded, input3, strlen(input3));
  Serial.print(encoded);
  all += encoded;
  delay(1000);

  base64_encode_finalize(&control, encoded);
  Serial.println(encoded);
  all += encoded;
  delay(1000);

  char decoded[100];
  strcpy(encoded, all.c_str());
  base64_decode(decoded, encoded, strlen(encoded));
  Serial.println(decoded);
}


void loop()
{

}
