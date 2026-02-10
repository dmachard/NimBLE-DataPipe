/**
 * @file BleConfigExample.ino
 * @brief Configuration example using JSON over NimBLE-DataPipe.
 */

#include <NimBLE-DataPipe.h>

NimBLE_DataPipe pipe("ESP32-Config-Pipe", "1234", "5678");

void setup() {
  Serial.begin(115200);

  // Specialized JSON handler
  pipe.setOnJson([](const JsonDocument &doc) {
    if (doc["cmd"] == "get_config") {
      JsonDocument res;
      res["ssid"] = "MyWiFi";
      res["status"] = "connected";
      pipe.sendJson(res);
    }
  });

  pipe.begin();
}

void loop() { delay(1000); }
