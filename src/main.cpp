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
#include <PubSubClient.h>

uint32_t timeNow = 0;
uint32_t timeLast = 0;
uint32_t lastMeasureTime = 0;

const uint8_t GROVE_POWER = 15;
const uint8_t CONFIG_BUTTON = 0;
/* This value will need to be updated as things go along. May look into making
 * this a configurable value instead of a compile constant. */
const uint16_t ADC_THRESHOLD = 10000;

uint16_t distance = 0;
uint8_t lastIndex = 0;

const uint8_t greyCode[] { 0x0, 0x1, 0x3, 0x2, 0x6, 0x7, 0x5, 0x4, 0xC, 0xD, 0xF, 0xE, 0xA, 0xB, 0x9, 0x8 };

/* Grey code for position measurement:
 * posiiton       grey  binary
 *        0:         0:   0000
 *        1:         1:   0001
 *        2:         3:   0011
 *        3:         2:   0010
 *        4:         6:   0110
 *        5:         7:   0111
 *        6:         5:   0101
 *        7:         4:   0100
 *        8:        12:   1100
 *        9:        13:   1101
 *       10:        15:   1111
 *       11:        14:   1110
 *       12:        10:   1010
 *       13:        11:   1011
 *       14:         9:   1001
 *       15:         8:   1000
 */

bool pressed = false;

bool updateLog = false;

Adafruit_ADS1115 ads(ADS1015_ADDRESS_GND);


/*******************************************************************************
 * Time Tracking vars start
 ******************************************************************************/
uint8_t day = 1;
uint8_t month = 1;
uint8_t year = 18;

uint8_t seconds = 0;
uint8_t minutes = 33;
uint8_t hours   = 12;

/*******************************************************************************
 * Time Tracking vars end
 ******************************************************************************/
ESP8266WebServer server(80);

const char mqttServer[] = "10.0.1.15";
const char mqttToken[] = "mdr6IOkiyMCdLqKnkiWC";
const uint16_t mqttPort = 1883;
WiFiClient client;
PubSubClient psclient(client);

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
void mqttReconnect();

/* Binary/Gray conversion code taken from
 * https://en.wikipedia.org/wiki/Gray_code
 */
uint8_t BinaryToGray(uint8_t num);
uint8_t GrayToBinary(uint8_t num);

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

  if (MDNS.begin("FurBall"))
  {
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

  // Initialize mqtt
  psclient.setServer(mqttServer,mqttPort);

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

void loop(void)
{
  processTime();
  server.handleClient();

  if ( HIGH == digitalRead(CONFIG_BUTTON))
  {
    if (!pressed)
    {
      pressed = true;

      int16_t adc1, adc2, adc3, adc4;

      adc1 = ads.readADC_SingleEnded(0);
      adc2 = ads.readADC_SingleEnded(1);
      adc3 = ads.readADC_SingleEnded(2);
      adc4 = ads.readADC_SingleEnded(3);

      Serial.println("Analog A1:" + String(adc1) + " A2:" + String(adc2) + " A3:" + String(adc3) + " A4:" + String(adc4));

      distance++;
    }
  }
  else
  {
    pressed = false;
  }

  // Check to see if it's been more than 10 milliseconds since last reading.
  if ( (millis() - lastMeasureTime) > 10)
  {
    lastMeasureTime = millis();

    uint8_t curSegment = 0x00;
    uint8_t curIndex = 0;

    int16_t adc1, adc2, adc3, adc4;

    adc1 = ads.readADC_SingleEnded(0);
    adc2 = ads.readADC_SingleEnded(1);
    adc3 = ads.readADC_SingleEnded(2);
    adc4 = ads.readADC_SingleEnded(3);

    if ( adc1 >= ADC_THRESHOLD )
    {
      curSegment |= 0x01;
    }

    if ( adc2 >= ADC_THRESHOLD )
    {
      curSegment |= 0x02;
    }

    if ( adc3 >= ADC_THRESHOLD )
    {
      curSegment |= 0x04;
    }

    if ( adc4 >= ADC_THRESHOLD )
    {
      curSegment |= 0x08;
    }

    for ( uint8_t i = 0; i < 16; i++ )
    {
      if (greyCode[i] == curSegment)
      {
        curIndex = i;
        break;
      }
    }

    if ( curIndex != lastIndex )
    {
      /* I don't care what direction it is going in, so I won't bother with that
       * calculation. I also make the assumption that it will not increment twice
       * as then the cats would be going better than 53 miles an hour. */
      lastIndex = curIndex;
      distance++;
    }
  }


  if ( updateLog )
  {
    // clear flag so we don't keep logging.
    updateLog = false;

    logDistance();

  }

  if ( !psclient.connected() )
  {
    mqttReconnect();
  }

  psclient.loop();
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

      // get todays date and get ready to parse it.
      String dateToday = line.substring(7, 15);

      // get the time and get ready to parse it.
      String inTime = line.substring(16, 24);

      hours = inTime.substring(0,2).toInt();
      minutes = inTime.substring(3,5).toInt();
      seconds = inTime.substring(6,8).toInt();

      // TODO test this to make sure it's being parsed correctly
      year = dateToday.substring(0,2).toInt();
      month = dateToday.substring(3,5).toInt();
      day = dateToday.substring(6,8).toInt();

      Serial.println("Time: " + inTime);
      Serial.println("Date: " + dateToday);
    }

    client.stop();
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

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"distance\":";
  /* Segments are about .78 inches long, so before sending the data I correct
   * to the actual distance traveled instead of the segment count. */
  payload += int(float(distance)*0.78);
  payload += "}";

  distance = 0;

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  psclient.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
}

/*
 * This function converts an unsigned binary
 * number to reflected binary Gray code.
 *
 * The operator >> is shift right. The operator ^ is exclusive or.
 */
uint8_t BinaryToGray(uint8_t numBin)
{
  return (uint8_t)(numBin ^ (numBin >> 1));
}

/*
* This function converts a reflected binary
* Gray code number to a binary number.
* Each Gray code bit is exclusive-ored with all
* more significant bits.
*/
uint8_t GrayToBinary(uint8_t numGray)
{
  uint8_t mask = numGray;
  while (mask != 0)
  {
    mask = mask >> 1;
    numGray = numGray ^ mask;
  }
  return numGray;
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!psclient.connected()) {
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( psclient.connect("adae2390-3d32-11e8-8b9f-cd5b613d54ae", mqttToken, NULL) ) {
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( psclient.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
