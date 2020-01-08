
WebSocketsClient webSocket;
WiFiClient client;
#define HEARTBEAT_INTERVAL 30000 // 5 Minutes 
uint32_t heartbeatTimestamp = 0;

void turnOn(String deviceId) {
  char tx_buffer[80];
  if (DEVICE1.indexOf(deviceId) > -1){
    alexaCmd = true;  
    cmdMsg.request = START;          
    checkTempTime = millis(); 
    sprintf(tx_buffer, "\nRicevuto comando di accensione da Alexa\n");                                
    myBot.postMessage(telegramId, tx_buffer, myKbd);   
    Serial.println(tx_buffer);
  }  
}

void turnOff(String deviceId) {
  char tx_buffer[80];
  if (DEVICE1.indexOf(deviceId) > -1){
    alexaCmd = true;                                  
    cmdMsg.request = STOP;   
    sprintf(tx_buffer, "\nRicevuto comando di spegnimento da Alexa\n");                                
    myBot.postMessage(telegramId, tx_buffer, myKbd);   
    Serial.println(tx_buffer);
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:      
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {      
      Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("Waiting for commands from sinric.com ...\n");        
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);
      
#if ARDUINOJSON_VERSION_MAJOR == 5
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char*) payload);      
#endif        
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        
        if(action == "setPowerState") { // Switch or Light
            String value = json ["value"];
            if(value == "ON") {
                turnOn(deviceId);
            } else {
                turnOff(deviceId);
            }
        }
        else if (action == "test") {
            Serial.println("[WSc] received test command from sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
    default :
      break;
  }
}


//eg: setSetTemperatureSettingOnServer("deviceid", 25.0, "CELSIUS" or "FAHRENHEIT", 23.0, 45.3)
// setPoint: Indicates the target temperature to set on the termostat.
void setSetTemperatureSettingOnServer(String deviceId, float setPoint, String scale, float ambientTemperature, float ambientHumidity) {
#if ARDUINOJSON_VERSION_MAJOR == 5
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
  DynamicJsonDocument root(1024);
#endif        
  root["action"] = "SetTemperatureSetting";
  root["deviceId"] = deviceId;

#if ARDUINOJSON_VERSION_MAJOR == 5
  JsonObject& valueObj = root.createNestedObject("value");
  JsonObject& temperatureSetting = valueObj.createNestedObject("temperatureSetting");
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
  JsonObject valueObj = root.createNestedObject("value");
  JsonObject temperatureSetting = valueObj.createNestedObject("temperatureSetting");
#endif
  temperatureSetting["setPoint"] = setPoint;
  temperatureSetting["scale"] = scale;
  temperatureSetting["ambientTemperature"] = ambientTemperature;
  temperatureSetting["ambientHumidity"] = ambientHumidity;
   
  StreamString databuf;
#if ARDUINOJSON_VERSION_MAJOR == 5
  root.printTo(databuf);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
  serializeJson(root, databuf);
#endif  
  
  webSocket.sendTXT(databuf);
}



// Call ONLY If status changed. DO NOT CALL THIS IN loop() and overload the server. 

void setThermostatModeOnServer(String deviceId, String thermostatMode) {
#if ARDUINOJSON_VERSION_MAJOR == 5
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
  DynamicJsonDocument root(1024);
#endif        
  root["deviceId"] = deviceId;
  root["action"] = "SetThermostatMode";
  root["value"] = thermostatMode;
  StreamString databuf;
#if ARDUINOJSON_VERSION_MAJOR == 5
  root.printTo(databuf);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
  serializeJson(root, databuf);
#endif  
  
  webSocket.sendTXT(databuf);
}



void setPowerStateOnServer(String deviceId, String value) {
#if ARDUINOJSON_VERSION_MAJOR == 5
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
  DynamicJsonDocument root(1024);
#endif        
  root["deviceId"] = deviceId;
  root["action"] = "setPowerState";
  root["value"] = value;
  StreamString databuf;
#if ARDUINOJSON_VERSION_MAJOR == 5
  root.printTo(databuf);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
  serializeJson(root, databuf);
#endif  
  
  webSocket.sendTXT(databuf);
}
