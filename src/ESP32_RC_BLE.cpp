#include <Arduino.h>
#include "ESP32_RC_BLE.h"


int ESP32_RC_BLE::role;
int ESP32_RC_BLE::status;
uint8_t ESP32_RC_BLE::data_sent[_MAX_MSG_LEN];
uint8_t ESP32_RC_BLE::data_recv[_MAX_MSG_LEN];
String ESP32_RC_BLE::data_sent_str;
String ESP32_RC_BLE::data_recv_str;
BLE2902* ESP32_RC_BLE::pBLE2902;
// for server
BLEServer* ESP32_RC_BLE::p_server;
BLEService* ESP32_RC_BLE::p_service;
BLECharacteristic* ESP32_RC_BLE::p_rx_charcs;
BLECharacteristic* ESP32_RC_BLE::p_tx_charcs;
BLEAdvertising* ESP32_RC_BLE::p_advertising;

// for client
BLEClient*  ESP32_RC_BLE::p_client;
BLEAdvertisedDevice* ESP32_RC_BLE::server_device;
BLEScan* ESP32_RC_BLE::p_scanner;
BLEUUID ESP32_RC_BLE::service_uuid;
BLEUUID ESP32_RC_BLE::rx_charcs_uuid;
BLEUUID ESP32_RC_BLE::tx_charcs_uuid;
BLERemoteService* ESP32_RC_BLE::p_peer_service;
BLERemoteCharacteristic* ESP32_RC_BLE::p_peer_tx_charcs;
BLERemoteCharacteristic* ESP32_RC_BLE::p_peer_rx_charcs;

/*
 * ================================================================
 *  External Callback functions 
 * 
 * ================================================================
 */
class Server_Callbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      ESP32_RC_BLE::status = SERVER_CONNECTED;
      ESP32_RC_BLE::debug("Server is connected");
    };

    void onDisconnect(BLEServer* pServer) {
      ESP32_RC_BLE::status = SERVER_NOT_CONNECTED;
      ESP32_RC_BLE::debug("Server is not connected");
    }
};




class Server_CharcsCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }
    }

};


/*
 * ================================================================
 *  External Callback functions - For client
 * 
 * ================================================================
 */

class Client_CallBack : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    ESP32_RC_BLE::debug("onConnect");
  }

  void onDisconnect(BLEClient* pclient) {
    ESP32_RC_BLE::debug("onDisconnect");
  }
};


class Advertise_DeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    //ESP32_RC_BLE::debug("BLE Found: " + advertisedDevice.toString());
    ESP32_RC_BLE::debug(advertisedDevice.toString().c_str() );
    
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(ESP32_RC_BLE::service_uuid)) {
      BLEDevice::getScan()->stop();
      ESP32_RC_BLE::server_device = new BLEAdvertisedDevice(advertisedDevice);
      ESP32_RC_BLE::debug(ESP32_RC_BLE::server_device->getAddress().toString().c_str());
    } // Found our server
    
  } 
}; 




/*
 * ================================================================
 *  Constructor
 * 
 * ================================================================
 */

ESP32_RC_BLE::ESP32_RC_BLE(int role, int core, bool fast_mode, bool debug_mode) : ESP32RemoteControl(role, core, fast_mode, debug_mode) {
  //ESP32_RC_ESPNOW::instance = this;

  Serial.begin(SERIAL_BAUD_RATE);
  role  = role;
  service_uuid = BLEUUID(SERVICE_UUID);
  rx_charcs_uuid = BLEUUID(CHARC_RX_UUID);
  tx_charcs_uuid = BLEUUID(CHARC_TX_UUID);
  memset(data_sent, 0, _MAX_MSG_LEN);
  memset(data_recv, 0, _MAX_MSG_LEN);
  data_sent_str = "";
  data_recv_str = "";  
  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);
};

/*
 * ================================================================
 *  All public functions
 * 
 * ================================================================
 */


void ESP32_RC_BLE::init() {
  switch (role) {
    case _ROLE_CONTROLLER: 
      init_server();
      do_advertising();
      break;
    case _ROLE_EXECUTOR:
      do_scan();
      init_client();
      this->connect();
      //read_server();
      break;
    default:
      break;
  }
}


bool ESP32_RC_BLE::check_connection() {
  switch (role) {
    case _ROLE_CONTROLLER: 
      return (status == SERVER_CONNECTED );      
      break;
    case _ROLE_EXECUTOR:

      break;
    default:
      break;
  }
  return false;
}

void ESP32_RC_BLE::send(String message) {

  int data_length = message.length();


  switch (role) {
    case _ROLE_CONTROLLER:
      do_notify(message);
      break;
    case _ROLE_EXECUTOR:
      do_write();
      break;
    default:
      break;
  }

}


String* ESP32_RC_BLE::recv() {


    return nullptr;
}

/*
 * ================================================================
 *  Server functions
 * 
 * ================================================================
 */


void ESP32_RC_BLE::init_server() {
  status = SERVER_INIT;
  BLEDevice::init(DEVICE_NAME);
  p_server = BLEDevice::createServer();
  p_server->setCallbacks(new Server_Callbacks());

  p_service = p_server->createService(SERVICE_UUID);
  p_tx_charcs = p_service->createCharacteristic(
              CHARC_TX_UUID,
              BLECharacteristic::PROPERTY_READ   |
              BLECharacteristic::PROPERTY_WRITE  |
              BLECharacteristic::PROPERTY_NOTIFY |
              BLECharacteristic::PROPERTY_INDICATE
            );

  p_tx_charcs->setValue(SERVER_HANDSHAKE_MSG); 
  p_tx_charcs->addDescriptor(pBLE2902);

  p_rx_charcs = p_service->createCharacteristic(
							CHARC_RX_UUID,
              BLECharacteristic::PROPERTY_READ   |
              BLECharacteristic::PROPERTY_WRITE  |
              BLECharacteristic::PROPERTY_NOTIFY |
              BLECharacteristic::PROPERTY_INDICATE
						);

  p_rx_charcs->setCallbacks(new Server_CharcsCallback());
  
  // Start the service
  p_service->start();
  
}

void ESP32_RC_BLE::do_advertising() {

  // Start advertising
  p_advertising = BLEDevice::getAdvertising();
  p_advertising->addServiceUUID(SERVICE_UUID);
  p_advertising->setScanResponse(true);
  p_advertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  p_advertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  // Ready to be connected
  status = SERVER_NOT_CONNECTED;
  debug("Server is not connected");
}


void ESP32_RC_BLE::do_notify(String message) {
  int data_length = message.length();
  debug(message);
  // set max message length
  if (data_length > _MAX_MSG_LEN) {
    data_length = _MAX_MSG_LEN;
  };

  // convert to uint8_t type.
  for (int i = 0; i < data_length; i++) {
    data_sent[i] = (uint8_t)message[i];
  };

  p_tx_charcs->setValue(data_sent, data_length);
  p_tx_charcs->notify();
}


/*
 * ================================================================
 *  Client functions
 * 
 * ================================================================
 */

// Client receive notification from server a adn then invoke this call back.
void ESP32_RC_BLE::client_notify_callback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  Serial.println((char*)pData);
}

void ESP32_RC_BLE::do_scan() {
  BLEDevice::init("");
  p_scanner = BLEDevice::getScan();
  p_scanner->setAdvertisedDeviceCallbacks(new Advertise_DeviceCallbacks());
  p_scanner->setInterval(1349);
  p_scanner->setWindow(449);
  p_scanner->setActiveScan(true);
  p_scanner->start(5, false);
}

void ESP32_RC_BLE::init_client() {
  p_client  = BLEDevice::createClient();
  p_client->setClientCallbacks(new Client_CallBack());
}

void ESP32_RC_BLE::connect() {
  p_client->connect(server_device); 
  p_peer_service = p_client->getService(service_uuid);
  if (p_peer_service == nullptr) {
    debug(service_uuid.toString().c_str());
    p_client->disconnect();
    return;
  }

  p_peer_tx_charcs = p_peer_service->getCharacteristic(tx_charcs_uuid);
  if (p_peer_tx_charcs == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(tx_charcs_uuid.toString().c_str());
    p_client->disconnect();
    return;
  }

  p_peer_rx_charcs = p_peer_service->getCharacteristic(rx_charcs_uuid);
  if (p_peer_rx_charcs == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(rx_charcs_uuid.toString().c_str());
    p_client->disconnect();
    return;
  }

  if (p_peer_tx_charcs->canNotify()) {
    p_peer_tx_charcs->registerForNotify(client_notify_callback);
  }

}


String ESP32_RC_BLE::read_server() {
  if (p_peer_tx_charcs->canRead()) {
      std::string value = p_peer_tx_charcs->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
  }
  return "aa";
}


void ESP32_RC_BLE::do_write() {
  String newValue = "Time since boot: " + String(millis()/1000);
  p_peer_rx_charcs->writeValue(newValue.c_str(), newValue.length());
}



/* 
 * ========================================================
 * run - Override 
 * ========================================================
 */
void ESP32_RC_BLE::run(void* data) {
 
}




/* 
 * ========================================================
 *   Common Utility Functions
 *   
 * 
 * 
 * ==========================================================
 */

void ESP32_RC_BLE::debug(String msg) {
  if (DEBUG) {
    Serial.println(String(millis()) + " : " + msg);
  }
}