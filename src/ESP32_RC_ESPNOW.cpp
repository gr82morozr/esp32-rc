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
ESP32_RC_ESPNOW::ESP32_RC_ESPNOW(int role, bool fast_mode, bool debug_mode) : ESP32RemoteControl(role, fast_mode, debug_mode) {
  ESP32_RC_ESPNOW::instance = this;
}

ESP32_RC_ESPNOW::~ESP32_RC_ESPNOW() {
  // clean up existing timers
  xTimerStop(send_timer, 0);
  xTimerDelete(send_timer, 0);
  xTimerStop(heartbeat_timer, 0);
  xTimerDelete(heartbeat_timer, 0);  
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

  // turn on WiFi
  WiFi.mode(WIFI_STA);

  // Set channel, power
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
  if (send_queue == NULL || recv_queue == NULL) {
    _ERROR_("Failed to create queues.");
  }

  // Create the mutex
  mutex = xSemaphoreCreateMutex();

  // Create Timer
  // For example : _ESPNOW_DATA_RATE = 100 (times/sec)
  //              => int(1000/_ESPNOW_DATA_RATE) = 10 (ms),  delay 10ms for each timer event
  send_timer = xTimerCreate("Timer", pdMS_TO_TICKS( int(1000/_ESPNOW_DATA_RATE) ), pdTRUE, (void*)0, send_timer_callback);
  
  // heartbeat_timer triggers every 1 sec
  heartbeat_timer = xTimerCreate("Timer", pdMS_TO_TICKS( int(1000/_ESPNOW_HEARTBEAT_RATE) ), pdTRUE, (void*)0, heartbeat_timer_callback);

  if (send_timer == NULL || heartbeat_timer == NULL) {
    _ERROR_("Failed to create timer");
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
  
  xTimerStart(send_timer, 0);
  xTimerStart(heartbeat_timer, 0);
  _DEBUG_("Success.");
}


void ESP32_RC_ESPNOW::send_timer_callback(TimerHandle_t xTimer) {
  //ESP32_RC_ESPNOW* this_instance = static_cast<ESP32_RC_ESPNOW*>(pvTimerGetTimerID(xTimer));
  instance->send_queue_msg();
}

void ESP32_RC_ESPNOW::heartbeat_timer_callback(TimerHandle_t xTimer) {
  instance->op_send(instance->create_sys_msg(_HEARTBEAT_MSG));
  digitalWrite(BUILTIN_LED, HIGH);
}




/* 
 * ========================================================
 * Util functions
 * ========================================================
 */

Message ESP32_RC_ESPNOW::create_sys_msg(String data) {
  Message msg;
  strncpy(msg.sys, data.c_str(), sizeof(msg.sys) - 1);
  msg.sys[sizeof(msg.sys) - 1] = '\0';
  msg.is_set = true;
  return msg;
}


String ESP32_RC_ESPNOW::extract_sys_msg(const Message &msg) {
  return String(msg.sys);
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
  while(de_queue(queue, &msg) == pdPASS){
    // do nothing.
  }
}

bool ESP32_RC_ESPNOW::en_queue(QueueHandle_t queue,  Message *pmsg) {
  return (xQueueSend(queue, pmsg, ( TickType_t ) 10) == pdPASS);
};

bool ESP32_RC_ESPNOW::de_queue(QueueHandle_t queue,  Message *pmsg) {
  if (pmsg == nullptr) return false;
  return (xQueueReceive(queue, pmsg, ( TickType_t ) 10) == pdPASS);
};


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

void ESP32_RC_ESPNOW::send(Message data) {
  int status = 0;
  get_value(&handshake_status, &status);
  if (fast_mode) {
    if (status != _STATUS_HSHK_OK) { return; }
    if (get_queue_depth(send_queue) >= _RC_QUEUE_DEPTH ) {
      Message msg;
      de_queue(send_queue, &msg);
      send_metric.out_count --;
    }
    en_queue(send_queue, &data);
    send_metric.in_count ++;
    return;      
  } else {
    while (true) {
      // make sure handshake is completed successfully, then perform send
      if( get_queue_depth(send_queue) < _RC_QUEUE_DEPTH && status == _STATUS_HSHK_OK ) {
        //msg = create_sys_msg(data);
        en_queue(send_queue, &data);
        send_metric.in_count ++;
        return;
      } else {
        vTaskDelay(pdMS_TO_TICKS(int( 1000/_ESPNOW_DATA_RATE ))); 
      }
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
  // only wait for new messages while when the queue is empty.
  if (get_queue_depth(send_queue) == 0 ) {
    vTaskDelay(pdMS_TO_TICKS( int( 1000/_ESPNOW_DATA_RATE/2 ) )); 
    return;
  }

  // start sending process ...
  long start_time = millis();
  Message msg =  {};
  while (millis () - start_time < 2000) {
    // peek the message in send_queue, ** not committed to de-queue **
    if ( ! msg.is_set ) {
      if (xQueuePeek(send_queue, &msg, ( TickType_t ) 10) == pdTRUE) { 
        msg.is_set  = true;
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
        msg.is_set  = true;
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

/*
bool ESP32_RC_ESPNOW::op_send(Message msg) {
  set_value(&send_status, _STATUS_SEND_IN_PROG);
  return (esp_now_send(peer.peer_addr, msg.data, msg.length) == ESP_OK);
}
*/

bool ESP32_RC_ESPNOW::op_send(Message msg) {
  set_value(&send_status, _STATUS_SEND_IN_PROG);
  return (esp_now_send(peer.peer_addr, (uint8_t *)&msg, sizeof(Message)) == ESP_OK);
}

/* 
 * ========================================================
 * run - Override 
 * ========================================================
 */
void ESP32_RC_ESPNOW::run(void* data) {
  connect();
}

/* 
 * ========================================================
 * recv - Override 
 * ========================================================
 */
Message ESP32_RC_ESPNOW::recv(void) {
  Message msg;
  if (get_queue_depth(recv_queue) > 0) {
    if (xQueueReceive(recv_queue, &msg, ( TickType_t ) 10) == pdTRUE) {
      recv_metric.out_count ++;
      return msg;
    }
  } 
  return {};
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
  op_send(create_sys_msg(_HANDSHAKE_MSG));
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
    //_DEBUG_("to (" + mac2str(mac_addr) + ") Failed.  status = " + String (op_status));
  }
}



void ESP32_RC_ESPNOW::on_datarecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  int status;
  Message msg;
  String data_recv_str;
  
  memcpy(&msg, data, sizeof(Message));
  
  
  // Handshake Hello received and send Ack (priority #1)
  if (strcmp(msg.sys, _HANDSHAKE_MSG) == 0) {
    pair_peer(mac_addr);
    op_send(create_sys_msg(_HANDSHAKE_ACK_MSG));
    empty_queue(send_queue);
    return;
  }

  // check if handshake in progress, and process Ack
  get_value(&handshake_status, &status);
  //_DEBUG_( String(status));
  if (strcmp(msg.sys, _HANDSHAKE_ACK_MSG) == 0 && status == _STATUS_HSHK_IN_PROG) {
    pair_peer(mac_addr);
    empty_queue(send_queue);
    empty_queue(recv_queue);
    set_value(&handshake_status, _STATUS_HSHK_OK);
    return;
  }

  // received heartbeat, then return heartbeat Ack
  if (strcmp(msg.sys, _HEARTBEAT_MSG) == 0 ) {
    op_send(create_sys_msg(_HEARTBEAT_ACK_MSG));
    return ;
  }

  // received heart beat ack, heart beat cycle completed, then turn off the LED
  if (strcmp(msg.sys, _HEARTBEAT_ACK_MSG) == 0 ) {
    digitalWrite(BUILTIN_LED,LOW);
    return ;
  }

  // regular message
  if (status == _STATUS_HSHK_OK) {
    recv_metric.in_count ++;
    // add protection to de-queue front message to avoid queue overflow failure.
    while (get_queue_depth(recv_queue) >= _RC_QUEUE_DEPTH )  { 
      xQueueReceive(recv_queue, &msg, ( TickType_t ) 5); 
    } 
    // en-queue the message, ready for send
    if (xQueueSend(recv_queue, &msg, ( TickType_t ) 10) != pdPASS ) {
      _ERROR_("'recv_queue' depth = " + String(get_queue_depth(recv_queue)));
    }
  }
}

