/* FurBall - Cat wheel logging
 * Copyright (C) 2018  Erik Ekedahl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <FS.h>  // flash file system.
#include <Adafruit_ADS1015.h>    // https://github.com/MrScrith/Adafruit_ADS1X15

unsigned long timeNow = 0;
unsigned long timeLast = 0;

const uint8_t GROVE_POWER = 15;
const uint8_t CONFIG_BUTTON = 0;
/* I don't want to bother logging if the cats haven't moved at least X feet
 * in the last 5 min. */
const unsigned long DISTANCE_MIN = 12;

const char* DISTANCE_LOG_FILE = "distance.log";

unsigned long distance = 0;
bool pressed = false;

bool updateLog = false;

Adafruit_ADS1115 ads(ADS1015_ADDRESS_SCL);


/*******************************************************************************
 * Time Tracking vars start
 ******************************************************************************/

String dateToday = "18-01-01"; // note: in year-month-day format.
int startingHour = 12;
// set your starting hour here, not below at int hour. This ensures accurate daily correction of time

int seconds = 0;
int minutes = 33;
int hours = startingHour;

/*******************************************************************************
 * Time Tracking vars end
 ******************************************************************************/
ESP8266WebServer server(80);

/*******************************************************************************
 * Function Prototype Definitions
 ******************************************************************************/
void processTime();
void getNistTime();
void handleRoot();
void handleNotFound();
void logDistance();
String getContentType(String filename);
bool handleFileRead(String path);

/*******************************************************************************
 * Function Implementations
 ******************************************************************************/
void setup(void)
{
  // Turn on the power to Grove modules:
  pinMode(GROVE_POWER, OUTPUT);
  digitalWrite(GROVE_POWER, 1);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  wifiManager.autoConnect("FurBall");

  Serial.println("");

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("FurBall")) {
    Serial.println("MDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", handleRoot);
  // TODO:: setup serving files from the file system
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  // get the initial time set.
  getNistTime();

  // Setup input/config button
  pinMode(CONFIG_BUTTON, INPUT);

  // Initialize access to the file system.
  Serial.println("Initializing file system.");
  SPIFFS.begin();
  Serial.println("File System Initialized.");

  Serial.println("Initializing ADC.");
  Wire.begin();
  ads.setGain(GAIN_TWOTHIRDS); // 1x gain   +/- 4.096V  1 bit = 2mV   0.125mV
  ads.begin();
}

void loop(void){
  processTime();
  server.handleClient();

  if ( HIGH == digitalRead(CONFIG_BUTTON))
  {
    if (!pressed)
    {
      distance++;
      pressed = true;

      int16_t adc1, adc2, adc3, adc4;

      adc1 = ads.readADC_SingleEnded(0);
      adc2 = ads.readADC_SingleEnded(1);
      adc3 = ads.readADC_SingleEnded(2);
      adc4 = ads.readADC_SingleEnded(3);

      Serial.println("Analog A1:" + String(adc1) + " A2:" + String(adc2) + " A3:" + String(adc3) + " A4:" + String(adc4));
    }
  }
  else
  {
    pressed = false;
  }


  if ( updateLog )
  {
    // clear flag so we don't keep logging.
    updateLog = false;

    // Only log if we have reached the minimum distance.
    if ( distance > DISTANCE_MIN)
    {
      logDistance();
    }
    /* NOTE: Distance will accumulate if not logged... so if the cat wheel turns
     * for 1 foot every few minutes, eventually when distance reaches the
     * minimum value it will be logged.*/
  }
}


void processTime()
{
  int timeupdated = 0;
  /* The number of seconds that have passed since the last time 60 seconds was
   * reached. */
  timeNow = millis()/1000; // the number of milliseconds that have passed since boot
  seconds = timeNow - timeLast;

  /* If one minute has passed, start counting milliseconds from zero again and
   * add one minute to the clock. */
  if (seconds == 60)
  {
    timeLast = timeNow;
    minutes = minutes + 1;
    timeupdated = 1;
  }

  /* if one hour has passed, start counting minutes from zero and add one hour
   * to the clock */
  if (minutes == 60)
  {
    minutes = 0;
    hours = hours + 1;
  }

  /* if 24 hours have passed, add one day */
  if (hours == 24)
  {
    hours = 0;
    getNistTime();
  }

  if ( timeupdated == 1 )
  {
    Serial.println("The time is: " + String(hours) + ":" + String(minutes) + ":00");
    timeupdated = 0;

    if ( (minutes % 5) == 0 )
    {
      // every 5 minutes I want to update the log.
      updateLog = true;
    }
  }
}

void getNistTime()
{
  const int timePort = 13;
  const char* host = "time.nist.gov";
  WiFiClient client;

  if (!client.connect(host,timePort))
  {
    Serial.print("Connection to time server '");
    Serial.print(host);
    Serial.print("' failed.");
  }
  else
  {
    client.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 Arduino;)\r\n\r\n");

    // wait for response.
    delay(100);

    while(client.available())
    {
      String line = client.readStringUntil('\r');

      // get todays date and save it globaly
      dateToday = line.substring(7, 15);

      // get the time and get ready to parse it.
      String inTime = line.substring(16, 24);

      hours = inTime.substring(0,2).toInt();
      minutes = inTime.substring(3,5).toInt();
      seconds = inTime.substring(6,8).toInt();

      Serial.println("Time: " + inTime);
      Serial.println("Date: " + dateToday);
    }
  }
}

void handleRoot()
{
  WiFiClient client = server.client();

  String s;
  s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n";
  s += "<html><body><h1>Hello from Wio Link!</h1>";
  if(!client)
  {
    s += "<p>No client found, are you sure you exist?</p>";
  }
  else
  {
    IPAddress ip = client.remoteIP();

    s += "<p> Thank you for connecting: " + String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]) + "</p>";
    s += "<p>Currently, cats have traveled " + String(distance) + " feet since last restart.</p>";
  }
  s += "</body></html>";
  
  server.send(200, "text/html", s);
}

void handleNotFound()
{
  if (!handleFileRead(server.uri()))
  {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i=0; i<server.args(); i++){
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
  }
}

String getContentType(String filename)
{ // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  Serial.println("\tFile Not Found");
  return false;                                         // If the file doesn't exist, return false
}

void logDistance()
{
  // before we do anything, make sure the log file exists.
  if (!SPIFFS.exists(DISTANCE_LOG_FILE))
  {
    // Log file does not exist... create it.
    SPIFFS.format();

    File logFile = SPIFFS.open(DISTANCE_LOG_FILE,"a");
  }
}
