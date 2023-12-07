/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 */

#include "BLEDevice.h"
//#include "BLEScan.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>  // Required for 16 MHz Adafruit Trinket
#endif
#include "AiEsp32RotaryEncoder.h"
#include "Arduino.h"

// The remote service we wish to connect to.
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// The characteristic of the remote service we are interested in.
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

int16_t encoderValue = 0;

// Which pin on the Arduino is connected to the NeoPixels?
#define LED_PIN 7  // On Trinket or Gemma, suggest changing this to 1

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 7  // Popular NeoPixel ring size

#define BUZZER 19
#define ROTARY_ENCODER_A_PIN 36
#define ROTARY_ENCODER_B_PIN 37
#define ROTARY_ENCODER_BUTTON_PIN 38
#define ROTARY_ENCODER_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
#define ROTARY_ENCODER_STEPS 1

// use first channel of 16 channels (started from zero)
#define LEDC_CHANNEL_0 0

// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT 13

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ 2000

bool on_Button_down = false;

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

int16_t test_limits = 2;
// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 100  // Time (in milliseconds) to pause between pixels

int led_red();
int led_green();
int led_blue();
int buzzer();

void rotary_loop();
void rotary_onButtonClick();

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  Serial.write(pData, length);
  Serial.println();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remove BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");
  pClient->setMTU(517);  //set client to request maximum MTU from server (default is 23 otherwise)

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");


  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());
  }

  if (pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  connected = true;
  return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    }  // Found our server
  }    // onResult
};     // MyAdvertisedDeviceCallbacks

// 데이터 전송을 위한 함수
void sendStringToServer(String data) {
  if (connected && pRemoteCharacteristic) {
    pRemoteCharacteristic->writeValue(data.c_str(), data.length());
    Serial.println("Data sent to server: " + data);
  } else {
    Serial.println("Not connected to a server.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  // END of Trinket-specific code.

  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)

  // initialize digital pin output.
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  led_blue();  // blue led

  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)

  //we must initialize rotary encoder
  rotaryEncoder.begin();

  rotaryEncoder.setup(
    [] {
      rotaryEncoder.readEncoder_ISR();
    },
    [] {
      on_Button_down = true;
    });
  //optionally we can set boundaries and if values should cycle or not
  bool circleValues = false;
  rotaryEncoder.setBoundaries(0, 1000, circleValues);  //minValue, maxValue, circleValues true|false (when max go to min and vice versa)

  /*Rotary acceleration introduced 25.2.2021.
    * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
    * without accelerateion you need long time to get to that number
    * Using acceleration, faster you turn, faster will the value raise.
    * For fine tuning slow down.
    */
  //rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  rotaryEncoder.setAcceleration(250);  //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
}  // End of setup.


// This is the Arduino main loop function.
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    String newValue = "Time since boot: " + String(millis() / 30);
    //Serial.println("Setting new characteristic value to \"" + newValue + "\"");

    // Set the characteristic's value to be the array of bytes that is actually a string.
    //pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());

    String message = "Hello from client";
    //sendStringToServer(message);
    sendStringToServer((String)encoderValue);
    // Serial.print("Value: ");
    // Serial.println(encoderValue);


  } else if (doScan) {
    BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
  }

  delay(1000);     // Delay a second between loops.

  if (millis() > 20000)
        rotaryEncoder.enable();

    rotary_loop();
}  // End of loop


void rotary_onButtonClick()
{

    if (on_Button_down) {

        static unsigned long lastTimePressed = 0;
        if (millis() - lastTimePressed < 500)
            return; //ignore multiple press in that time milliseconds
        lastTimePressed = millis();
        buzzer();
        Serial.print(" buzzer");

        rotaryEncoder.setBoundaries(0, 1000, false);
        // Serial.print("new boundaries are between minimumn value ");
        // Serial.print(-test_limits);
        // Serial.print(" and maximum value");
        // Serial.println(-test_limits);
        // rotaryEncoder.reset();



        // if (test_limits >= 2048)
        //     test_limits = 2;
        // test_limits *= 2;

        on_Button_down = false;
    }
}

void rotary_loop()
{
    rotary_onButtonClick();
    //lets see if anything changed
    int16_t encoderDelta = rotaryEncoder.encoderChanged();

    //optionally we can ignore whenever there is no change
    if (encoderDelta == 0)
        return;

    //for some cases we only want to know if value is increased or decreased (typically for menu items)
    if (encoderDelta > 0) {
        led_green();
        Serial.print("+");
    }

    if (encoderDelta < 0) {
        led_red();
        Serial.print("-");

    }


    //for other cases we want to know what is current value. Additionally often we only want if something changed
    //example: when using rotary encoder to set termostat temperature, or sound volume etc

    //if value is changed compared to our last read
    if (encoderDelta != 0) {
        //now we need current value
        encoderValue = rotaryEncoder.readEncoder();
        //process new value. Here is simple output.
        Serial.print("Value: ");
        Serial.println(encoderValue);
    }
}

int buzzer()
{

    tone(LEDC_CHANNEL_0, LEDC_BASE_FREQ);
    analogWrite(LEDC_CHANNEL_0, 150);
    delay(300);
    analogWrite(LEDC_CHANNEL_0, 0);
    return 1;
}

int led_red()
{
    pixels.clear(); // Set all pixel colors to 'off'
    // The first NeoPixel in a strand is #0, second is 1, all the way up
    // to the count of pixels minus one.
    for (int i = 0; i < NUMPIXELS; i++) { // For each pixel...
        // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
        // Here we're using a moderately bright green color:
        pixels.setPixelColor(i, pixels.Color(150, 0, 0));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(1); // Pause before next pass through loop
    }

    return 1;
}
int led_green()
{
    pixels.clear(); // Set all pixel colors to 'off'
    // The first NeoPixel in a strand is #0, second is 1, all the way up
    // to the count of pixels minus one.
    for (int i = 0; i < NUMPIXELS; i++) { // For each pixel...
        // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
        // Here we're using a moderately bright green color:
        pixels.setPixelColor(i, pixels.Color(0, 150, 0));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(1); // Pause before next pass through loop
    }
    return 1;
}
int led_blue()
{

    pixels.clear(); // Set all pixel colors to 'off'
    // The first NeoPixel in a strand is #0, second is 1, all the way up
    // to the count of pixels minus one.
    for (int i = 0; i < NUMPIXELS; i++) { // For each pixel...
        // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
        // Here we're using a moderately bright green color:
        pixels.setPixelColor(i, pixels.Color(0, 0, 150));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(1); // Pause before next pass through loop
    }
    return 1;
}

