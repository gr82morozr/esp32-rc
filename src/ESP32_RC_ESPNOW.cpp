#include <ESP32_RC_ESPNOW.h>

uint8_t ESP32_RC_ESPNOW::broadcast_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
ESP32_RC_ESPNOW* ESP32_RC_ESPNOW::instance = nullptr;

/* 
 * ========================================================
 * Constructor
 * ========================================================
 * 
 * For ESPNOW, the role value is ignored.
 *  
 */
ESP32_RC_ESPNOW::ESP32_RC_ESPNOW(int role, int core, bool debug_mode) : ESP32RemoteControl(role, core, debug_mode) {
  ESP32_RC_ESPNOW::instance = this;
}

/* 
 * ========================================================
 * init - Override 
 * ========================================================
 */

void ESP32_RC_ESPNOW::init(void)  {
  _DEBUG_("Started");

  // allocate memory
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  memcpy(peer.peer_addr, broadcast_addr, 6); 

  WiFi.mode(WIFI_STA);

  // Set channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_max_tx_power(_ESPNOW_OUTPUT_POWER);
  

  int attempt = 0;
  int max_retry = 100;
  while (attempt <= max_retry) {
    attempt++;
    if (esp_now_init() == ESP_OK)  break;
    vTaskDelay(pdMS_TO_TICKS(10)); 
    if (attempt >= max_retry ) {
      _ERROR_ ("Failed. Attempts >= Max Retry (" + String(max_retry) + ")");
    }  
  }
  
  // Create queues
  send_queue = xQueueCreate(_RC_QUEUE_DEPTH, sizeof(Message));
  recv_queue = xQueueCreate(_RC_QUEUE_DEPTH, sizeof(Message));

  // Create the mutex
  mutex = xSemaphoreCreateMutex();

  if (send_queue == NULL || recv_queue == NULL) {
    _ERROR_("Failed to create queues.");
  }

  // Register
  esp_now_register_send_cb(ESP32_RC_ESPNOW::static_on_datasent);
  esp_now_register_recv_cb(ESP32_RC_ESPNOW::static_on_datarecv); 
  
  // Set the init value of peer_status
  _DEBUG_("Success.");
}

/* 
 * ========================================================
 * connect - Override 
 * ========================================================
 */
void ESP32_RC_ESPNOW::connect(void) {
  _DEBUG_ ("Started.");
  int attempt = 0;
  int max_retry = 100;
  while (attempt <= max_retry) {
    attempt++;
    if (handshake() == true) break;
    vTaskDelay(pdMS_TO_TICKS(10)); 
    if (attempt >= max_retry ) {
      _ERROR_ ("Failed. Attempts >= Max Retry (" + String(max_retry) + ")");
    }  
  }

  // start processing the send message queue.
  set_value(&send_status, _STATUS_SEND_READY);
  
  start(); 
  _DEBUG_("Success.");
}

/* 
 * ========================================================
 * Util functions
 * ========================================================
 */

ESP32_RC_ESPNOW::Message ESP32_RC_ESPNOW::create_message(String data) {
  ESP32_RC_ESPNOW::Message  msg;
  msg.length = data.length() < _MAX_MSG_LEN ? data.length() : _MAX_MSG_LEN;
  data.toCharArray((char*)msg.data, msg.length + 1);
  return msg;
}

String ESP32_RC_ESPNOW::extract_message(Message msg) {
  return ((String) ((char*)msg.data)).substring(0,msg.length);
}

void ESP32_RC_ESPNOW::set_value(int *in_varible, int value) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  *in_varible = value;
  xSemaphoreGive(mutex);      
};

void ESP32_RC_ESPNOW::get_value(int *in_varible, int *out_varible) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  *out_varible = *in_varible;
  xSemaphoreGive(mutex);        
};

void ESP32_RC_ESPNOW::empty_queue(QueueHandle_t queue) {
  Message msg;
  while(xQueueReceive(queue, &msg, (TickType_t) 10) == pdPASS){
    // do nothing.
  }
}

void ESP32_RC_ESPNOW::pair_peer(const uint8_t *mac_addr) {
  memcpy( &peer.peer_addr, mac_addr, ESP_NOW_ETH_ALEN );
  if ( ! esp_now_is_peer_exist(peer.peer_addr) ) { // if not exists
    // prepare for pairing - RC receiver needs to set ifidx
    peer.channel = _ESPNOW_CHANNEL;  // pick a channel
    peer.encrypt = 0;               // no encryption
    peer.ifidx   = WIFI_IF_STA ;
    esp_now_add_peer(&peer);
  }; 
}

void ESP32_RC_ESPNOW::unpair_peer(const uint8_t *mac_addr) {
  esp_now_del_peer(mac_addr);
}



/* 
 * ========================================================
 * send - Override 
 * en-queue the message only
 * ========================================================
 */

void ESP32_RC_ESPNOW::send(String data) {
  int status = 0;
  while (true) {
    // make sure handshake is completed successfully, then perform send
    get_value(&handshake_status, &status);

    if( get_queue_depth(send_queue) < _RC_QUEUE_DEPTH && status == _STATUS_HSHK_OK ) {
      Message msg;
      msg.length = data.length() < _MAX_MSG_LEN ? data.length() : _MAX_MSG_LEN;
      data.toCharArray((char*)msg.data, msg.length + 1);
      xQueueSend(send_queue, &msg, ( TickType_t ) 10);
      send_metric.in_count ++;
      return;
    } else {
      vTaskDelay(pdMS_TO_TICKS(int( 1000/_ESPNOW_DATE_RATE ))); 
    }
  }
}

void ESP32_RC_ESPNOW::send_queue_msg() {
  int status = 0;

  
  // Lock the varible
  get_value(&send_status, &status);

  /*
  // not ready, last message could be still in progress.
  if (status != _STATUS_SEND_READY) {
    vTaskDelay(pdMS_TO_TICKS(2)); 
    return;
  }
  */

  // if send_queue empty, then done
  // only wait for while when the queue is empty.
  if (get_queue_depth(send_queue) == 0 ) {
    vTaskDelay(pdMS_TO_TICKS( int( 1000/_ESPNOW_DATE_RATE/2 ) )); 
    return;
  }

  // start sending process...
  long start_time = millis();
  Message msg =  { {}, 0 };
  while (millis () - start_time < 2000) {
    // peek the message in send_queue
    if (msg.length == 0) {
      if (xQueuePeek(send_queue, &msg, ( TickType_t ) 10) == pdTRUE) { 
      } else {
        continue;
      } 
    }

    // trigger send operation.
    // if failed, go to next cycle
    if (op_send(msg) == false) { 
      continue;
    }
    
    while (millis () - start_time <= 1000) {  // wait for on_datasent to confirm.
      // Lock the varible
      get_value(&send_status, &status);

      if (status == _STATUS_SEND_DONE) { // all good.
        xQueueReceive(send_queue, &msg, ( TickType_t ) 10);  // de-queue the message
        send_metric.out_count ++;
        set_value(&send_status, _STATUS_SEND_READY);
        return; 
      };
      if (status == _STATUS_SEND_ERR) {
        send_metric.err_count ++;
        break; // exit the loop and try again.
      }
      vTaskDelay(pdMS_TO_TICKS(2));   // wait for a while
    }
  }
  
}

bool ESP32_RC_ESPNOW::op_send(Message msg) {
  set_value(&send_status, _STATUS_SEND_IN_PROG);
  return (esp_now_send(peer.peer_addr, msg.data, msg.length) == ESP_OK);
}


/* 
 * ========================================================
 * run - Override 
 * ========================================================
 */
void ESP32_RC_ESPNOW::run(void* data) {
  TickType_t xlast_waketime;
  const TickType_t xfreq = pdMS_TO_TICKS(1000/_ESPNOW_DATE_RATE); 

  unsigned long count = 0;
  xlast_waketime = xTaskGetTickCount();
  while (true) {
    vTaskDelayUntil( &xlast_waketime, xfreq );
    send_queue_msg();
    count ++;
    if (count % _ESPNOW_DATE_RATE == 0) {
      _DEBUG_("Send : => " + String(send_metric.in_count) + " => Q ("+ String(get_queue_depth(send_queue)) + ") => "  + String(send_metric.out_count)  + " : Err = " + String(send_metric.err_count) );
      _DEBUG_("Recv : => " + String(recv_metric.in_count) + " => Q ("+ String(get_queue_depth(recv_queue)) + ") => " +  String(recv_metric.out_count));
      _DEBUG_("");
    }
  }
}

/* 
 * ========================================================
 * recv - Override 
 * ========================================================
 */
String* ESP32_RC_ESPNOW::recv(void){
  static String data;
  Message msg;
  if (get_queue_depth(recv_queue) > 0) {
    if (xQueueReceive(recv_queue, &msg, ( TickType_t ) 10) == pdTRUE) {
      data = extract_message(msg);
      recv_metric.out_count ++;
      return &data;
    }
  } 
  return NULL;
}


/* 
 * ========================================================
 * Handshake two peers
 *  - Needs to be done before pairing 
 *  - it should block send/send_queue_msg 
 * ========================================================
 */
bool ESP32_RC_ESPNOW::handshake() {
  _DEBUG_("Started.");
  
  // Lock the varible
  set_value(&handshake_status, _STATUS_HSHK_IN_PROG);

  unsigned long start_time = millis();

  // Send broadcast
  pair_peer(broadcast_addr);
  op_send(create_message(_HANDSHAKE_MSG));
  unpair_peer(broadcast_addr);

  

  // Wait Ack 
  int status = 0;
  while (millis() - start_time < 10000) {   
    // read handshake
    get_value(&handshake_status, &status);
    if (status == _STATUS_HSHK_OK) {
      _DEBUG_("Success.");
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  _DEBUG_("Failed.");
  return false;
}



/* 
 * ========================================================
 * Call back functions
 * ==========================================================
 */
void ESP32_RC_ESPNOW::static_on_datasent(const uint8_t *mac_addr, esp_now_send_status_t op_status) {
  instance->on_datasent(mac_addr, op_status);
}

void ESP32_RC_ESPNOW::static_on_datarecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  instance->on_datarecv(mac_addr, data, data_len);
}

void ESP32_RC_ESPNOW::on_datasent(const uint8_t *mac_addr, esp_now_send_status_t op_status) {
  if (op_status == ESP_NOW_SEND_SUCCESS) {
    set_value(&send_status, _STATUS_SEND_DONE);
    //_DEBUG_("to (" + mac2str(mac_addr) + ") Success.");
  } else {
    set_value(&send_status, _STATUS_SEND_ERR);
    _DEBUG_(String(send_status));
    _DEBUG_("to (" + mac2str(mac_addr) + ") Failed.  status = " + String (op_status));
  }
}

void ESP32_RC_ESPNOW::on_datarecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  int status;
  Message msg;
  String data_recv_str;
  
  msg.length = data_len>_MAX_MSG_LEN ? _MAX_MSG_LEN: data_len;
  memcpy(msg.data, data, msg.length);

  // collect received data
  data_recv_str = extract_message(msg);
  //_DEBUG_(data_recv_str);
  // Handshake Hello received and send Ack (priority #1)
  if (data_recv_str == _HANDSHAKE_MSG ) {   
    pair_peer(mac_addr);
    op_send(create_message(_HANDSHAKE_ACK_MSG));
    empty_queue(send_queue);
    return;
  }

  // check if handshake in progress, and process Ack
  get_value(&handshake_status, &status);
  //_DEBUG_( String(status));
  if (data_recv_str == _HANDSHAKE_ACK_MSG && status == _STATUS_HSHK_IN_PROG) {
    pair_peer(mac_addr);
    empty_queue(send_queue);
    empty_queue(recv_queue);
    set_value(&handshake_status, _STATUS_HSHK_OK);
    return;
  }

  // regular message
  if (status == _STATUS_HSHK_OK) {
   recv_metric.in_count ++;
   // add protection to de-queue front message to avoid failure.
    while (get_queue_depth(recv_queue) >= _RC_QUEUE_DEPTH )  { 
      xQueueReceive(recv_queue, &msg, ( TickType_t ) 10); 
    } 
    if (xQueueSend(recv_queue, &msg, ( TickType_t ) 10) != pdPASS ) {
      _ERROR_("'recv_queue' depth = " + String(get_queue_depth(recv_queue)));
    }
  }
}

