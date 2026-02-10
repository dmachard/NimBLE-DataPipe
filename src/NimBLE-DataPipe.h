#ifndef NIMBLE_DATA_PIPE_H
#define NIMBLE_DATA_PIPE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <functional>
#include <vector>

// can be disabled by defining DATAPIPE_SILENT
#ifndef DATAPIPE_SILENT
#define DATAPIPE_LOG(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#else
#define DATAPIPE_LOG(fmt, ...)
#endif

class NimBLE_DataPipe : public NimBLECharacteristicCallbacks {
public:
  typedef std::function<void(uint8_t type, const uint8_t *data, size_t len)>
      BinaryHandler;
  typedef std::function<void(const JsonDocument &doc)> JsonHandler;

  static const uint8_t TYPE_JSON = 0x00;

  NimBLE_DataPipe(const char *deviceName, const char *serviceUuid,
                  const char *charUuid);

  void begin();
  void stop();

  void sendBinary(uint8_t type, const uint8_t *data, size_t len);
  void sendJson(const JsonDocument &doc);

  void setOnBinary(BinaryHandler handler) { _binaryHandler = handler; }
  void setOnJson(JsonHandler handler) { _jsonHandler = handler; }

  void setUseIndication(bool use) { _useIndication = use; }

  bool isConnected();
  uint16_t getMTU();

  void onWrite(NimBLECharacteristic *pCharacteristic,
               NimBLEConnInfo &connInfo) override;

private:
  const char *_deviceName;
  const char *_serviceUuid;
  const char *_charUuid;

  NimBLEServer *_pServer = nullptr;
  NimBLECharacteristic *_pChar = nullptr;

  BinaryHandler _binaryHandler = nullptr;
  JsonHandler _jsonHandler = nullptr;

  std::vector<uint8_t> _rxBuffer;
  uint8_t _expectedType = 0;
  size_t _expectedLen = 0;
  bool _headerReceived = false;
  bool _useIndication = false;

  void sendInternal(uint8_t type, const uint8_t *payload, size_t len);
};

#endif // NIMBLE_DATA_PIPE_H
