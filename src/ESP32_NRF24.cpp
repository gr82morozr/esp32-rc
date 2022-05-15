#include <Arduino.h>
#include <ESP32_NRF24.h>

ESP32_NRF24::ESP32_NRF24 (int r) {
  this->role = r;
  switch (this->role) {
    case _ROLE_CONTROLLER :
      this->address = (uint8_t *) "node2";
      break;
    case _ROLE_EXECUTOR :
      this->address = (uint8_t *) "node1";
      break;
    default : 
      raise_error("Invalid Role");
      break;
  }

}


void ESP32_NRF24::init () {
  this->radio = new RF24(NRF24_CE, HSPI_CS);
  SPI.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);

  while (! this->radio->begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    delay (1000); 
  };

  this->radio->setPALevel(RF24_PA_LOW); 
  //this->radio->setDataRate(RF24_250KBPS);  // For longest range
  this->radio->enableDynamicPayloads();    // ACK payloads are dynamically sized
  //this->radio->enableAckPayload();

  // set the TX address of the RX node into the TX pipe
  this->radio->openWritingPipe((uint8_t *) "node2");     // always uses pipe 0

  // set the RX address of the TX node into a RX pipe
  this->radio->openReadingPipe(1, (uint8_t *) "node1"); // using pipe 1
  this->radio->printPrettyDetails();
}


void ESP32_NRF24::connect () {

  switch (this->role) {
    case _ROLE_CONTROLLER :
      this->radio->stopListening();  // put radio in TX mode
      break;
    case _ROLE_EXECUTOR :
      this->radio->startListening(); // put radio in RX mode
      break;
    default : 
      raise_error("Invalid Role");
      break;
  }

}


void ESP32_NRF24::send(String data) {
  
  bool report = this->radio->write(&data, sizeof( data ));    // transmit & save the report

   if (report) {
      Serial.print(F("Transmission successful! "));           // payload was delivered
      Serial.println(data);                                   // print payload sent
  } else {
      Serial.println(F("Transmission failed or timed out"));  // payload was not delivered
  }
}


String ESP32_NRF24::recv () {
  uint8_t pipe;
  String data;
  if (this->radio->available(&pipe)) {                      // is there a payload? get the pipe number that recieved it
      uint8_t bytes = this->radio->getDynamicPayloadSize(); // get the size of the payload
      this->radio->read(&data, bytes);                      // fetch payload from FIFO
      Serial.print(F("Received "));
      Serial.print(bytes);                                  // print the size of the payload
      Serial.print(F(" bytes on pipe "));
      Serial.print(pipe);                                   // print the pipe number
      Serial.print(F(": "));
      Serial.println(data);                                 // print the payload's value
    }
  return data;
}


bool ESP32_NRF24::check() {
  return true;
}
