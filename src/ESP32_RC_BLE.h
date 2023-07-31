#pragma once
#include <Arduino.h>
#include <ESP32_RC.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLE2902.h>
/* 
 * ============================================================================
 *
 * Controller -> BLE Server
 * Receiver   -> BLE Client 
 * 
 * 
 *  RC Controller - BLE Server
 *    - Running and listening
 *    - detects client connections
 *    - once client connected - send messages 
 *  RC Receiver   - BLE Client
 *    
 *  
 *  
 * 
 * 
 * ============================================================================
*/



#define DEBUG                   1
#define SERIAL_BAUD_RATE        115200


// BLE Specific marcos
#define DEVICE_NAME             "ESP32_BLE_CNTLR"

#define SERVICE_UUID            "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" 
#define CHARC_RX_UUID           "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARC_TX_UUID           "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"



//Peer handshake
#define SERVER_HANDSHAKE_MSG    "HELLO_FROM_BLE_SERVER"
#define CLIENT_HANDSHAKE_MSG    "HELLO_FROM_BLE_CLIENT"


// BLE connection status - Server
#define SERVER_ERROR            -1
#define SERVER_INIT             0
#define SERVER_NOT_CONNECTED    1
#define SERVER_CONNECTED        2

// BLE connection status - Client
#define SERVER_DEVICE_FOUND     10
#define SERVER_SERVICE_FOUND    11
#define CLIENT_ERROR            -10


class ESP32_RC_BLE : public ESP32RemoteControl {
  public:
    ESP32_RC_BLE(int role, int core=1, bool fast_mode=false, bool debug_mode=false); 
    void init(void) override;
    void connect(void) override;               // general wrapper to establish the connection
    void send(String data) override;           // only en-queue the message
    String* recv(void) override;               // general wrapper to receive data
    void run(void* data) override;             // Override the Task class run function

    static int status;
    bool check_connection();
    static void debug(String msg);

    static BLEUUID service_uuid;
    static BLEUUID tx_charcs_uuid;
    static BLEUUID rx_charcs_uuid;
    static BLEAdvertisedDevice* server_device;
    
  private:
    static int role;
    static uint8_t data_sent[_MAX_MSG_LEN];
    static String data_sent_str;
    static uint8_t data_recv[_MAX_MSG_LEN];
    static String data_recv_str;

    static BLE2902 *pBLE2902;
    // for server
    static BLEServer *p_server;
    static BLEService *p_service;
    static BLECharacteristic *p_rx_charcs; 
    static BLECharacteristic *p_tx_charcs; 
    static BLEAdvertising *p_advertising;

    // for cleint
    static BLEScan *p_scanner;
    static BLEClient*  p_client;
    static BLERemoteService* p_peer_service ;
    static BLERemoteCharacteristic* p_peer_tx_charcs;
    static BLERemoteCharacteristic* p_peer_rx_charcs;

    // for server
    static void init_server();
    static void do_advertising();
    static void do_notify(String data);

    // for client
    static void init_client();
    static void do_scan();
    static String read_server();
    static void do_write();
    static void client_notify_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

}; 



