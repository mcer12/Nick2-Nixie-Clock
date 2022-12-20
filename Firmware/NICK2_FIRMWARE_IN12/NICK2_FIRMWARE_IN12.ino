/*
   GNU General Public License v3.0
   Copyright (c) 2022 Martin Cerny
*/

#include <Arduino.h>

#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <math.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <DNSServer.h>
//#include <ESPmDNS.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <WiFiUdp.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <Ticker.h>

#include <TimeLib.h>
#include <Timezone.h>

// Pick a clock version below!
//#define CLOCK_VERSION_IV6
//#define CLOCK_VERSION_IV12
#define CLOCK_VERSION_IN12

#if !defined(CLOCK_VERSION_IN12) && !defined(CLOCK_VERSION_IN16) && !defined(CLOCK_VERSION_ZM1080)
#error "You have to select a clock version! Line 25"
#endif

#define AP_NAME "NICK2_"
#define FW_NAME "NICK2"
#define FW_VERSION "1.0.0"
#define CONFIG_TIMEOUT 300000 // 300000 = 5 minutes

// ONLY CHANGE DEFINES BELOW IF YOU KNOW WHAT YOU'RE DOING!
#define NORMAL_MODE 0
#define OTA_MODE 1
#define CONFIG_MODE 2
#define CONFIG_MODE_LOCAL 3
#define CONNECTION_FAIL 4
#define UPDATE_SUCCESS 1
#define UPDATE_FAIL 2
#define DATA 13
#define CLOCK 14
#define LATCH 15
#define TIMER_INTERVAL_uS 50 // 200 = safe value for 6 digits. You can go down to 150 for 4-digit one. Going too low will cause crashes.

// User global vars
const char* dns_name = "nick2"; // only for AP mode
const char* update_path = "/update";
const char* update_username = "nick2";
const char* update_password = "nick2";
const char* ntpServerName = "pool.ntp.org";

const int dotsAnimationSteps = 2000; // dotsAnimationSteps * TIMER_INTERVAL_uS = one animation cycle time in microseconds
const uint8_t PixelCount = 5; // Addressable LED count

HsbColor red[] = {
  HsbColor(RgbColor(100, 0, 0)), // LOW
  HsbColor(RgbColor(150, 0, 0)), // MEDIUM
  HsbColor(RgbColor(200, 0, 0)), // HIGH
};
HsbColor green[] = {
  HsbColor(RgbColor(0, 100, 0)), // LOW
  HsbColor(RgbColor(0, 150, 0)), // MEDIUM
  HsbColor(RgbColor(0, 200, 0)), // HIGH
};
HsbColor blue[] = {
  HsbColor(RgbColor(0, 0, 100)), // LOW
  HsbColor(RgbColor(0, 0, 150)), // MEDIUM
  HsbColor(RgbColor(0, 0, 200)), // HIGH
};
HsbColor yellow[] = {
  HsbColor(RgbColor(100, 100, 0)), // LOW
  HsbColor(RgbColor(150, 150, 0)), // MEDIUM
  HsbColor(RgbColor(200, 200, 0)), // HIGH
};
HsbColor purple[] = {
  HsbColor(RgbColor(100, 0, 100)), // LOW
  HsbColor(RgbColor(150, 0, 150)), // MEDIUM
  HsbColor(RgbColor(200, 0, 200)), // HIGH
};
HsbColor azure[] = {
  HsbColor(RgbColor(0, 100, 100)), // LOW
  HsbColor(RgbColor(0, 150, 150)), // MEDIUM
  HsbColor(RgbColor(0, 200, 200)), // HIGH
};

#if defined(CLOCK_VERSION_IV6)
HsbColor colonColorDefault[] = {
  HsbColor(RgbColor(30, 70, 50)), // LOW
  HsbColor(RgbColor(50, 100, 80)), // MEDIUM
  HsbColor(RgbColor(80, 130, 100)), // HIGH
};
#else
HsbColor colonColorDefault[] = {
  HsbColor(RgbColor(30, 70, 50)), // LOW
  HsbColor(RgbColor(50, 100, 80)), // MEDIUM
  HsbColor(RgbColor(120, 220, 140)), // HIGH
};
/*
  RgbColor colonColorDefault[] = {
  RgbColor(30, 70, 50), // LOW
  RgbColor(50, 100, 80), // MEDIUM
  RgbColor(100, 200, 120), // HIGH
  };
*/
#endif

/*
  RgbColor colonColorDefault[] = {
  RgbColor(30, 6, 1), // LOW
  RgbColor(38, 8, 2), // MEDIUM
  RgbColor(50, 10, 2), // HIGH
  };
*/
RgbColor currentColor = RgbColor(0, 0, 0);
//RgbColor colonColorDefault = RgbColor(90, 27, 7);
//RgbColor colonColorDefault = RgbColor(38, 12, 2);


const int digitsCount = 4;
uint8_t anodes_pins[] = {33, 26, 25, 32}; // Anode drivers for multiplexing
uint8_t SN7414_pins[] = {4, 17, 5, 16}; // A, B, C, D
uint8_t numbers[10][4] = // SN7414 truth table
{
  {0, 1, 0, 0}, // 0
  {0, 0, 0, 0}, // 1
  {1, 0, 0, 0}, // 2
  {1, 0, 1, 0}, // 3
  {0, 0, 1, 0}, // 4
  {0, 1, 1, 0}, // 5
  {1, 1, 0, 0}, // 6
  {1, 1, 1, 0}, // 7
  {0, 0, 0, 1}, // 8
  {1, 0, 0, 1}, // 9
};
// this is a very crude way to set brightness with multiplex
uint16_t bri_delay_blank[] = {600, 200, 50}; // blank period needs to be long enough to prevent ghosting
uint16_t bri_delay_active[] = {150, 150, 600}; // active period needs to be long enough to assure uniformly lit digits
// Cathode poisoning prevention pattern --> circle through least used digits, prioritize number 7
uint8_t healPattern[6][10] = {
  {0, 6, 9, 3, 4, 5, 6, 7, 8, 9},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
  {7, 3, 4, 7, 5, 6, 7, 8, 7, 9},
  {4, 5, 6, 7, 8, 9, 6, 7, 8, 9},
  {6, 7, 8, 7, 9, 6, 7, 8, 9, 7},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
};


// Better left alone global vars
TaskHandle_t Task1;
volatile bool isPoweredOn = true;
unsigned long configStartMillis, prevDisplayMillis;
volatile int activeDot;
uint8_t deviceMode = NORMAL_MODE;
bool timeUpdateFirst = true;
volatile bool toggleSeconds;
bool breatheState;
byte mac[6];
volatile int transitionState = 0;
volatile uint8_t activeDigits[] = {0, 0, 0, 0, 0, 0};
volatile uint8_t targetDigits[] = {0, 0, 0, 0, 0, 0};
volatile byte bytes[digitsCount];
volatile byte prevBytes[digitsCount];
volatile uint8_t bri = 0;
uint8_t healIterator;
uint8_t timeUpdateStatus = 0; // 0 = no update, 1 = update success, 2 = update fail,
uint8_t failedAttempts = 0;
volatile bool enableDotsAnimation;
volatile unsigned short dotsAnimationState;
RgbColor colonColor;
IPAddress ip_addr;

TimeChangeRule EDT = {"EDT", Last, Sun, Mar, 1, 120};  //UTC + 2 hours
TimeChangeRule EST = {"EST", Last, Sun, Oct, 1, 60};  //UTC + 1 hours
Timezone TZ(EDT, EST);
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s0Ws2812xMethod> strip(PixelCount, 3);
NeoGamma<NeoGammaTableMethod> colorGamma;
NeoPixelAnimator animations(PixelCount);
DynamicJsonDocument json(2048); // config buffer
Ticker onceTicker;
Ticker colonTicker;
DNSServer dnsServer;
WebServer server(80);
WiFiUDP Udp;
HTTPUpdateServer httpUpdateServer;
unsigned int localPort = 8888;  // local port to listen for UDP packets


// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin(115200);
  Serial.println("");
  Serial.print("Code running on core ");
  Serial.println(xPortGetCoreID());

  if (!LittleFS.begin(true)) {
    Serial.println("[CONF] Failed to mount file system");
  }
  readConfig();

  initStrip();
  initRgbColon();
  initScreen();

  WiFi.macAddress(mac);

  const char* ssid = json["ssid"].as<const char*>();
  const char* pass = json["pass"].as<const char*>();
  const char* ip = json["ip"].as<const char*>();
  const char* gw = json["gw"].as<const char*>();
  const char* sn = json["sn"].as<const char*>();

  if (ssid != NULL && pass != NULL && ssid[0] != '\0' && pass[0] != '\0') {
    Serial.println("[WIFI] Connecting to: " + String(ssid));
    WiFi.mode(WIFI_STA);

    if (ip != NULL && gw != NULL && sn != NULL && ip[0] != '\0' && gw[0] != '\0' && sn[0] != '\0') {
      IPAddress ip_address, gateway_ip, subnet_mask;
      if (!ip_address.fromString(ip) || !gateway_ip.fromString(gw) || !subnet_mask.fromString(sn)) {
        Serial.println("[WIFI] Error setting up static IP, using auto IP instead. Check your configuration.");
      } else {
        WiFi.config(ip_address, gateway_ip, subnet_mask);
      }
    }
    // serializeJson(json, Serial);

    enableDotsAnimation = true; // Start the dots animation

    updateColonColor(yellow[bri]);
    strip_show();

    WiFi.hostname(AP_NAME + macLastThreeSegments(mac));
    WiFi.begin(ssid, pass);

    //startBlinking(200, colorWifiConnecting);

    for (int i = 0; i < 1000; i++) {
      if (WiFi.status() != WL_CONNECTED) {
        if (i > 200) { // 20s timeout
          enableDotsAnimation = false;
          deviceMode = CONFIG_MODE;
          updateColonColor(red[bri]);
          strip_show();
          Serial.print("[WIFI] Failed to connect to: " + String(ssid) + ", going into config mode.");
          delay(500);
          break;
        }
        delay(100);
      } else {
        updateColonColor(green[bri]);
        enableDotsAnimation = false;
        strip_show();
        Serial.print("[WIFI] Successfully connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("[WIFI] Mac address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("[WIFI] IP address: ");
        Serial.println(WiFi.localIP());
        delay(1000);
        break;
      }
    }
  } else {
    deviceMode = CONFIG_MODE;
    Serial.println("[CONF] No credentials set, going to config mode.");
  }

  if (deviceMode == CONFIG_MODE || deviceMode == CONNECTION_FAIL) {
    startConfigPortal(); // Blocking loop
  } else {
    ndp_setup();
    startServer();
  }

  //initScreen();

  if (json["rst_cycle"].as<unsigned int>() == 1) {
    cycleDigits();
    delay(500);
  }

  if (json["rst_ip"].as<unsigned int>() == 1) {
    showIP(5000);
    delay(500);
  }

  /*
    if (!MDNS.begin(dns_name)) {
      Serial.println("[ERROR] MDNS responder did not setup");
    } else {
      Serial.println("[INFO] MDNS setup is successful!");
      MDNS.addService("http", "tcp", 80);
    }
  */
}

// the loop function runs over and over again forever
void loop() {

  if (timeUpdateFirst == true && timeUpdateStatus == UPDATE_FAIL || deviceMode == CONNECTION_FAIL) {
    setAllDigitsTo(0);
    updateColonColor(red[bri]); // red
    strip_show();
    delay(10);
    return;
  }

  if (millis() - prevDisplayMillis >= 1000) { //update the display only if time has changed
    prevDisplayMillis = millis();
    toggleNightMode();

    if (
      (json["cathode"].as<int>() == 1 && (hour() >= 2 && hour() <= 6) && minute() < 10) ||
      (json["cathode"].as<int>() == 2 && (((hour() >= 2 && hour() <= 6) && minute() < 10) || minute() < 1))
    ) {
      healingCycle(); // do healing loop if the time is right :)
    } else {

      if (timeUpdateStatus) {
        if (timeUpdateStatus == UPDATE_SUCCESS) {
          setTemporaryColonColor(5, green[bri]);
        }
        if (timeUpdateStatus == UPDATE_FAIL) {
          if (failedAttempts > 2) {
            colonColor = red[bri];
          } else {
            setTemporaryColonColor(5, red[bri]);
          }
        }
        timeUpdateStatus = 0;
      }

      handleColon();
      showTime();

    }
  }
  animations.UpdateAnimations();
  strip_show();

  //MDNS.update();
  server.handleClient();
  delay(1); // Keeps the ESP cold!

}
