#include <ESP32_RC_Common.h>

void raise_error(String message) {
  Serial.println(String(millis()) + " : ERROR : " + message);
  throw (message);
};

void debug(String message) {
  if (_ESP32_RC_DEBUG) {
    Serial.println(String(millis()) + " : " + message);
  }
};


String mac2str(const uint8_t *mac_addr) {
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  return mac_str;
}


bool is_mac_set(const uint8_t *mac_addr) {
  // Check the ESPNow MAC has been set
  int n = 6;
  while(--n>0 && mac_addr[n]==mac_addr[0]);
  return n!=0;
}