#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h>        //https://github.com/tzapu/WiFiManager
#include <CTBot.h>              //https://github.com/shurillu/CTBot

CTBot myBot;
CTBotInlineKeyboard myKbd, goKbd;  // custom inline keyboard object helper

#define OFF LOW
#define ON HIGH
#define TELEGRAM_TIME 500
#define UPDATE_TEMP_TIME 1000
#define CHECK_ON_TIME 20000

//String token = "488075445:AAG5S_I2MHDOtMK8v4U8QhcogrKef1Yltd8";  
String token;

const uint8_t OUT = D1;    
uint32_t checkTelegramTime, updateTempTime, checkTempTime; 
uint8_t statoBruciatore = ON;

// Dallas DS18B20
#define ONE_WIRE_BUS D3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress ds18b20;
float  ActualTemp = 25.0F;
#define ON_TEMP 45.0F

typedef enum { WAIT = -1, STOP = 0, START = 1, STATE = 3, TIME_ON = 4, TIME_OFF = 5 };
typedef struct  {
  int request = WAIT;
  bool confirm = false;
  uint16_t value1;
  uint16_t value2;
} telegramCmd;

telegramCmd cmdMsg;
TBMessage msg;

#define START_CALLBACK  "Comando bruciatore ON" 
#define STOP_CALLBACK "Comando bruciatore OFF"
#define STATE_CALLBACK "Stato bruciatore"
#define CONFIRM_CALLBACK "Esegui"
#define CANCEL_CALLBACK "Annulla"

WiFiEventHandler connectedHandler;
WiFiEventHandler disconnectedHandler;

void setup() {	
  pinMode(OUT, OUTPUT);
	Serial.begin(115200);	
  connectedHandler = WiFi.onStationModeConnected(&onConnection);
  disconnectedHandler = WiFi.onStationModeDisconnected(&onDisconnection);
    
  WiFiManager wifiManager;  
  WiFiManagerParameter telegramToken("Telegram", "Token", "782955778:AAH4i5TyacBag5FMVzAUGUnVX3JhpjFrR-M", 50);
  //wifiManager.resetSettings();
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(180);  
  wifiManager.addParameter(&telegramToken);
  wifiManager.autoConnect();
  token = telegramToken.getValue();
  Serial.println(token);
  delay(1000);  
  if (WiFi.isConnected()) {    
    WiFiEventStationModeConnected nullEvent;
    onConnection(nullEvent);
  }  
  // Dallas DS18B20
  sensors.begin();  
  Serial.printf("Locating devices...\nFound %d devices\n", sensors.getDeviceCount());
  if (!sensors.getAddress(ds18b20, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(ds18b20, 10);
  sensors.requestTemperatures();
  ActualTemp = sensors.getTempC(ds18b20);
  if (ActualTemp > ON_TEMP)
    statoBruciatore = ON;      
  else 
    statoBruciatore = OFF;     
}

void loop() {	
  
  if(millis() - checkTelegramTime > TELEGRAM_TIME){
    checkTelegramTime = millis();
    checkTelegramKbd();
  }
  
  if(millis() - updateTempTime > UPDATE_TEMP_TIME){ 
    updateTempTime = millis();
    sensors.requestTemperatures();          
    ActualTemp = sensors.getTempC(ds18b20);     
    Serial.printf("Temperature: %02d.%02d°C\n", (int)ActualTemp, (int)(ActualTemp*100)%100);
  }

  if((millis() - checkTempTime > CHECK_ON_TIME) && (cmdMsg.request == START)){    
    char tx_buffer[80];
    if(ActualTemp > ON_TEMP){
      sprintf(tx_buffer, "Tutto ok!\nIl bruciatore è partito correttamente\nTemperatura acqua: %02d.%02d°C\n",
                         (int)ActualTemp, (int)(ActualTemp*100)%100);                                
      myBot.sendMessage(msg.sender.id, tx_buffer);   
      Serial.println(tx_buffer);
      statoBruciatore = ON;
      cmdMsg.request = WAIT;         
    }
    else{
      sprintf(tx_buffer, "Attenzione!\nIl bruciatore non è partito.\nTemperatura acqua: %02d.%02d°C\n",
                         (int)ActualTemp, (int)(ActualTemp*100)%100);                                
      myBot.sendMessage(msg.sender.id, tx_buffer);   
      Serial.println(tx_buffer);
      statoBruciatore = OFF;
      cmdMsg.request = WAIT;      
    }
    
  }

  if(cmdMsg.confirm == true){
    cmdMsg.confirm = false;
    if(cmdMsg.request == START && statoBruciatore == OFF){
      digitalWrite(OUT, ON);   
      delay(2000);
      digitalWrite(OUT, OFF);      
    } 
    else if(cmdMsg.request == STOP && statoBruciatore == ON){
      digitalWrite(OUT, ON);
      delay(2000);
      digitalWrite(OUT, OFF);        
      cmdMsg.request = WAIT;                
    }
  }
}

void saveConfigCallback() {
  Serial.println("Should save config");  
}


void onConnection(const WiFiEventStationModeConnected& evt) {
  Serial.println("\nWiFi connected");   
  Serial.println("Starting TelegramBot...");  
  //myBot.wifiConnect(ssid, pass);
  myBot.setTelegramToken(token);
  Serial.print("\nTelegram test connection ");
  Serial.println(myBot.testConnection() ? "OK" : "NOT OK");   
  myKbd.flushData();
  myKbd.addButton("START", START_CALLBACK, CTBotKeyboardButtonQuery); 
  myKbd.addButton("STOP", STOP_CALLBACK, CTBotKeyboardButtonQuery);    
  myKbd.addRow();  
  myKbd.addButton("Stato attuale", STATE_CALLBACK, CTBotKeyboardButtonQuery);  
  goKbd.flushData();  
  goKbd.addButton("ESEGUI", CONFIRM_CALLBACK, CTBotKeyboardButtonQuery); 
  goKbd.addButton("ANNULLA", CANCEL_CALLBACK, CTBotKeyboardButtonQuery);
}

void onDisconnection(const WiFiEventStationModeDisconnected& evt) {
  Serial.print("WiFi disconnected. Reason: ");  
  Serial.println(evt.reason); 
}


void checkTelegramKbd(){
  //TBMessage msg;
  #define TX_SIZE 120
  if (myBot.getNewMessage(msg)) {    
    char tx_buffer[TX_SIZE];
    if (msg.messageType == CTBotMessageText) {       
      sprintf(tx_buffer, "Ciao %s.!\nStato bruciatore : %s\nTemperatura acqua : %02d.%02d°C\n",
              msg.sender.username.c_str(), statoBruciatore ? "ON" : "OFF", (int)ActualTemp, (int)(ActualTemp * 100) % 100);
      myBot.sendMessage(msg.sender.id, tx_buffer, myKbd);
    } 
    else if (msg.messageType == CTBotMessageQuery) {      
      if (msg.callbackQueryData.equals(START_CALLBACK)) {
        cmdMsg.confirm = false;  
        cmdMsg.request = START;          
        checkTempTime = millis();                
        myBot.endQuery(msg.callbackQueryID, "Comando di accensione bruciatore ricevuto..");                   
        myBot.sendMessage(msg.sender.id, "Confermi l'esecuzione del comando?", goKbd);  
      } 
      else if (msg.callbackQueryData.equals(STOP_CALLBACK)) {        
        cmdMsg.confirm = false;                                   
        cmdMsg.request = STOP;    
        myBot.endQuery(msg.callbackQueryID, "Comando di spegnimento bruciatore ricevuto..");               
        myBot.sendMessage(msg.sender.id, "Confermi l'esecuzione del comando?", goKbd);              
      }
      else if (msg.callbackQueryData.equals(STATE_CALLBACK)) {
        cmdMsg.confirm = false;        
        sprintf(tx_buffer, "Stato bruciatore: %s\nTemperatura acqua: %02d.%02d°C\n",
                            statoBruciatore ? "ON" : "OFF", (int)ActualTemp, (int)(ActualTemp*100)%100);
        myBot.endQuery(msg.callbackQueryID, "");                                  
        myBot.sendMessage(msg.sender.id, tx_buffer);   
      }
      else if (msg.callbackQueryData.equals(CONFIRM_CALLBACK)) {
        cmdMsg.confirm = true;                                      
        checkTempTime = millis(); 
        myBot.endQuery(msg.callbackQueryID, "");
        if(cmdMsg.request != WAIT)
          myBot.sendMessage(msg.sender.id, "Comando eseguito."); 
        else
          myBot.sendMessage(msg.sender.id, "Non c'è nessun comando da eseguire."); 
      }
      else if (msg.callbackQueryData.equals(CANCEL_CALLBACK)) {
        cmdMsg.confirm = false;                           
        myBot.endQuery(msg.callbackQueryID, "");        
        if(cmdMsg.request != WAIT)
          myBot.sendMessage(msg.sender.id, "Comando annullato."); 
        else
          myBot.sendMessage(msg.sender.id, "Non c'è nessun comando da annullare."); 
      }
    }
  }
  
}






/*
void checkTelegram(){  
  if (myBot.getNewMessage(msg)) {    
    if (msg.text.equalsIgnoreCase("/accendi")) { 
      cmdMsg.request = ON;   
      cmdMsg.confirm = false;         
      myBot.sendMessage(msg.sender.id, "Comando di accensione caldaia ricevuto.."); 
      delay(100);
      myBot.sendMessage(msg.sender.id, "Confermi l'esecuzione?\n\n/esegui             /annulla"); 
    }
    else if (msg.text.equalsIgnoreCase("/spegni")) {     
      cmdMsg.request = OFF;    
      cmdMsg.confirm = false;                           
      myBot.sendMessage(msg.sender.id, "Comando di spegnimento caldaia ricevuto..");
      delay(100);
      myBot.sendMessage(msg.sender.id, "Confermi l'esecuzione?\n\n/esegui             /annulla"); 
    }
    else if (msg.text.equalsIgnoreCase("/esegui")) {     
      cmdMsg.confirm = true;                           
      myBot.sendMessage(msg.sender.id, "Comando eseguito.");    
      checkTempTime = millis();  
    }
    else if (msg.text.equalsIgnoreCase("/annulla")) {     
      cmdMsg.confirm = false;                           
      myBot.sendMessage(msg.sender.id, "Comando annullato.");
    }   
    else if (msg.text.equalsIgnoreCase("/status")) {   
      char tx_buffer[50];
      sprintf(tx_buffer, "Stato caldaia: %s\nTemperatura acqua: %02d.%02d°C\n",
              digitalRead(OUT) ? "ON" : "OFF", (int)ActualTemp, (int)(ActualTemp*100)%100);                                
      myBot.sendMessage(msg.sender.id, tx_buffer);   
    }
    else {                                                      
      String reply;
      reply = (String)"Ciao " + msg.sender.username + 
              (String)"\n/accendi - per accendere la caldaia." +
              (String)"\n/spegni - per spegnere la caldaia." +
              (String)"\n/stato - per conoscere lo stato attuale.";
      myBot.sendMessage(msg.sender.id, reply);         
    }
  }  
}
*/
