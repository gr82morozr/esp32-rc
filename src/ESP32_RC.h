#pragma once
#include <Arduino.h>
#include <ESP32_Common.h>
#include "Task/Task.h"
#include <queue>


/*
 *
 * Remote Control Library
 *
 *
*/


class ESP32RemoteControl : public Task {
  public:
    // pointer function 
    typedef void (*funcPtrType)(void); // a function pointer type

    // constructor
    ESP32RemoteControl(int role, int core, bool debug_mode);   // role = _ROLE_CONTROLLER | _ROLE_EXECUTOR

    // common functions
    virtual void init(void)         = 0;     // general wrapper to init the RC configuration
    virtual void connect(void)      = 0;    // general wrapper to establish the connection
    virtual void send(String data)  = 0;    // general wrapper to send data
    virtual String* recv(void)      = 0;    // general wrapper to receive data
    

    funcPtrType custom_handler = nullptr;           // A Custom Exception Handler

    void enable_debug(bool debug_mode);             // if mode==false, then disable, othewrise enable


  protected:
    // common settings
    int role;                                // _ROLE_CONTROLLER or _ROLE_EXECUTOR  
    int status;                               // connection status
    bool debug_mode = false;

    void debug(String func_name, String message) ;        // Output debug info
    void raise_error(String func_name, String message) ;  // Common method to raise error.
    String mac2str(const uint8_t *mac_addr) ; // Convert MAC address to String
    int get_queue_depth(QueueHandle_t queue);    
    
  
  private :
    String exception_message;
    void handle_exception();                 // Exception handler
    String format_time(unsigned long ms);
    String format_time();
    bool is_serial_set = false;

    

};


