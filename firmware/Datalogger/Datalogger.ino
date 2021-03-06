/*********************************************
 *      Variables definition
 ********************************************/
#define USE_CREDENTIALS
#define SIMULATE_SENSOR_DATA

#define _WIFI_NETWORK "myNetwokr"
#define _WIFI_PASSWORD "myPassword"

#define _FIWARE_device_ID "SENSOR_ID"
#define _FIWARE_server "iota-int.iotplatform.telefonica.com"
#define _FIWARE_port "8085"
#define _FIWARE_apikey "APIKEY"

#define _SENDING_INTERVAL_MS 15000
/********************************************/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

//ESP8266 Native libraries
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>

//External libraries
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#ifndef USE_CREDENTIALS
  #include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
  WiFiManager wifiManager;                //Wifi Manager to setup WIFI SSID
#endif


// I2C Barometric sensor libraries
#ifndef SIMULATE_SENSOR_DATA
  #include <Wire.h>
  #include <Adafruit_BMP085.h>
  #define SCL_PIN D1
  #define SDA_PIN D2
  Adafruit_BMP085 bmp;
#endif

// Function definitios
void dataSenderCallback();

/*************** Global Variables ***************/
// Timing Variables
unsigned long previousMillis=0;
unsigned long deltaMillis=_SENDING_INTERVAL_MS;

//ESP8266 objects
ESP8266WebServer server(80);            //Web server
ESP8266HTTPUpdateServer httpUpdater;    //For HTTP OTA Update
HTTPClient http;                        //Declare object of class HTTPClient

//FIWARE Variables
String FIWARE_device_ID   = _FIWARE_device_ID;
String FIWARE_server      = _FIWARE_server;
String FIWARE_port        = _FIWARE_port;
String FIWARE_apikey      = _FIWARE_apikey;

//Wifi Variables
#ifdef USE_CREDENTIALS
  const char* WiFi_Network = _WIFI_NETWORK;
  const char* WiFi_Password = _WIFI_PASSWORD;
#else
  const char* WiFi_SoftAP_Name = "FIWAREZone_IoT";
  const char* WiFi_SoftAP_WiFi_Name = "FIWAREZone_IoT_Wifi";
#endif

//OTA
const char* update_path = "/webota";
const char* update_username = "admin";
const char* update_password = "admin";

//Other Variables
int transmisionStatus = 0;
char rxBuf[64];
int rxBufIndex;
char dev_hostname[30];




/*********************************************
 *                Setup Code
 ********************************************/
void setup() {
  //serial Port setup
  Serial.begin(9600);
  Serial.println("\n FIWARE Zone IoT Device");

  //Setup Hostname
  (String("SC-") + ESP.getChipId()).toCharArray(dev_hostname, 30);
  Serial.print("Dev-name: ");
  Serial.println(dev_hostname);

//Configuring WIFI
#ifdef USE_CREDENTIALS
/***************************Hardcoded credentials*******************************/
  WiFi.begin(WiFi_Network, WiFi_Password);

  Serial.println();
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("success!");
  Serial.print("IP Address is: ");
  Serial.println(WiFi.localIP());

#else
  /***************************Wifi Manager usage********************************/
  wifiManager.setTimeout(5);


  if (!wifiManager.autoConnect(WiFi_SoftAP_Name, "")) {
    Serial.println("failed to connect and hit timeout");
    
    //Start Soft AP
    WiFi.softAP(WiFi_SoftAP_Name, "");
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
  }
  else {

    //if you get here you have connected to the WiFi
    Serial.println("Connected!");

    // Set up mDNS responder:
    if (!MDNS.begin(dev_hostname)) {
      Serial.println("Error setting up MDNS responder!");
      while (1) {
        delay(1000);
      }
    }
    Serial.println("mDNS responder started");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);

  }
  
  //Start OTA updater
  httpUpdater.setup(&server, update_path, update_username, update_password);

  // Start TCP (HTTP) server
  server.begin();
  Serial.println("TCP server started");

  //Bind Webserver URL functions
  server.on ( "/", handleRoot );
  server.on ( "/wrst", handleWrst );
  server.on ( "/wifi", handleWifi );
  server.on ( "/webota", handleWebota );
  server.on ( "/help", handleHelp );
  server.on ( "/postul2", handlePostUL2 );
  server.on ( "/postul2data", handlePostUL2data );
  server.on ( "/sensors", handleSensors );
#endif


#ifndef SIMULATE_SENSOR_DATA
  // I2C Sensor Initialization
  Wire.pins(SDA_PIN, SCL_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bmp.begin()) {
     Serial.println("No BMP180 / BMP085");// we dont wait for this
     while (1) {}
  }
#endif

}



/*********************************************
 *                Loop Code
 ********************************************/
void loop() {
  //WebServer task
  server.handleClient();


  if (millis()>previousMillis+deltaMillis){
    previousMillis=millis();
    //Execute task
    dataSenderCallback();
  }
}





int ultralightSend (String URL, String port, String Token, String ID, String Body) {
  int httpCode = 0;
  
  Serial.println("\nSENDING DATA TO IoTAgent - Ultralight");
  Serial.println("   http://" + URL + ":" + port + "/iot/d?k=" + Token + "&i=" + ID + "&getCmd=1");
  
  // Configure URL and headers
  http.begin("http://" + URL + ":" + port + "/iot/d?k=" + Token + "&i=" + ID + "&getCmd=1");
  http.addHeader("Content-Type", "text/plain");

  // Execute request and get response payload 
  httpCode = http.POST(Body);
  String payload = http.getString();

  //Print HTTP response
  Serial.print("   Request executed.Return code: ");
  Serial.print(httpCode);
  Serial.print(". Response payload: ");
  Serial.println(payload);

  //Close connection
  http.end();  											                  
  return httpCode;
}



String dataReaderSensor(){

    String t_meas;
    String p_meas;
    String a_meas;

#ifndef SIMULATE_SENSOR_DATA
    t_meas = String(bmp.readTemperature());
    p_meas = String(bmp.readPressure());
    a_meas = String(bmp.readAltitude(101325));            // insert pressure at sea level
#else
    t_meas = random(0,40);
    p_meas = random(100000,102000);
    a_meas = random(0,3000);
#endif

    Serial.println("\nReading data from sensors:");
    Serial.println(" - T=" + t_meas + " *C");
    Serial.println(" - P=" + p_meas + " Pa");
    Serial.println(" - A=" + a_meas + " m");

    String r = "t|"+ t_meas +"|p|"+ p_meas +"|a|"+ a_meas;

  return r;
}



void dataSenderCallback(){
  int returnCode = ultralightSend(FIWARE_server,FIWARE_port,FIWARE_apikey,FIWARE_device_ID,dataReaderSensor());
   if (returnCode ==200){
      Serial.println("Data sent successfully to FIWARE\n\n");
    }
    else{
      Serial.println("Failure sending data to FIWARE\n\n");
    }
}
