#pragma once
#include <Arduino.h>

#include <ESP32_RC_Common.h>
#include <esp_now.h>
#include <WiFi.h>



// Peer status enum 
#define _PEER_ERROR          -1
#define _PEER_NOT_FOUND      -2
#define _PEER_FOUND          1   // pair found, ready for pairing
#define _PEER_PAIRED         2   // pairing is done, ready to handshake
#define _PEER_HANDSHAKE      3   // Handshake in progress
#define _PEER_READY          4   // Handshake is done, ready to communicate



class ESP32_ESPNOW {
  public :
    ESP32_ESPNOW(int role);
    static void init();
    static void connect();
    static bool check();
    static void send(String data);
    static String recv();
    static int role;
    static int status;

  private:
    static esp_now_peer_info_t peer;
    static volatile int peer_status;

    static uint8_t data_sent[_MAX_MSG_LEN];
    static String data_sent_str;
    static uint8_t data_recv[_MAX_MSG_LEN];
    static String data_recv_str;

    static void config_ap();
    static void scan_network();
    static void pair_peer();
    static void do_handshake();

    static void on_datasent(const uint8_t *mac_addr, esp_now_send_status_t status) ;
    static void on_datarecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);


};