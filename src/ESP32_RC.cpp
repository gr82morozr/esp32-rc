#include <Arduino.h>
#include <ESP32_RC.h>



ESP32RemoteControl::ESP32RemoteControl(int role) {
  this->role = role;
  Serial.println(ESP32_RC_DEV);        
  
  // constructors for all devs
  #if ESP32_RC_DEV == _ESP32_BLE
    this->rc_class = new ESP32_BLE(role);
   
  // BLE specific stuff here
  #elif ESP32_RC_DEV == _ESP32_WLAN
    this->rc_class = new ESP32_WLAN(role);

  // WLAN specific stuff here
  #elif ESP32_RC_DEV == _ESP32_NRF24
    // NRF24 specific stuff here
    this->rc_class = new ESP32_NRF24(role);
  
  #else // ESP32_ESPNOW is the default and the best !!
    this->rc_class = new ESP32_ESPNOW(role);

  #endif 
};

void ESP32RemoteControl::init() {
  while (1) {
    try {
      this->rc_class->init();
      break;
    } catch (...) {
      handle_exception();
      debug("RC init failed, Role = " + String(this->role));
      delay(100);
    }
  }
}



void ESP32RemoteControl::connect() {
  while (1) {
    try {
      this->rc_class->connect();
      break;
    } catch (...) {
      handle_exception();
      debug("RC connect failed, Role = " + String(this->role));
      delay(100);
    }
  }
 
}


bool ESP32RemoteControl::check() {
  return this->rc_class->check();
}

void ESP32RemoteControl::send(String message) {
  this->rc_class->send(message);
}

String ESP32RemoteControl::recv() {
  return this->rc_class->recv();
}


void ESP32RemoteControl::handle_exception(void) {
  pinMode(BUILTIN_LED, OUTPUT);
  for (int i = 0; i <=3; i++ ) {
    digitalWrite(BUILTIN_LED, HIGH);
    delay(50);
    digitalWrite(BUILTIN_LED, LOW);
    delay(50);
  }
}