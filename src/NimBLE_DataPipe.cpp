#include "NimBLE_DataPipe.h"

class DataPipeServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer) override {
    DATAPIPE_LOG("[NimBLE-DataPipe] Client Connected");
  };
  void onDisconnect(NimBLEServer *pServer) override {
    DATAPIPE_LOG("[NimBLE-DataPipe] Client Disconnected");
    NimBLEDevice::startAdvertising();
  }
};

NimBLE_DataPipe::NimBLE_DataPipe(const char *deviceName, const char *serviceUuid, const char *charUuid)
    : _deviceName(deviceName), _serviceUuid(serviceUuid), _charUuid(charUuid) {
  _rxBuffer.reserve(2048);
}

void NimBLE_DataPipe::begin() {
  NimBLEDevice::init(_deviceName);
  _pServer = NimBLEDevice::createServer();
  _pServer->setCallbacks(new DataPipeServerCallbacks());

  uint32_t properties = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE;
  if (_useIndication)
    properties |= NIMBLE_PROPERTY::INDICATE;
  else
    properties |= NIMBLE_PROPERTY::NOTIFY;

  NimBLEService *pService = _pServer->createService(_serviceUuid);
  _pChar = pService->createCharacteristic(_charUuid, properties);

  _pChar->setCallbacks(this);
  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(_serviceUuid);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  DATAPIPE_LOG("[NimBLE-DataPipe] Initialized (%s mode)", _useIndication ? "Indicate" : "Notify");
}

void NimBLE_DataPipe::stop() {
  NimBLEDevice::deinit(true);
  DATAPIPE_LOG("[NimBLE-DataPipe] Stopped");
}

bool NimBLE_DataPipe::isConnected() {
  return _pServer && _pServer->getConnectedCount() > 0;
}

uint16_t NimBLE_DataPipe::getMTU() {
  if (!_pServer || !isConnected())
    return 23;

  auto peers = _pServer->getPeerDevices();
  if (peers.empty())
    return 23;

  return _pServer->getPeerMTU(peers[0]);
}

void NimBLE_DataPipe::sendJson(const JsonDocument &doc) {
  if (!isConnected())
    return;
  String payload;
  serializeJson(doc, payload);
  sendInternal(TYPE_JSON, (const uint8_t *)payload.c_str(), payload.length());
}

void NimBLE_DataPipe::sendBinary(uint8_t type, const uint8_t *data, size_t len) {
  sendInternal(type, data, len);
}

void NimBLE_DataPipe::sendInternal(uint8_t type, const uint8_t *payload, size_t len) {
  if (!_pChar || !isConnected())
    return;

  uint16_t rawMTU = getMTU();
  if (rawMTU < 5) return;
  size_t mtu = rawMTU - 4;
  size_t totalLen = len + 3; // Payload + 3-byte header

  uint8_t *buffer = new uint8_t[totalLen];
  buffer[0] = type;
  buffer[1] = len & 0xFF;
  buffer[2] = (len >> 8) & 0xFF;
  memcpy(buffer + 3, payload, len);

  for (size_t i = 0; i < totalLen; i += mtu) {
    size_t chunkSize = min(mtu, totalLen - i);
    _pChar->setValue(buffer + i, chunkSize);

    if (_useIndication) {
      // Indicate blocks until ACK — no throttle needed
      _pChar->indicate();
    } else {
      _pChar->notify();
      // Only throttle on multi-chunk messages; scale with chunk count.
      if (totalLen > mtu) {
        size_t chunkCount = (totalLen + mtu - 1) / mtu;
        uint32_t throttleMs = (chunkCount > 10) ? 10 : 5;
        delay(throttleMs);
      }
    }
  }

  delete[] buffer;
}

void NimBLE_DataPipe::onWrite(NimBLECharacteristic *pCharacteristic) {
  NimBLEAttValue val = pCharacteristic->getValue();
  const uint8_t *data = val.data();
  size_t len = val.length();

  if (len == 0)
    return;

  size_t offset = 0;

  // 1. Handle Header
  if (!_headerReceived) {
    if (len >= 3) {
      _expectedType = data[0];
      _expectedLen = data[1] | (data[2] << 8);
      _headerReceived = true;
      _rxBuffer.clear();
      offset = 3;
    } else {
      // TODO: support fragmented headers in a future version
      DATAPIPE_LOG("[NimBLE-DataPipe] Error: Fragmented header not supported");
      return;
    }
  }

  // 2. Append Data
  size_t remainingToRead = _expectedLen - _rxBuffer.size();
  size_t canRead = min(len - offset, remainingToRead);

  if (canRead > 0) {
    _rxBuffer.insert(_rxBuffer.end(), data + offset, data + offset + canRead);
  }

  // 3. Check if Complete
  if (_rxBuffer.size() >= _expectedLen && _headerReceived) {
    if (_expectedType == TYPE_JSON && _jsonHandler) {
      JsonDocument doc;
      DeserializationError error =
          deserializeJson(doc, _rxBuffer.data(), _rxBuffer.size());
      if (!error)
        _jsonHandler(doc);
      else
        DATAPIPE_LOG("[NimBLE-DataPipe] JSON Error: %s", error.c_str());
    } else if (_binaryHandler) {
      _binaryHandler(_expectedType, _rxBuffer.data(), _rxBuffer.size());
    }

    // Reset for next message
    _headerReceived = false;
    _rxBuffer.clear();
  }
}