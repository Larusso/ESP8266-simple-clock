#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include "time.h"
#include <Fonts/DSEG14Modern_Bold40pt7b.h>
#include <Fonts/Droid_Sans_Mono_Nerd_Font_Complete_Mono15pt7b.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define TFT_CS               D2
#define TFT_DC               D1
#define LUX_SDA              D4
#define LUX_SCL              D3
#define DHTPIN               D9
#define DHTTYPE              DHT11

//const int DHT_OUT = 27;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

/**
 * TFT Display Constants
 */
const int xTime = 21;
const int yTime = 104;
const int tftBG = ILI9341_BLACK;
const int tftTimeFG = ILI9341_RED;

String prevTime = "";
String currTime = "";
String prevDate = "";
String currDate = "";

float prevTemp = 0;
float currTemp = 0;
float prevHumi = 0;
float currHumi = 0;

float prevLux = 0;
float currLux = 0;

bool onWifi = false;
String weekDays[] = {"", "Mon", "Thu", "Wed", "Thu", "Fri", "Sat", "Sun"};
String months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
uint32_t delayMS;
ESP8266WiFiMulti WiFiMulti;
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
BH1750 lightMeter(0x23);
DHT_Unified dht(DHTPIN, DHTTYPE);

#define ILI9341_LORANGE      0xFC08
#define ILI9341_LGREEN       0x87F0
#define ILI9341_LCYAN        0x0418

#ifndef WIFI_SSID
#define WIFI_SSID "my ssid"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "password"
#endif

bool wifiConnect();

bool getNtpTime();

String refreshTime();

void displayTime();

void displayDate();

void displayTemp();

void displayHumi();

void displayLux();

void luxChanged();

void tempChanged();

void humiChanged();

float getCurrentLux();

float getCurrentTemp();

float getCurrentHumi();

void printTempSensorInfo(const sensor_t &sensor);

void printHumiditySensorInfo(const sensor_t &sensor);

void setup() {
    Serial.begin(115200);
    tft.begin();
    tft.setRotation(3);
    yield();
    Wire.begin(LUX_SDA, LUX_SCL);


    //Boot screen
    tft.fillScreen(ILI9341_BLACK);
    yield();
    tft.setTextSize(4);
    tft.setTextColor(ILI9341_LORANGE);
    tft.println("Larusso");
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.println("");
    tft.println("Booting...");
    tft.println("Setting up devices...");

    tft.setTextColor(ILI9341_LORANGE);
    tft.println("Connecting to WiFi AP ");
    tft.setTextColor(ILI9341_WHITE);
    tft.println(WIFI_SSID);

    /**
     * Wifi connect
     */
    delay(100);

    wifiConnect();
    if (onWifi) {
        tft.print("   Connection succeed, obtained IP ");
        tft.println(WiFi.localIP());
    } else {
        tft.println("   Connection failed. Unexpected operation results.");
    }

    /**
     * NTP Time
     */
    tft.println("Obtaining NTP time from remote server...");
    getNtpTime();
    delay(100); //We need a delay to allow info propagation

    //Set up Lightmeter
    tft.println("Setup Light meter.");
    lightMeter.begin(BH1750::Mode::CONTINUOUS_HIGH_RES_MODE_2);

    tft.println("Setup Temperature sensor.");
    dht.begin();
    sensor_t sensor;

    dht.temperature().getSensor(&sensor);
    printTempSensorInfo(sensor);

    delay(1000);
    tft.println("Setup Humidity sensor.");
    dht.humidity().getSensor(&sensor);
    printHumiditySensorInfo(sensor);

    delayMS = sensor.min_delay / 1000;

    tft.println("End of booting process.");

    delay(10000);

    //Prepare screen for normal operation
    tft.fillScreen(ILI9341_BLACK);
    yield();
    displayTime();
    displayDate();
    displayTemp();
    displayHumi();
    displayLux();
}

void printHumiditySensorInfo(const sensor_t &sensor) {
    tft.println("Humidity Sensor");
    tft.print("Sensor Type: ");
    tft.println(sensor.name);
    tft.print("Driver Ver: ");
    tft.println(sensor.version);
    tft.print("Unique ID: ");
    tft.println(sensor.sensor_id);
    tft.print("Max Value: ");
    tft.print(sensor.max_value);
    tft.println("%");
    tft.print("Min Value: ");
    tft.print(sensor.min_value);
    tft.println("%");
    tft.print("Resolution: ");
    tft.print(sensor.resolution);
    tft.println("%");
}

void printTempSensorInfo(const sensor_t &sensor) {
    tft.println("Temperature Sensor");
    tft.print("Sensor Type: ");
    tft.println(sensor.name);
    tft.print("Driver Ver:  ");
    tft.println(sensor.version);
    tft.print("Unique ID:   ");
    tft.println(sensor.sensor_id);
    tft.print("Max Value:   ");
    tft.print(sensor.max_value);
    tft.println("C");
    tft.print("Min Value:   ");
    tft.print(sensor.min_value);
    tft.println("C");
    tft.print("Resolution:  ");
    tft.print(sensor.resolution);
    tft.println("C");
}

void loop() {
    refreshTime();
    getCurrentLux();
    getCurrentHumi();
    getCurrentTemp();
    delay(delayMS);
}

/**
 * Connects to WIFI
 */
bool wifiConnect() {
    // // check for the presence of the shield:
    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(WIFI_SSID, WIFI_PASS);

    while (WiFiMulti.run() != WL_CONNECTED) {
        tft.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        onWifi = true;
        Serial.println(" CONNECTED");
    }
    return onWifi;
}

/**
 * Obtains time from NTP server
 */
bool getNtpTime() {
    bool result = false;
    if (onWifi) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        result = true;
    } else {
        Serial.println("getNtpTime: Not connected to wifi!");
    }
    return result;
}

/**
 * Displays time string erasing the previous one
 */
void displayTime() {
    tft.setTextSize(1);

    tft.setCursor(xTime, yTime);
    tft.setFont(&DSEG14Modern_Bold40pt7b);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds("00:00", xTime, yTime, &x1, &y1, &w, &h);
    yield();
    tft.fillRect(x1, y1, w, h, tftBG);
    tft.setCursor(xTime, yTime);
    tft.setTextColor(tftTimeFG);
    tft.println(currTime);
    yield();
    tft.setFont();
}

/**
 * Displays date string
 */
void displayDate() {
    tft.setTextSize(1);
    tft.setCursor(16, yTime + 40);
    tft.setFont(&Droid_Sans_Mono_Nerd_Font_Complete_Mono15pt7b);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds("Mon, 31 Jun 2020", 16, yTime + 40, &x1, &y1, &w, &h);
    yield();
    tft.fillRect(x1, y1, w, h, tftBG);
    tft.setTextColor(ILI9341_LGREEN);
    tft.println(currDate);
    yield();
    tft.setFont();

}

void displayLux() {
    int bgColor = 0;
    if (currLux == prevLux) {
        bgColor = ILI9341_LGREEN;
    } else if (currLux < prevLux) {
        bgColor = ILI9341_LCYAN;
    } else {
        bgColor = ILI9341_LORANGE;
    }
    tft.setTextColor(ILI9341_BLACK);
    yield();
    tft.fillRect(212, 170, 92, 60, bgColor);
    tft.drawRect(212, 170, 92, 60, ILI9341_WHITE);
    yield();
    tft.setTextSize(2);
    tft.setCursor(242, 174);
    tft.print("LUX");
    tft.setTextSize(3);
    tft.setCursor(214, 200);
    char cLux[5] = " ";
    sprintf(cLux, "%05f", currLux);
    tft.print(cLux);
}

/**
 * Displays temperature
 */
void displayTemp() {
    int bgColor = 0;
    if (currTemp == prevTemp) {
        bgColor = ILI9341_LGREEN;
    } else if (currTemp < prevTemp) {
        bgColor = ILI9341_LCYAN;
    } else {
        bgColor = ILI9341_LORANGE;
    }

    tft.setTextColor(ILI9341_BLACK);
    yield();
    tft.fillRect(14, 170, 92, 60, bgColor);
    tft.drawRect(14, 170, 92, 60, ILI9341_WHITE);
    yield();
    tft.setTextSize(2);
    tft.setCursor(34, 174);
    tft.print("TEMP");
    tft.setTextSize(3);
    tft.setCursor(16, 200);
    tft.print(currTemp);
}

/**
 * Displays relative humidity
 */
void displayHumi() {
    int bgColor = 0;
    if (currHumi == prevHumi) {
        bgColor = ILI9341_LGREEN;
    } else if (currHumi < prevHumi) {
        bgColor = ILI9341_LCYAN;
    } else {
        bgColor = ILI9341_LORANGE;
    }
    tft.setTextColor(ILI9341_BLACK);
    yield();
    tft.fillRect(113, 170, 92, 60, bgColor);
    tft.drawRect(113, 170, 92, 60, ILI9341_WHITE);
    yield();
    tft.setTextSize(2);
    tft.setCursor(135, 174);
    tft.print("H.R.");
    tft.setTextSize(3);
    tft.setCursor(115, 200);
    tft.print(currHumi);
}

/**
 * Returns ambient light luxes
 */
float getCurrentLux() {
    prevLux = currLux;
    currLux = lightMeter.readLightLevel();

    if (prevLux != currLux) {
        luxChanged();
    }
    return currLux;
}

/**
 * Returns temperature
 */
float getCurrentTemp() {
    prevTemp = currTemp;

    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
        Serial.println("Error reading temperature!");
    } else {
        Serial.print("Temperature: ");
        Serial.print(event.temperature);
        Serial.println("°C");
        currTemp = event.temperature;
    }

    if (prevTemp != currTemp) {
        tempChanged();
    }

    return currTemp;
}

/**
 * Returns humidity
 */
float getCurrentHumi() {
    prevHumi = currHumi;
    sensors_event_t event;
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
        Serial.println("Error reading humidity!");
    } else {
        Serial.print("Humidity: ");
        Serial.print(event.relative_humidity);
        Serial.println("%");
    }
    currHumi = event.relative_humidity;
    if (prevHumi != currHumi) {
        humiChanged();
    }
    return currHumi;
}

/**
 * Event for change of ambient light
 */
void luxChanged() {
    Serial.print("luxChanged event fired! ");
    Serial.println(currLux);
    displayLux();
}

/**
 * Event for change of temperature
 */
void tempChanged() {
    Serial.print("tempChanged event fired! ");
    displayTemp();
}

/**
 * Event for change of humidity
 */
void humiChanged() {
    Serial.println("humiChanged event fired!");
    displayHumi();

}

/**
 * Returns a string formatted HH:MM based on hours and minutes
 */
String hourMinuteToTime(int hour, int minute) {
    String sTime;
    char cTime[5] = " ";
    sprintf(cTime, "%02d:%02d", hour, minute);
    sTime = (char *) cTime;
    return sTime;
}

/**
 *  EVENTS
 */
/**
 * Event for change of time HH:MM
 */
void timeChanged() {
    Serial.println("timeChanged event fired!");
    displayTime();
}

/**
 * Event for change of date weekDay, day de Month de Year
 */
void dateChanged() {
    Serial.println("dateChanged event fired!");
    displayDate();
}

/*
 * Returns current time in HH:MM format
 */
String refreshTime() {
    Serial.println("refresh time");

    //Time
    time_t now;
    struct tm *timeinfo;
    time(&now);
    timeinfo = localtime(&now);
    prevTime = currTime;
    currTime = hourMinuteToTime(timeinfo->tm_hour, timeinfo->tm_min);
    Serial.println(currTime);
    if (prevTime != currTime) {
        timeChanged();
        //If time has changed, lets check if date has changed too
        //Date
        int wDay;
        wDay = timeinfo->tm_wday;
        if (wDay == 0) {
            wDay = 7;
        }
        String calDate = "";
        calDate = calDate + weekDays[wDay];
        calDate = calDate + ", ";
        calDate = calDate + timeinfo->tm_mday;
        calDate = calDate + " ";
        calDate = calDate + months[(timeinfo->tm_mon + 1)];
        calDate = calDate + " ";
        calDate = calDate + (timeinfo->tm_year + 1900);
        if (calDate.length() == 21) {
            calDate = " " + calDate;
        }
        prevDate = currDate;
        currDate = calDate;
        if (prevDate != currDate) {
            dateChanged();
        }
    }

    Serial.println("refresh time done");

    return currTime;
}
