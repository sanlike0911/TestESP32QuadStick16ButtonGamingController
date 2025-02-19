#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"

#define ENABLED_LOGICAL_VALUE_2BYTE

BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;

bool deviceConnected = false;

uint16_t buttons = 0;

#ifdef ENABLED_LOGICAL_VALUE_2BYTE
uint16_t leftStickX = 0, leftStickY = 0;
uint16_t rightStickX = 0, rightStickY = 0;
#else
int8_t leftStickX = 0, leftStickY = 0;
int8_t rightStickX = 0, rightStickY = 0;
#endif

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 1000; // 50ms間隔で更新

class MyCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Disconnected");
        BLEDevice::startAdvertising();
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    BLEDevice::init("CKD ESP32 Gamepad v1");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyCallbacks());

    hid = new BLEHIDDevice(pServer);
    input = hid->inputReport(1);
    output = hid->outputReport(1);

    hid->manufacturer()->setValue("Espressif");
    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    // hid->manufacturer()->setValue("Microsoft");
    // hid->pnp(0x01, 0x045E, 0x02FD, 0x0100);
    hid->hidInfo(0x00, 0x0A);

    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

    const uint8_t reportMap[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x05,        // Usage (Game Pad)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x01,        //   Report ID (1)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x10,        //   Usage Maximum (0x10)  // 16個のボタンに対応
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x10,        //   Report Count (16)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
        0x09, 0x30,        //   Usage (X)  - 左スティックX
        0x09, 0x31,        //   Usage (Y)  - 左スティックY
        0x09, 0x32,        //   Usage (X)  - 右スティックX
        0x09, 0x33,        //   Usage (Y) - 右スティックY
#ifdef ENABLED_LOGICAL_VALUE_2BYTE
        0x15, 0x00, 0x00,  //   Logical Minimum  (0)
        0x26, 0xFF, 0xFF,  //   Logical Maximum  (65535)
        0x75, 0x10,        //   Report Size (16)
#else
        0x15, 0x81,        //   Logical Minimum (-127)
        0x25, 0x7F,        //   Logical Maximum (127)
        0x75, 0x08,        //   Report Size (8)
#endif
        0x95, 0x04,        //   Report Count (4)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xC0               // End Collection
    };

    const uint8_t *pReport = reportMap;
    hid->reportMap((uint8_t*)pReport, sizeof(reportMap));
    hid->startServices();
    hid->setBatteryLevel(100);

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(HID_GAMEPAD);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();

    Serial.println("Waiting for a client connection to notify...");
}

void updateGamepadState() {
    // ボタンの状態を更新（0から15のボタンを順番に押す）
    static int currentButton = 0;
    buttons = 1 << currentButton;
    currentButton = (currentButton + 1) % 16;

    // ジョイスティックの位置を円を描くように更新
    static float angle = 0;
#ifdef ENABLED_LOGICAL_VALUE_2BYTE
    leftStickX = static_cast<uint16_t>(32767 * cos(angle));
    leftStickY = static_cast<uint16_t>(32767 * sin(angle));
    rightStickX = static_cast<uint16_t>(32767 * cos(angle + PI / 2));
    rightStickY = static_cast<uint16_t>(32767 * sin(angle + PI / 2));
#else
    leftStickX = static_cast<int8_t>(127 * cos(angle));
    leftStickY = static_cast<int8_t>(127 * sin(angle));
    rightStickX = static_cast<int8_t>(127 * cos(angle + PI / 2));
    rightStickY = static_cast<int8_t>(127 * sin(angle + PI / 2));
#endif
    angle += 0.2;
    if (angle > 2 * PI) angle -= 2 * PI;
}

void loop() {
    if (deviceConnected) {
        unsigned long currentTime = millis();
        if (currentTime - lastUpdateTime >= updateInterval) {
            updateGamepadState();

            // レポートを送信
            uint8_t report[] = {
              buttons & 0xFF, (buttons >> 8) & 0xFF,
#ifdef ENABLED_LOGICAL_VALUE_2BYTE
              static_cast<uint8_t>(leftStickX & 0xFF), static_cast<uint8_t>((leftStickX >> 8) & 0xFF),
              static_cast<uint8_t>(leftStickY & 0xFF), static_cast<uint8_t>((leftStickY >> 8) & 0xFF),
              static_cast<uint8_t>(rightStickX & 0xFF), static_cast<uint8_t>((rightStickX >> 8) & 0xFF),
              static_cast<uint8_t>(rightStickY & 0xFF), static_cast<uint8_t>((rightStickY >> 8) & 0xFF)
#else
              static_cast<int8_t>(leftStickX), static_cast<int8_t>(leftStickY),
              static_cast<int8_t>(rightStickX), static_cast<int8_t>(rightStickY)
#endif
            };
            input->setValue(report, sizeof(report));
            input->notify();

            lastUpdateTime = currentTime;
        }
    }
}
