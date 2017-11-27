#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <FS.h>

#define VPIN_VBAT       V0
#define VPIN_POWER      V1
#define PIN_POWER       D2
#define PIN_WIFI_CONFIG D1

/* Full scale on the ADC is 1V, we care about an SLA battery that will have a scale of 14V.
   The Wemos D1 is already populated with a voltage divider for 3.3V consisting of 200k and 100k resistors.
   We add a jumper straight to the A0 pin on the ESP8266 that connects to our voltage divider.
   This bypasses the high side (but not the low side) of the voltage divider on the board, so there will remain a 100k resistor between A0 and ground already on the board.
   I had lying around 127k and 10k resistors that I used for the external divider.
   The ESP8266 ADC is not very good so you may need an error correction factor (which should be close to 1). Determine this using a multimeter if desired.
*/
#define ESP_8266_ADC_ERRC 1.053 // This may vary per a particular board
#define R_DIV_HIGH        127.
#define R_DIV_LOW_EXT     10.
#define R_DIV_LOW_BOARD   100.
#define R_DIV_LOW         (1. / ((1. / R_DIV_LOW_EXT) + (1. / R_DIV_LOW_BOARD)))
#define ESP_8266_ADC_FULL 1023.

static const float vScale = (R_DIV_HIGH / R_DIV_LOW + 1) / ESP_8266_ADC_FULL * ESP_8266_ADC_ERRC;
// the battery voltage below which the load should be disabled
static const float vBatMin = 11.;
// the battery voltage above which the load can be re-enabled
static const float vBatEnable = 12.;

// This is what gets displayed in the config portal. Do not put a Blynk token here.
char blynk_token[34] = "Blynk Device Token";
#define CONFIG_AP_NAME "Switch Config"

extern "C" {
#include "user_interface.h"
}

// STATE VARIABLES
static bool   appConnected      = false;
static bool   powerState        = false;
static bool   powerRequested    = false;
static float  vBat              = 10.;
static bool   shouldSaveConfig  = false;
static BlynkTimer timer;

// LOGIC

// Blynk plots only record data sent from the board (and not the app), which is probably a bug. Force the update as needed.
void updateButton() {
  Blynk.virtualWrite(VPIN_POWER, powerRequested);
}

void powerOn() {
  if (!powerState) {
    digitalWrite(PIN_POWER, HIGH);
    updateButton();
    BLYNK_LOG("Power on");
    powerState = true;
  }
}

void powerOff() {
  if (powerRequested) {
    powerRequested = false;
    updateButton();
  }
  if (powerState) {
    digitalWrite(PIN_POWER, LOW);
    BLYNK_LOG("Power off");
    powerState = false;
  }
}

void checkPower() {
  if (powerRequested) {
    if (vBat < vBatMin) {
      powerOff();
    }
    else if (vBat > vBatEnable) {
      powerOn();
    }
    else if (!powerState) {
      // for button bounce-back
      powerOff();
    }
  } else {
    powerOff();
  }
}

void sampleVBat() {
  // The ESP8266 ADC is awful and has a lot of jitter.
  // Use an EWMA to smooth out the readings.
  float vBatSample = analogRead(A0) * vScale;
  vBat = 0.7 * vBat + 0.3 * vBatSample;
}

void writeVBat() {
  // Only display 50mV precision. More detail than that will just be noise with the awful onboard ADC.
  float vBatRound = roundf(vBat * 20.) / 20.;
  Blynk.virtualWrite(VPIN_VBAT, vBatRound);
}

//////////////////////////
// APP REQUEST HANDLERS //
//////////////////////////

void onAppConnected() {
  appConnected = true;

  // State changes that happen while disconnected are not resolved by Blynk, which is probably a bug
  updateButton();
}

BLYNK_APP_CONNECTED() {
  BLYNK_LOG("App connected");
  onAppConnected();
}

BLYNK_APP_DISCONNECTED() {
  BLYNK_LOG("App disconnected");
  appConnected = false;
}

BLYNK_READ(VPIN_VBAT) {
  // Blynk will, incorrectly, not report that the app has connected if the app is already open when the hardware boots.
  // The app is obviously open if a read has been requested, so set it here as well.
  onAppConnected();

  sampleVBat();
  checkPower();
  writeVBat();
}

BLYNK_WRITE(VPIN_POWER) {
  powerRequested = param.asInt() > 0;
  checkPower();
}

void saveConfigCallback () {
  BLYNK_LOG("Configuration will be saved");
  shouldSaveConfig = true;
}

void setup() {
  pinMode(PIN_POWER, OUTPUT);
  pinMode(PIN_WIFI_CONFIG, INPUT_PULLUP);

  powerState = true;
  powerOff();

  // Debug console
  Serial.begin(9600);
  BLYNK_LOG("Startup");

  bool loaded_config = false;
  BLYNK_LOG("Mounting FS");
  if (SPIFFS.begin()) {
    BLYNK_LOG("Mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      BLYNK_LOG("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        BLYNK_LOG("Opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          BLYNK_LOG("Parsed json");
          strcpy(blynk_token, json["blynk_token"]);
          loaded_config = true;
        } else {
          BLYNK_LOG("Failed to load json config");
        }
      }
    }
    else {
      BLYNK_LOG("No configuration found");

    }
  } else {
    BLYNK_LOG("Failed to mount FS");
  }

  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  WiFiManagerParameter custom_blynk_token("Blynk Token", "Blynk Token", blynk_token, 33);
  wifiManager.addParameter(&custom_blynk_token);

  if (digitalRead(PIN_WIFI_CONFIG) == LOW || !loaded_config) {
    BLYNK_LOG("Configuration portal manual start");
    wifiManager.setTimeout(300); // 5 minutes

    if (!wifiManager.startConfigPortal(CONFIG_AP_NAME)) {
      BLYNK_LOG("failed to connect and hit timeout");
      delay(3000);
      ESP.reset();
    }
  } else {
    wifiManager.autoConnect(CONFIG_AP_NAME);
  }
  BLYNK_LOG("Connected to wifi");

  strcpy(blynk_token, custom_blynk_token.getValue());

  if (shouldSaveConfig) {
    BLYNK_LOG("Saving configuration");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }

  WiFi.mode(WIFI_STA);
  wifi_set_sleep_type(LIGHT_SLEEP_T);

  BLYNK_LOG2("ESP Core: ", ESP.getCoreVersion());
  BLYNK_LOG2("ESP SDK: ", ESP.getSdkVersion());

  Blynk.config(blynk_token);
  while (Blynk.connect() != true) {}

  // upload once per minute, which is the maximum allowed by Blynk historical plots
  timer.setInterval(60000L, writeVBat);
}

void loop() {
  Blynk.run();
  timer.run();

  // If the app is not connected then we don't need to be particularly responsive.
  // Enter light sleep to save power.
  // Don't insert delays if Blynk isn't connected yet because it'll disrupt the login process.
  if (!appConnected && Blynk.connected()) {
    delay(5000);
    sampleVBat();
    checkPower();
  }
}

