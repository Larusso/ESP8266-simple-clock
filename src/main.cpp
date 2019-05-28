#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include "time.h"

#define TFT_CS               D2
#define TFT_DC               D1
#define TFT_LED              D0

const char *ntpServer        = "pool.ntp.org";
const long gmtOffset_sec     = 3600;
const int daylightOffset_sec = 3600;

/**
 * TFT Display Constants
 */
const int xTime              = 14;
const int yTime              = 14;
const int tftBG              = ILI9341_BLACK;
const int tftTimeFG          = ILI9341_RED;

String prevTime              = "";
String currTime              = "";
String prevDate              = "";
String currDate              = "";

bool onWifi                  = false;
String weekDays[]            = {"", "Mon", "Thu", "Wed", "Thu", "Fri", "Sat", "Sun"};
String months[]              = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

ESP8266WiFiMulti             WiFiMulti;
WiFiServer                   wifiServer(1234);

Adafruit_ILI9341 tft         = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define ILI9341_LORANGE      0xFC08
#define ILI9341_LGREEN       0x87F0

#ifndef WIFI_SSID
#define WIFI_SSID "my ssid"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "password"
#endif

bool wifiConnect();
bool getNtpTime();
void setBGLuminosity(int intensity);
String refreshTime();
void displayTime();

void setup() {
    Serial.begin(115200);
    tft.begin();
    tft.setRotation(3);
    yield();
    pinMode(TFT_LED, OUTPUT);
    setBGLuminosity(255);


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

    tft.println("End of booting process.");

    delay(10000);

    //Prepare screen for normal operation
    setBGLuminosity(0);
    tft.fillScreen(ILI9341_BLACK);
    yield();
    displayTime();
    setBGLuminosity(255);
}

void loop() {
    refreshTime();
    delay(1000);
}

void setBGLuminosity(int level) {
    analogWrite(TFT_LED, level);
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
    tft.setTextSize(10);
    tft.setCursor(xTime, yTime);
    tft.setTextColor(tftBG);
    yield();

    tft.println(prevTime);
    yield();

    tft.setCursor(xTime, yTime);
    tft.setTextColor(tftTimeFG);
    yield();

    tft.println(currTime);
}

/**
 * Displays date string
 */
void displayDate() {
    tft.fillRect(14, 94, 192, 30, ILI9341_BLACK);
    tft.setTextSize(3);
    tft.setCursor(26, 94);
    tft.setTextColor(ILI9341_LGREEN);
    yield();
    tft.println(currDate);
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
