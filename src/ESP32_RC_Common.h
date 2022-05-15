#pragma once

#include <Arduino.h>

/* 
  ESP32 supported Wireless connections 
*/
#define _ESP32_NOW            0
#define _ESP32_BLE            1
#define _ESP32_WLAN           2
#define _ESP32_NRF24          3

/* 
  Below are specific for Remote Control purpose, 
  not for general remote large amount data transfer 
*/

#define _MAX_MSG_LEN          140         // This is designed specifically for RC
#define _CONTROL_RATE         30          // 30 times per second

/* 
  Remote Control Roles 
  Only support two nodes (in pair), not multi-executors
*/

#define _ROLE_CONTROLLER      1           // RC Controller Role
#define _ROLE_EXECUTOR        2           // RC Executor Role


/* 
  Status Code for all possible scenarios 
  - Different Wireless devices may have different status
  - Mainly classified by 3 : [Success], [in Progress], [Error]

*/
#define _STATUS_SUCCESS        0
#define _STATUS_INIT_OK        1

#define _STATUS_INIT_ERR      -1
#define _STATUS_CONN_ERR      -2
#define _STATUS_TIMEOUT       -3
#define _STATUS_DISCONN       -4

#define _STATUS_SEND_ERR      -5
#define _STATUS_RECV_ERR      -6

#define _HANDSHAKE_MSG         "HELLO_ESP32_RC"

#define _ESP32_RC_DEBUG        0                    // when 1 , enable debug




/* =========   ESPNOW/WiFi shared Macros ========= */
#define WIFI_SSID             "ESP32-RC-WLAN"
#define WIFI_PASSWORD         "vdjfiend#d0%d"

// ESPNOW Specific Marcos
/* =========   ESPNOW Specific Macros ========= */
#define ESPNOW_CHANNEL        1


/* =========   NRF24 Specific Macros ========= */
/*
  For ESP32, needs to define the SPI pins
*/

#define HSPI_MISO             12
#define HSPI_MOSI             13  
#define HSPI_SCLK             14  // Clock Pin
#define HSPI_CS               15  // Chip Selection Pin
#define NRF24_CE              4   // Chip Enable Pin


void raise_error(String message) ;
void debug(String message) ;
String mac2str(const uint8_t *mac_addr) ;
bool is_mac_set(const uint8_t *mac_addr);