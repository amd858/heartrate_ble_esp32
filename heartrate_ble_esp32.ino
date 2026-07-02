#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// Standard Bluetooth SIG UUIDs for Heart Rate Profile
static BLEUUID serviceUUID("180d");         // Heart Rate Service
static BLEUUID charUUID("2a37");            // Heart Rate Measurement Characteristic

static bool doConnect = false;
static bool connected = false;
static bool doScan = false;
static BLEAdvertisedDevice* myDevice = nullptr;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

// Callback function triggered whenever the Magene sends data
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    
    if (length >= 2) {
        uint8_t flags = pData[0];
        uint16_t heartRate = 0;
        
        // Check if Heart Rate Value Format is 8-bit or 16-bit
        if (flags & 0x01) {
            heartRate = pData[1] | (pData[2] << 8);
        } else {
            heartRate = pData[1];
        }
        
        Serial.print("Heart Rate Received: ");
        Serial.print(heartRate);
        Serial.println(" BPM");
    }
}

// Client status change listener
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) override {
    Serial.println("Connection handshaking successful.");
  }

  void onDisconnect(BLEClient* pClient) override {
    connected = false;
    Serial.println("Disconnected from Magene Monitor.");
  }
};

// Handle server initialization and characteristic pairing
bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient* pClient = BLEDevice::createClient();
    Serial.println(" - Created native BLE client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remote BLE server
    if (!pClient->connect(myDevice)) {
        Serial.println(" - Failed to establish connection matrix.");
        delete pClient;
        return false;
    }
    Serial.println(" - Connected to device");

    // Extract Heart Rate Core Service 
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find target Service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      delete pClient;
      return false;
    }
    Serial.println(" - Found Heart Rate Service");

    // Extract Data Characteristic Stream
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find target Characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      delete pClient;
      return false;
    }
    Serial.println(" - Found Heart Rate Measurement Characteristic");

    // Connect the data notification stream pipeline and write descriptors explicitly
    if (pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
      
      // EXPLICIT FIX: Force the CCCD Descriptor to turn on notifications (0x0001)
      BLERemoteDescriptor* pDescriptor = pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902));
      if (pDescriptor != nullptr) {
          uint8_t val[] = {0x01, 0x00};
          pDescriptor->writeValue(val, 2, true);
          Serial.println(" - Successfully registered and forced CCCD Notifications ON");
      } else {
          Serial.println(" - Warning: CCCD Descriptor (0x2902) not found");
      }
    }

    connected = true;
    return true;
}

// Global environmental scanner loop listener
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    Serial.print("BLE Device spotted: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Native BLE Client for ESP32-C3...");
  BLEDevice::init("ESP32-C3-HRM");

  // Spin up scanning parameters
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the Magene Strap.");
    } else {
      Serial.println("Connection initialization sequence failed.");
    }
    doConnect = false;
  }

  if (!connected && doScan) {
    BLEDevice::getScan()->start(0);  // Loop scanning engine if disconnected
  }
  
  delay(1000);
}
