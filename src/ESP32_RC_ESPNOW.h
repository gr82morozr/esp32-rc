#pragma once
#include <Arduino.h>
#include <ESP32_RC.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>


class ESP32_RC_ESPNOW : public ESP32RemoteControl {
  public:
    ESP32_RC_ESPNOW(int role, int core=1, bool fast_mode=false, bool debug_mode=false); 
    void init(void) override;
    void connect(void) override;               // general wrapper to establish the connection
    void send(String data) override;           // only en-queue the message
    String* recv(void) override;               // general wrapper to receive data
  
  private:
    struct Message {
      uint8_t data[_MAX_MSG_LEN];              // Buffer that holds the message
      size_t length;                           // Actual size of the message
    };

    struct Metric {
      unsigned long in_count;
      unsigned long out_count;
      unsigned long err_count;
    };

    Metric send_metric = {0, 0, 0};
    Metric recv_metric = {0, 0, 0};
    SemaphoreHandle_t mutex;                   // for locking
    int handshake_status;                      // keep track of handshake status 
    int send_status;                           // keep track of send status 
    static ESP32_RC_ESPNOW* instance;
    static uint8_t broadcast_addr[6];

    esp_now_peer_info_t peer;
    QueueHandle_t send_queue;
	  QueueHandle_t recv_queue;


    void pair_peer(const uint8_t *mac_addr);   // ESPNOW - pairing peer
    void unpair_peer(const uint8_t *mac_addr); // ESPNOW - un-pairing peer
    bool handshake(void);                      // Handshake process
    void run(void* data) override;             // Override the Task class run function
    void send_queue_msg(void) ;                // send msg in send_queue
    bool op_send(Message msg) ;                // only en-queue the message to send_queue

    static void static_on_datasent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void static_on_datarecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);

    void on_datasent(const uint8_t *mac_addr, esp_now_send_status_t status) ;
    void on_datarecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);

    Message create_message(String data);       // convert gaven String to Message
    String extract_message(Message msg);       // extract Message data to String
    
    void set_value(int *in_varible, int value);
    void get_value(int *in_varible, int *out_varible);
    void empty_queue(QueueHandle_t queue);
    bool en_queue(QueueHandle_t queue, Message *pmsg);
    bool de_queue(QueueHandle_t queue, Message *pmsg);
};



// To do:
// 1 - add reliable mode, to make sure every message is sent or received.
//     in non-reliable mode, messages are dropped when queue is full.
//     

