#pragma once
#include <Arduino.h>

#include <ESP32_RC_Common.h>
#include <SPI.h>
#include "RF24.h"


class ESP32_NRF24 {
  public :
    ESP32_NRF24(int role);
    void init();
    void connect();
    bool check();
    void send(String data);
    String recv();
    int role;
    int status;

  private:
    RF24 *radio;
    uint8_t * address;

    uint8_t data_sent[_MAX_MSG_LEN];
    String data_sent_str;
    uint8_t data_recv[_MAX_MSG_LEN];
    String data_recv_str;

};