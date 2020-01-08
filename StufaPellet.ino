#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>      
#include <StreamString.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>        //https://github.com/tzapu/WiFiManager
#include <WiFiClientSecure.h>
#include <TelegramBotClient.h>  //https://github.com/schlingensiepen/TelegramBotClient
#include <WebSocketsClient.h> 

String MyApiKey = "137a122e-9b05-4d76-9046-e887b56926d3";
String DEVICE1 = "5df10e27952b38749d2c9d33";

WiFiEventHandler disconnectedEventHandler;
WiFiEventHandler gotIpEventHandler;

#define OFF LOW
#define ON HIGH
#define TELEGRAM_TIME 500
#define UPDATE_TEMP_TIME 1000
uint32_t CHECK_ON_TIME = 20000;
uint8_t  ON_TEMP = 50;

#define TX_SIZE 120

enum { WAIT = -1, STOP = 0, START = 1, STATE = 3};
const char * state[] = { "OFF", "HEATING", "ON"} ;
uint8_t idxState = 0;
typedef struct  {
  int request = WAIT;
  bool confirm = false;
  uint16_t value1;
  uint16_t value2;
} telegramCmd;

const uint8_t OUT = D1;    
uint32_t wifiTime, updateTempTime, checkTempTime; 
int32_t telegramId;

bool isOnline = false;
bool alexaCmd = false;

// Dallas DS18B20
#define ONE_WIRE_BUS D3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress ds18b20;
float  ActualTemp = 25.0F;
uint32_t wifiTimeout = millis();

void saveConfig();

#include "telegramBot.h"
#include "sinric.h"


void setup() {  
  pinMode(OUT, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200); 
  Serial.print("\n\nMounting FS...");
  if (SPIFFS.begin()) {
    Serial.println(" done");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.print("Reading config file..");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (!json.success()){
          Serial.println("Error on parsing json file");               
        }     
        else {                         
          token = json["token"].as<String>();          
          ON_TEMP = json["valueTemp"];
          CHECK_ON_TIME = json["checkTempTime"] ;
          MyApiKey = json["SinricAPIKey"].as<String>();
          DEVICE1 = json["DeviceID"].as<String>();        
          telegramId = json["TelegramID"];
          Serial.println(" done");
        }
        configFile.close();
      }
    } 
    else   
      Serial.println("/config.json not present");
  }
  else {
    Serial.println(" failed to mount FS");
  }

  // Dallas DS18B20
  sensors.begin();  
  Serial.printf("Locating devices...\nFound %d devices\n", sensors.getDeviceCount());
  if (!sensors.getAddress(ds18b20, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(ds18b20, 10);
  sensors.requestTemperatures();
  ActualTemp = sensors.getTempC(ds18b20);
  if (ActualTemp > ON_TEMP)
    idxState = 2;                       
  else 
    idxState = 0;                   

  Serial.printf("\n\nTelegram token: \t%s", token.c_str());
  Serial.printf("\nON Temperature setpoint: \t%u °C", ON_TEMP);
  Serial.printf("\nON timeout check: \t%u ms.", CHECK_ON_TIME);
  Serial.printf("\nsinric API key: \t%s", MyApiKey.c_str());
  Serial.printf("\nAlexa Device ID: \t%s", DEVICE1.c_str());
  Serial.printf("\nTelegram ID: \t%d", telegramId);
  startConnections() ; 

  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event){
    Serial.print("Station connected, IP: ");
    Serial.println(WiFi.localIP());          
    isOnline = true;    
  });

  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    Serial.println("Station disconnected");
    isOnline = false;    
  });
  
  delay(3000);
  if(!isOnline) {      
    captivePortal(300) ; 
  }
  
}

void loop() {     
  webSocket.loop();  

  if(isOnline){
    isOnline = false;
    if(telegramId != 0){     
      char tx_buffer[80];   
      sprintf(tx_buffer, "Stufa pellet online.\nStato bruciatore: %s\nTemperatura acqua: %02d.%02d°C\n",
                         state[idxState], (int)ActualTemp, (int)(ActualTemp*100)%100);   
      myBot.postMessage(telegramId, tx_buffer, myKbd);       
    }  
  }
  
  if(WiFi.status() == WL_CONNECTED) {      
  // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
    if((millis() - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
      heartbeatTimestamp = millis();
      //webSocket.sendTXT("H");          
      setSetTemperatureSettingOnServer(DEVICE1, 25.0, "CELSIUS" , ActualTemp, 45.0) ;
    }
    digitalWrite(LED_BUILTIN, HIGH);
  } else if(millis() - wifiTime > 1000){
    wifiTime = millis(); 
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  
  if(millis() - updateTempTime > UPDATE_TEMP_TIME){ 
    updateTempTime = millis();
    sensors.requestTemperatures();          
    ActualTemp = sensors.getTempC(ds18b20);     
    //Serial.printf("Temperature: %02d.%02d°C\n", (int)ActualTemp, (int)(ActualTemp*100)%100);
    myBot.loop();    
  }

  if((millis() - checkTempTime > CHECK_ON_TIME) && (cmdMsg.request == START)){    
    char tx_buffer[80];
    if(ActualTemp > ON_TEMP){
      idxState = 2;
      sprintf(tx_buffer, "Tutto ok!\nIl bruciatore è partito correttamente\nTemperatura acqua: %02d.%02d°C\n",
                         (int)ActualTemp, (int)(ActualTemp*100)%100);                                
      myBot.postMessage(telegramId, tx_buffer);   
      Serial.printf("\n(%d) %s", telegramId, tx_buffer);      
      cmdMsg.request = WAIT;         
    }
    else{
      sprintf(tx_buffer, "Attenzione!\nIl bruciatore non è partito.\nTemperatura acqua: %02d.%02d°C\n",
                         (int)ActualTemp, (int)(ActualTemp*100)%100);             
      idxState = 0;                   
      myBot.postMessage(telegramId, tx_buffer);   
      Serial.printf("\n(%d) %s", telegramId, tx_buffer);      
      cmdMsg.request = WAIT;      
    }
    
  }

  if(cmdMsg.confirm == true || alexaCmd   ){
    cmdMsg.confirm = false;
    alexaCmd = false;  
    if(cmdMsg.request == START && idxState == 0){
      digitalWrite(OUT, ON);   
      delay(2000);
      digitalWrite(OUT, OFF);  
      setSetTemperatureSettingOnServer(DEVICE1, 25.0, "CELSIUS" , ActualTemp, 45.0) ;
      delay(250);
      setPowerStateOnServer(DEVICE1, "HEAT");
      idxState = 1;
    } 
    else if(cmdMsg.request == STOP && idxState > 0  ){
      digitalWrite(OUT, ON);
      delay(2000);
      digitalWrite(OUT, OFF);        
      cmdMsg.request = WAIT;        
      setPowerStateOnServer(DEVICE1, "OFF");        
      idxState = 0;
    }
  }

  
}


void captivePortal(uint16_t timeOut ) {    
  //WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  Serial.println("Start captive portal");
  WiFiManager wifiManager;
  wifiManager.setBreakAfterConfig(true);
  //wifiManager.setDebugOutput(false);
  //wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(timeOut);

  WiFiManagerParameter telegramLabel("<p>Telegram token</p>");
  WiFiManagerParameter telegramToken("Telegram", "Token", token.c_str(), 50);
  WiFiManagerParameter checkTempLabel("<p>Tempo di controllo accensione (s)</p>");
  WiFiManagerParameter checkTempSec("checkTempSec", "Controllo Acqua (secondi)", "1200", 50);
  WiFiManagerParameter valueTempLabel("<p>Setpoint controllo temperatura acqua (°C)</p>");
  WiFiManagerParameter valueTemp("valueTemp", "Temperatura acqua °C", "80", 50);

  WiFiManagerParameter alexaAPILabel("<p>Chiave API sinric(s)</p>");
  WiFiManagerParameter alexaAPIValue("SinricAPIKey", "Chiave API sinric", "137a122e-9b05-4d76-9046-e887b56926d3", 50);
  WiFiManagerParameter deviceIdLabel("<p>Alexa device ID</p>");
  WiFiManagerParameter deviceIdValue("DeviceID", "Alexa device ID", "5de942bff67aa861d0450bf9", 50);

  wifiManager.addParameter(&telegramLabel);
  wifiManager.addParameter(&telegramToken);
  wifiManager.addParameter(&checkTempLabel);
  wifiManager.addParameter(&checkTempSec);
  wifiManager.addParameter(&valueTempLabel);
  wifiManager.addParameter(&valueTemp);
  wifiManager.addParameter(&alexaAPILabel);
  wifiManager.addParameter(&alexaAPIValue);
  wifiManager.addParameter(&deviceIdLabel);
  wifiManager.addParameter(&deviceIdValue);
  //wifiManager.autoConnect();
  
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");       
  }
 
  //read updated parameters
  token = telegramToken.getValue();
  ON_TEMP = String(valueTemp.getValue()).toInt() ;  
  CHECK_ON_TIME = String(checkTempSec.getValue()).toInt() * 1000;
  MyApiKey = alexaAPIValue.getValue();
  DEVICE1 = deviceIdValue.getValue();
  Serial.println("Updated paramenter: ");
  Serial.println(token);
  Serial.println(ON_TEMP);  
  Serial.println(CHECK_ON_TIME);    
  Serial.println(MyApiKey);
  Serial.println(DEVICE1); 
  WiFi.mode(WIFI_STA);

  saveConfig();
}



void saveConfig(){ 
  Serial.println("Salvataggio configurazione in corso...");  
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  
  json["token"] = token;
  json["valueTemp"] = ON_TEMP;  
  json["checkTempTime"] = CHECK_ON_TIME;
  json["SinricAPIKey"] = MyApiKey;
  json["DeviceID"] = DEVICE1;
  json["TelegramID"] = telegramId;
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) { Serial.println("failed to open config file for writing"); }
  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  Serial.println(" done.");
}


// Eventi wifi
void startConnections() {  
  initTelegramBot();  
  //String ssid = "PuccosNET2";    
  //String pass = "Tole76tnt";   
  //String token = "488075445:AAFLd-B-spUviVfhMTQFWrRApG7t4gIPSWQ";   // REPLACE myToken WITH YOUR TELEGRAM BOT TOKEN
  // Serial.println(WiFi.SSID());
  // Serial.println(WiFi.psk());
  // Serial.println(token);

  uint32_t wifiTimeout = millis();
  Serial.printf("\nTry to connect to %s \n", WiFi.SSID().c_str());      
  WiFi.begin(WiFi.SSID(), WiFi.psk());
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(millis() - wifiTimeout > 10000){
      isOnline = false;
      break;
    }      
  }  
 
  if(WiFi.status() == WL_CONNECTED){
    isOnline = true;    
    Serial.print("\nWiFi connesso, Indirizzo IP: ");
    Serial.println(WiFi.localIP());

    webSocket.begin("iot.sinric.com", 80, "/");      
    webSocket.onEvent(webSocketEvent);
    webSocket.setAuthorization("apikey", MyApiKey.c_str());        
    webSocket.setReconnectInterval(5000);     
  } else
    Serial.printf("\nNon è stato possibilie collegarsi a %s\n", WiFi.SSID().c_str() );
}
