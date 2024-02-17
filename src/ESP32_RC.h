#pragma once
#include <Arduino.h>
#include <ESP32_RC_Common.h>
#include <Task/Task.h>
#include <freertos/timers.h>
#include <queue>


/*
 *
 * Remote Control Library
 * 
 * ESP32RemoteControl : Abstract Class
 * Support below protocols :
 * - ESPNOW
 * 
 * - BLE (to do)
 * - Bluetooth Serial (to do)
 * - Wifi (to do)
 * - NRD24 (to do)
 *
*/


class ESP32RemoteControl : public Task {
  public:
    // pointer function 
    typedef void (*funcPtrType)(void); // a function pointer type

    // constructor
    ESP32RemoteControl(int role, bool fast_mode, bool debug_mode);   // role = _ROLE_CONTROLLER | _ROLE_EXECUTOR

    // common functions
    virtual void init(void)               = 0;     // general wrapper to init the RC configuration
    virtual void connect(void)            = 0;     // general wrapper to establish the connection
    virtual void send(Message data)       = 0;     // general wrapper to send data
    virtual Message recv(void)            = 0;     // general wrapper to receive data
    
    void enable_fast(bool mode);                   // if mode==false, then disable, othewrise enable
    void enable_debug(bool mode);                  // if mode==false, then disable, othewrise enable

    funcPtrType custom_handler = nullptr;          // A Custom Exception Handler.
                                                   // For example : beeping, LED blinking... etc


  protected:
    // common settings
    int role;                                      // _ROLE_CONTROLLER or _ROLE_EXECUTOR  
    int status;                                    // connection status
    bool fast_mode = false;                        // enable or disable quick mode
    bool debug_mode = false;                       // enable or disable debug mode


    void debug(String func_name, String message) ;        // Output debug info
    void raise_error(String func_name, String message) ;  // Common method to raise error.
    String mac2str(const uint8_t *mac_addr) ;             // Convert MAC address to String
    int get_queue_depth(QueueHandle_t queue);             // get queue depth
    
  
  private :
    String exception_message;
    void handle_exception();                 // Exception handler
    String format_time(unsigned long ms);
    String format_time();
    bool is_serial_set = false;

    

};


