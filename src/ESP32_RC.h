#pragma once

/*
  ESP32 Remote Control Wrapper Library
  1 - 

*/

#include <Arduino.h>

#include <ESP32_RC_Common.h>
/*
#include <ESP32_BLE.h>
#include <ESP32_WLAN.h>
#include <ESP32_ESPNOW.h>
#include <ESP32_NRF24.h>
*/

#ifndef ESP32_RC_DEV
  #define ESP32_RC_DEV _ESP32_NRF24
#endif



#if ESP32_RC_DEV == _ESP32_BLE
  #define HDR "ESP32_BLE.h"
  #define RC_CLASS ESP32_BLE
#elif ESP32_RC_DEV == _ESP32_WLAN
  #define HDR "ESP32_WLAN.h"
  #define RC_CLASS ESP32_WLAN
#elif ESP32_RC_DEV == _ESP32_NRF24
  #define HDR "ESP32_NRF24.h"
  #define RC_CLASS ESP32_NRF24
//#elif ESP32_RC_DEV == _ESP32_NOW  
//  #define HDR "ESP32_ESPNOW.h"  
//  #define RC_CLASS ESP32_ESPNOW
#else
  #define HDR "ESP32_ESPNOW.h"  // default
  #define RC_CLASS ESP32_ESPNOW
#endif


#include HDR







// ===============================================

class ESP32RemoteControl {
  public:
    ESP32RemoteControl(int role);         // role = _ROLE_CONTROLLER | _ROLE_EXECUTOR
    RC_CLASS *rc_class;
    void init(void);                      // general wrapper to init the RC configuration
    void connect(void);                   // general wrapper to establish the connection
    bool check(void);                     // general wrapper to check and return the connection status
    void send(String data);               // general wrapper to send data
    String recv(void);                    // general wrapper to receive data
    void handle_exception();              // Exception handler
    int role;                             // 
    int status;                           // status of each step
    long count;                           // message count
    

  private:
    uint8_t data_sent[_MAX_MSG_LEN];
    String data_sent_str;
    uint8_t data_recv[_MAX_MSG_LEN];
    String data_recv_str;

};


