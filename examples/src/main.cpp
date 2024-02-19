#include <Arduino.h>
#include <ESP32_RC_ESPNOW.h>

/*
 * ESPNOW bi-directional communication sample
 * Both controllor/executor using the exactly same code as blow.
 *
*/



ESP32_RC_ESPNOW rc_controller(_ROLE_CONTROLLER, false, true);
//ESP32_RC_ESPNOW rc_controller(_ROLE_EXECUTOR, false, true);
unsigned long count = 0; 


unsigned long start_time = millis();
unsigned long total_bytes = 0;
Message msg;
Message recv_data;
int cycle_count = 100;



void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  rc_controller.init();
  rc_controller.connect();
  Serial.println(sizeof(Message));
}



void loop() {
  count ++;
  String str = String(10000000 + count) + " abcdefghijklmnopers";
  strcpy(msg.msg1, str.c_str());
  msg.a1 = millis();
  rc_controller.send(msg);
  recv_data = rc_controller.recv();
  vTaskDelay(pdMS_TO_TICKS(10));
  if (count % cycle_count == 0) {
    unsigned long time_taken = millis() - start_time;
    Serial.println(String(count) + " : " + String((float)int(time_taken/cycle_count*100)/100) + " : " + String(recv_data.msg1) );
    start_time = millis();
    total_bytes = 0;
  };
}