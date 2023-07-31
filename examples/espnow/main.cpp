#include <Arduino.h>
#include <ESP32_RC_ESPNOW.h>

/*
 * ESPNOW bi-directional communication sample
 * Both controllor/executor using the exactly same code as blow.
 *
*/


ESP32_RC_ESPNOW rc_controller(_ROLE_CONTROLLER, 1, true, false);
unsigned long count = 0; 

void setup() {
  rc_controller.init();
  rc_controller.connect();
}

unsigned long start_time = millis();
unsigned long total_bytes = 0;
String message;
String* recv_data;
int cycle_count = 10;

void loop() {
  count ++;
  message = String(10000000 + count) + "abcdefghijklmnoperstuvwxyz1234567890assssbcdefghijklmnoperstuvwxyz1234567890abcdefghijklmnotuvwxyz1234567890abcdefghijklmnoperstuvwxyz1234567890rstuvwxyz1234567890abcdefghijklmnoperstuvrstuvwxyz1234567890abcdefghijklmnoperstuvrstuvwxyz1234";
  total_bytes += message.length();
  rc_controller.send(message);
  recv_data = rc_controller.recv();
  vTaskDelay(pdMS_TO_TICKS(100));
  if (count % cycle_count == 0) {
    unsigned long time_taken = millis() - start_time;
    Serial.println(String(count) + " : " + String((float)int(time_taken/cycle_count*100)/100) + " : " + String(int(total_bytes/time_taken * 1000)) );
    start_time = millis();
    total_bytes = 0;
  };
}