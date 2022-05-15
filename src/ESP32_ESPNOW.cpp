#include <Arduino.h>
#include <ESP32_ESPNOW.h>

int ESP32_ESPNOW::role;
volatile int ESP32_ESPNOW::peer_status;
uint8_t ESP32_ESPNOW::data_sent[_MAX_MSG_LEN];
uint8_t ESP32_ESPNOW::data_recv[_MAX_MSG_LEN];
String ESP32_ESPNOW::data_sent_str;
String ESP32_ESPNOW::data_recv_str;
esp_now_peer_info_t ESP32_ESPNOW::peer;

/*
 * =================================================================
 * Contructor
 *  - Controllor  
 *  - Executor
 * =================================================================
*/


ESP32_ESPNOW::ESP32_ESPNOW(int r) {
  memset(peer.peer_addr, 0, 6);
  memset(data_sent, 0, _MAX_MSG_LEN);
  memset(data_recv, 0, _MAX_MSG_LEN);
  data_sent_str = "";
  data_recv_str = "";
  ESP32_ESPNOW::role = r;
  if (role != _ROLE_CONTROLLER && role != _ROLE_EXECUTOR ) {
    raise_error("Invalid RC Rile");
  }
  debug("This Device RC Role = " + String(role) );
}



/*
 * =================================================================
 * Init ESPNow configuration 
 *   if failed, reboot
 * =================================================================
*/
void ESP32_ESPNOW::init() {
  switch (role) {
    case _ROLE_CONTROLLER:
      /* Master connects to Receiver  */
      WiFi.mode(WIFI_STA);
      break;
    case _ROLE_EXECUTOR:
      /* Receiver hosts an WIFI network as AP */
      WiFi.mode(WIFI_AP);
      config_ap();
      break;
    default:
      debug("Error: No Role is defined. Rebooting ...");
      delay(1000);
      ESP.restart();
      break;
  }

  WiFi.disconnect();

  if (esp_now_init() == ESP_OK) {
    debug("ESPNow Init Success.");
  } else {
    debug("ESPNow Init Failed, Rebooting ...");
    delay(1000);
    ESP.restart();
  }

  esp_now_register_send_cb(on_datasent);
  esp_now_register_recv_cb(on_datarecv); 
  ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
}


/*
 * =================================================================
 * Init Connections between Controller and Receiver
 *
 * =================================================================
*/
void ESP32_ESPNOW::connect(void) {
  int retry = 0;
  ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
  while (ESP32_ESPNOW::peer_status != _PEER_READY) {
    retry ++;
    if (retry >= 100) {
      raise_error("Failed to connect.");
    }
    debug ("connect : Role(" + String(role) + ") ESP32_ESPNOW::peer_status=" + String(ESP32_ESPNOW::peer_status) + ")" );
    switch (ESP32_ESPNOW::peer_status) {
      case _PEER_NOT_FOUND:   // -2
        if (role == _ROLE_CONTROLLER) { 
          scan_network();
        };
        break;
      case _PEER_FOUND:       // 1
        pair_peer();
        break;
      case _PEER_PAIRED:      // 2
        do_handshake();
        break;
      case _PEER_HANDSHAKE:   // 3
        break;
      case _PEER_READY:       // 4
        break;
      case _PEER_ERROR:       // -1
        ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
        break;
      default:
        ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;  // -2
        break;    
    }
    delay(10);
  }
}


/*
 * =================================================================
 * Detect Network Connection Status
 *  - If error detected, needs to re-connect 
 * =================================================================
*/
bool ESP32_ESPNOW::check(void) {
  int retry = 0;
  while (ESP32_ESPNOW::peer_status != _PEER_READY && retry <=6) {
    retry ++;
    switch (ESP32_ESPNOW::peer_status) {
      case _PEER_NOT_FOUND:
        if (role == _ROLE_CONTROLLER) { 
          scan_network();
        };
        break;
      case _PEER_FOUND:  
        pair_peer();
        break;
      case _PEER_PAIRED:  
        do_handshake();
        break;
      case _PEER_HANDSHAKE:  
        break;
      case _PEER_READY:  
        break;
      case _PEER_ERROR:  
        ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
        break;    
      default:
        ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
        break;    
    }
  }
  debug ("check : Role(" + String(role) + ") ESP32_ESPNOW::peer_status=" + String(ESP32_ESPNOW::peer_status) + ")" );
  return (ESP32_ESPNOW::peer_status==_PEER_READY); 
}


/* 
 * ========================================================
 *   Only invoked from Executor (Slave)
 *   
 *  
 * ==========================================================
 */
void ESP32_ESPNOW::config_ap(void) {
  bool result = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, ESPNOW_CHANNEL, 0);
  if (!result) {
    debug("AP Config failed.");
  } else {
    debug("AP Config Success. Broadcasting with AP: " + String(WIFI_SSID));
    debug(WiFi.macAddress());
  }
}


void ESP32_ESPNOW::scan_network() {
/* 
 * ================================================================== 
 *  Only invoked from Controller (Master)
 *  to scan for the possible receivers
 * 
 * 
 * 
 * 
 * ==================================================================
 */
   
  int8_t scanResults = WiFi.scanNetworks();
  ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
  memset(&peer, 0, sizeof(peer));
  if (scanResults > 0) {
    debug("scan_network :"  + String(scanResults) + " devices found.");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
      delay(10);
      // Check if the current device matches
      if (SSID.indexOf(WIFI_SSID) == 0) {
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &peer.peer_addr[0], &peer.peer_addr[1], &peer.peer_addr[2], &peer.peer_addr[3], &peer.peer_addr[4], &peer.peer_addr[5] ) ) {
          ESP32_ESPNOW::peer_status = _PEER_FOUND;
        }          
        break;
      }
    }
  } else {
    debug("scan_network : Failed - No receiver found");
    ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
  }
  
  // clean up ram
  WiFi.scanDelete();
}



/* 
 * ========================================================
 * Pairing ESP Now Controller and Receiver
 * 
 * 
 * ========================================================
 */
void ESP32_ESPNOW::pair_peer() {
  if ( ! esp_now_is_peer_exist(peer.peer_addr) || ESP32_ESPNOW::peer_status != _PEER_READY ) { // if not exists
    
    //clean existing pairing
    if (esp_now_del_peer(peer.peer_addr) == ESP_OK ) {
		  debug("pair_peer : Pair cleaned success - " + String(ESP32_ESPNOW::peer_status));
    }

    // prepare for pairing - RC receiver needs to set ifidx
    peer.channel = ESPNOW_CHANNEL;  // pick a channel
    peer.encrypt = 0;               // no encryption
    if (role ==_ROLE_EXECUTOR ) { 
      peer.ifidx = WIFI_IF_AP;
    }

    // do pairing
    switch (esp_now_add_peer(&peer)) {
      case ESP_OK :
        ESP32_ESPNOW::peer_status = _PEER_PAIRED;
        debug("pair_peer : Success.");
        break;
      default:
        ESP32_ESPNOW::peer_status = _PEER_NOT_FOUND;
        debug("pair_peer : Failed." );
        break;
    }
  } 
}


/* 
 * ========================================================
 * Handshake two peers
 *  - Needs to be done after pairing 
 * ========================================================
 */
void ESP32_ESPNOW::do_handshake() {
  // assume here the ESP32_ESPNOW::peer_status is PEER_PAIRED
  if (ESP32_ESPNOW::peer_status != _PEER_PAIRED) {
    return ;
  };

  // do the hand shake
  switch (role) {
    case _ROLE_CONTROLLER :
      send(_HANDSHAKE_MSG);
      if (ESP32_ESPNOW::peer_status != _PEER_ERROR) {
        ESP32_ESPNOW::peer_status = _PEER_HANDSHAKE;
      } else {
        return ; // if ERROR
      }
      break;
    case _ROLE_EXECUTOR:
      ESP32_ESPNOW::peer_status = _PEER_HANDSHAKE;
      break;
    default :
      ESP32_ESPNOW::peer_status = _PEER_HANDSHAKE;
      break;
  }

  debug("do_handshake : status = " + String(ESP32_ESPNOW::peer_status) );
}



/* 
 * ========================================================
 *  Public send
 * ==========================================================
 */

void ESP32_ESPNOW::send(String message) {
  int data_length = message.length();
  esp_err_t result;

  // set max message length
  if (data_length > _MAX_MSG_LEN) {
    data_length = _MAX_MSG_LEN;
  };

  
  // convert to uint8_t type.
  for (int i = 0; i < data_length; i++) {
    data_sent[i] = (uint8_t)message[i];
  };

  // send message
  debug("send : [" + message + "] to (" + mac2str(peer.peer_addr) + ")");
  result = esp_now_send(peer.peer_addr, data_sent, data_length);
  if (result == ESP_OK) {
    data_sent_str = ((String) ((char*)data_sent)).substring(0,data_length);
    debug("send : Success");
  } else {
    ESP32_ESPNOW::peer_status = _PEER_ERROR;
    debug("send : Failed");
  }
}

/* 
 * ========================================================
 *  Public recv
 * ==========================================================
 */
String ESP32_ESPNOW::recv() {
  //String str = (char*)data_recv;
  return data_recv_str;
}



/* 
 * ========================================================
 * Call back functions
 *    
 * 
 * 
 * ==========================================================
 */

void ESP32_ESPNOW::on_datasent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  debug("on_datasent : to (" + mac2str(mac_addr) + ")");
  if (status != ESP_NOW_SEND_SUCCESS) {
    ESP32_ESPNOW::peer_status = _PEER_ERROR;
    debug("on_datasent : Failed.");
  } else {
    debug("on_datasent : Success.");
  }
}

void ESP32_ESPNOW::on_datarecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  int data_length = data_len;

  // truncate the message if longer than limit.
  if (data_length>_MAX_MSG_LEN) {
    data_length =  _MAX_MSG_LEN;
  }

  // set peer_addr
  if (!is_mac_set(peer.peer_addr) || ESP32_ESPNOW::peer_status == _PEER_NOT_FOUND ) {
    memcpy( &peer.peer_addr, mac_addr, 6 );
    ESP32_ESPNOW::peer_status = _PEER_FOUND;
  };



  // set data_recv buffer
  memcpy(data_recv,data,data_length);
  data_recv_str = ((String) ((char*)data_recv)).substring(0,data_length);

  debug("on_datarecv : [" + data_recv_str + "](len=" + String(data_length)  + ") from (" + mac2str(peer.peer_addr) + ")");

  // once received hand shake message - then ready.
  if (data_recv_str == _HANDSHAKE_MSG ) {
    if (role == _ROLE_EXECUTOR ) { // if receiver, echo message back.
      pair_peer();
      send(_HANDSHAKE_MSG);
      if (ESP32_ESPNOW::peer_status == _PEER_ERROR) {
        return;
      }
    };
    ESP32_ESPNOW::peer_status = _PEER_READY;
    debug("on_datarecv : ESP32_ESPNOW::peer_status = " + String(ESP32_ESPNOW::peer_status));
  };
}

