#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET 4

const char *OWONNAME = "B35T+";
static BLEUUID serviceUUID("0000fff0-0000-1000-8000-00805f9b34fb");
static BLEUUID charnotificationUUID("0000fff4-0000-1000-8000-00805f9b34fb");
static BLEAddress *address;
static BLERemoteCharacteristic *characteristicNotify;
const uint16_t scanTime = 10;
static unsigned long startBleScanning = 0;
volatile boolean deviceBleConnected = false;
const uint8_t replySize = 6;
char sortData[replySize];
volatile boolean newData = false;

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void display(uint8_t textSize, int16_t x, int16_t y, String text)
{
  oled.clearDisplay();
  oled.setTextSize(textSize);
  oled.setTextColor(1);
  oled.setCursor(x, y);
  oled.print(text);
  oled.display();
}

class DeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice device)
  {
    ESP_LOGI(TAG, "BLE scan stop\n");
    if (device.haveName() && strcmp(device.getName().c_str(), OWONNAME) == 0)
    {
      device.getScan()->stop();
      address = new BLEAddress(device.getAddress());
      ESP_LOGI(TAG, "BLE device found (%s at addr: %s)\n", OWONNAME, address->toString().c_str());
    }
    else
    {
      ESP_LOGI(TAG, "BLE device not found\n");
    }
  }
};

static void notifyCallback(BLERemoteCharacteristic *remoteCharacteristic, uint8_t *data, size_t length, bool isNotify)
{

  if (isNotify == true && length == replySize && remoteCharacteristic->getUUID().equals(charnotificationUUID))
  {

    if (memcmp(sortData, data, replySize) != 0)
    {
      if (newData == false)
      {

        sortData[0] = data[1];
        sortData[1] = data[0];
        sortData[2] = data[3];
        sortData[3] = data[2];
        sortData[4] = data[5];
        sortData[5] = data[4];

        // offline_function = ((uint16_t)sortData[4] << 8) | sortData[5];

        // ESP_LOGI(TAG,"%02X ", (offline_function >> 6) & 0x0f);

        // Serial.println(int((offline_function >> 6) & 0x0f));
        // Serial.println(int((offline_function >> 3) & 0x07));

        // Serial.println(float(offline_function) / pow(10.0, 1));

        newData = true;
      }
    }
  }
}

class Callbacks : public BLEClientCallbacks
{
  void onConnect(BLEClient *client)
  {
    deviceBleConnected = true;
    ESP_LOGI(TAG, "%s connected\n", OWONNAME);
  };

  void onDisconnect(BLEClient *client)
  {
    client->disconnect();
    deviceBleConnected = false;
    ESP_LOGI(TAG, "%s disconnected\n", OWONNAME);
  }
};

bool connectServer(BLEAddress Address)
{
  ESP_LOGI(TAG, "Create a connection to addr: %s\n", Address.toString().c_str());
  BLEClient *client = BLEDevice::createClient();
  ESP_LOGI(TAG, " - Client created\n");
  client->setClientCallbacks(new Callbacks());
  ESP_LOGI(TAG, " - Connecting to server...\n");
  client->connect(Address);
  BLERemoteService *service = client->getService(serviceUUID);
  if (service == nullptr)
  {
    ESP_LOGI(TAG, " - Service not found (UUID: %s)\n", serviceUUID.toString().c_str());
    return false;
  }
  else
  {
    ESP_LOGI(TAG, " - Service found (UUID: %s)\n", serviceUUID.toString().c_str());
  }

  characteristicNotify = service->getCharacteristic(charnotificationUUID);
  if (characteristicNotify == nullptr)
  {
    ESP_LOGI(TAG, " - Notify characteristic not found (UUID: %s)\n", charnotificationUUID.toString().c_str());
    return false;
  }
  else
  {
    ESP_LOGI(TAG, " - Notify characteristic found (UUID: %s)\n", charnotificationUUID.toString().c_str());
  }
  characteristicNotify->registerForNotify(notifyCallback);
  return true;
}

void doScan()
{
  ESP_LOGI(TAG, "Scan start\n");
  startBleScanning = millis();
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new DeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(scanTime);
}

void displayValues()
{
  uint16_t offline_function = ((uint16_t)sortData[0] << 8) | sortData[1];
  uint16_t offline_encodes = ((uint16_t)sortData[2] << 8) | sortData[3];
  uint16_t offline_measurement = ((uint16_t)sortData[4] << 8) | sortData[5];

  int function, scale, decimal, typeOne, typeTwo;
  char measurement[20];

  function = (offline_function >> 6) & 0x0f;
  scale = (offline_function >> 3) & 0x07;
  decimal = offline_function & 0x07;
  typeOne = (offline_encodes >> 4) & 0x07;
  typeTwo = (offline_encodes) & 0x07;

  oled.clearDisplay();
  oled.setTextColor(1);

  if (typeOne >= 2)
  {
    oled.setTextSize(1);
    oled.setCursor(110, 1);
    oled.print("MAX");
    typeOne = typeOne - 2;
  }
  if (typeOne >= 1)
  {
    oled.setTextSize(1);
    oled.setCursor(110, 1);
    oled.print("MIN");
    typeOne = typeOne - 1;
  }
  if (typeTwo >= 8)
  {
    oled.setTextSize(1);
    oled.setCursor(60, 55);
    oled.print("Low Battery");
    typeTwo = typeTwo - 8;
  }
  if (typeTwo >= 4)
  {
    oled.setTextSize(1);
    oled.setCursor(1, 1);
    oled.print("AUTO");
    typeTwo = typeTwo - 4;
  }
  if (typeTwo >= 2)
  {
    oled.setTextSize(1);
    oled.setCursor(1, 1);
    oled.print("Delta");
    typeTwo = typeTwo - 2;
  }
  if (typeTwo >= 1)
  {
    oled.setTextSize(1);
    oled.setCursor(102, 1);
    oled.print("Hold");
    typeTwo = typeTwo - 1;
  }

  switch (scale)
  {
  case 1:
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("n");
    break;

  case 2:
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("u");
    break;

  case 3:
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("m");
    break;

  case 5:
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("k");
    break;

  case 6:
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("M");
    break;
  }

  switch (function)
  {

  case 0:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("DC");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("V");
    break;

  case 1:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("AC");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("V");
    break;

  case 2:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("DC");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("A");
    break;

  case 3:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("AC");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("A");
    break;

  case 4:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("R");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("O");
    break;

  case 5:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("C");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("F");
    break;

  case 6:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("F");
    oled.setTextSize(2);
    oled.setCursor(104, 25);
    oled.print("Hz");
    break;

  case 7:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("%%");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("%");
    break;

  case 8:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("T");
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("C");
    break;

  case 9:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("T");
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("F");
    break;

  case 10:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("D");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("V");
    break;

  case 11:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("SP");
    oled.setTextSize(2);
    oled.setCursor(105, 25);
    oled.print("O");
    break;

  case 12:
    oled.setTextSize(1);
    oled.setCursor(1, 30);
    oled.print("D");
    oled.setTextSize(2);
    oled.setCursor(92, 25);
    oled.print("hFE");
    break;
  }

  if (decimal > 3)
  {
    decimal = 1;
  }

  if (offline_measurement < 0x7fff)
  {
    dtostrf((float)offline_measurement / pow(10.0, decimal), 1, decimal, measurement);
  }
  else
  {
    dtostrf(-1 * (float)(offline_measurement & 0x7fff) / pow(10.0, decimal), 1, decimal, measurement);
  }

  oled.setTextSize(2);
  oled.setCursor(17, 25);
  oled.print(measurement);

  oled.display();
  newData = false;
}

void setup()
{
  Serial.begin(115200);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    ESP_LOGI(TAG, "SSD1306 allocation failed");
    for (;;)
      ;
  }

  ESP_LOGI(TAG, "Start OWON B35T+ Client\n");
  BLEDevice::init("");

  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(1);
  oled.setCursor(20, 15);
  oled.print("Glasses");
  oled.setCursor(30, 35);
  oled.print("Ready");
  oled.display();
}

void loop()
{
  if (deviceBleConnected == false)
  {
    delay(250);
    if (startBleScanning != 0 && millis() > (startBleScanning + (scanTime * 1000)))
    {
      startBleScanning = 0;
      return;
    }

    if (startBleScanning == 0)
    {
      doScan();
      return;
    }
    startBleScanning = 0;
    connectServer(*address);
  }
  else
  {
    if (newData == true)
    {
      displayValues();
    }
  }
}
