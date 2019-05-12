#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h>        //https://github.com/tzapu/WiFiManager
#include <CTBot.h>              //https://github.com/shurillu/CTBot

#define OFF LOW
#define ON HIGH
#define TELEGRAM_TIME 500       // Intervallo di tempo con cui vengono controllati i messaggi
#define UPDATE_TEMP_TIME 1000   // Intervalo di tempo per aggiornare la temperatura
#define CHECK_ON_TIME 20000     // CHECK_ON_TIME ms dopo aver attivato la caldaia, eseguo una verifica sulla temperatura raggiunta
#define ON_TEMP 45.0F           // Temperatura oltre la quale considero la caldaia partita correttamente

// Messaggi di callback per Telegram per esecuzione query
#define START_CALLBACK  "Comando bruciatore ON" 
#define STOP_CALLBACK "Comando bruciatore OFF"
#define STATE_CALLBACK "Stato bruciatore"
#define CONFIRM_CALLBACK "Esegui"
#define CANCEL_CALLBACK "Annulla"

// Enum degli stati finiti possibili
typedef enum { WAIT = -1, STOP = 0, START = 1, STATE = 3};
// Strutura per memorizzare messaggio Telegram
typedef struct  {
  int request = WAIT;
  bool confirm = false;
  uint16_t value1;
  uint16_t value2;
} telegramCmd;

// Vars
const uint8_t OUT = D1;    
uint32_t checkTelegramTime, updateTempTime, checkTempTime; 
uint8_t heatStatus = ON;

// Telegram vars
CTBot myBot;
CTBotInlineKeyboard myKbd, goKbd;  // custom inline keyboard object helper
String token;
TBMessage msg;
telegramCmd cmdMsg;

// Dallas DS18B20
#define ONE_WIRE_BUS D3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress ds18b20;
float  ActualTemp = 25.0F;

// Wifi handler
WiFiEventHandler connectedHandler;
WiFiEventHandler disconnectedHandler;

void setup() {  
  pinMode(OUT, OUTPUT);
  Serial.begin(115200); 
  connectedHandler = WiFi.onStationModeConnected(&onConnection);
  disconnectedHandler = WiFi.onStationModeDisconnected(&onDisconnection);
  
  // Avvio del webserver utie per memorizzare le credenziali del WiFi e le altre variabili
  // come il token Telegram (TODO: inserire anche la temperatura ON, ed il tempo di attesa)
  WiFiManager wifiManager;  
  WiFiManagerParameter telegramToken("Telegram", "Token", "token_placeholder", 50);
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
  
  // Avvio gestione sensore di temperatura Dallas DS18B20
  sensors.begin();  
  Serial.printf("Locating devices...\nFound %d devices\n", sensors.getDeviceCount());
  if (!sensors.getAddress(ds18b20, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(ds18b20, 10);
  sensors.requestTemperatures();
  
  // Se la temperatura attuale è maggiore di ON_TEMP considero la caldaia accesa
  ActualTemp = sensors.getTempC(ds18b20);
  if (ActualTemp > ON_TEMP)
    heatStatus = ON;      
  else 
    heatStatus = OFF;     
}

void loop() { 
  // Controllo se è arrivato qualche messaggio da Telegram
  if(millis() - checkTelegramTime > TELEGRAM_TIME){
    checkTelegramTime = millis();
    checkTelegramKbd();
  }
  
  // Aggiorno la temperatura attuale letta dal sensore 
  if(millis() - updateTempTime > UPDATE_TEMP_TIME){ 
    updateTempTime = millis();
    sensors.requestTemperatures();          
    ActualTemp = sensors.getTempC(ds18b20);     
    Serial.printf("Temperature: %02d.%02d°C\n", (int)ActualTemp, (int)(ActualTemp*100)%100);
  }
  
  // Se ho ricevuto il comando di START e sono trascorsi CHECK_ON_TIME ms, controllo la temperatura 
  // e fornisco feedback all'utente con un messaggio Telegram
  if((millis() - checkTempTime > CHECK_ON_TIME) && (cmdMsg.request == START)){    
    char tx_buffer[80];
    if(ActualTemp > ON_TEMP){
      sprintf(tx_buffer, "Tutto ok!\nIl bruciatore è partito correttamente\nTemperatura acqua: %02d.%02d°C\n",
                         (int)ActualTemp, (int)(ActualTemp*100)%100);                                
      myBot.sendMessage(msg.sender.id, tx_buffer);   
      Serial.println(tx_buffer);
      heatStatus = ON;
      cmdMsg.request = WAIT;         
    }
    else{
      sprintf(tx_buffer, "Attenzione!\nIl bruciatore non è partito.\nTemperatura acqua: %02d.%02d°C\n",
                         (int)ActualTemp, (int)(ActualTemp*100)%100);                                
      myBot.sendMessage(msg.sender.id, tx_buffer);   
      Serial.println(tx_buffer);
      heatStatus = OFF;
      cmdMsg.request = WAIT;      
    }    
  }
  
  /* Per evitare comandi accidentali, ogni variazione di stato ricevuta da Telegram va confermata.
     Quindi alla richiesta di START ad esempio, invio un messaggio di richiesta conferma e solo quando
     ricevo risposta positiva/negativa eseguo il comando previsto 
  */
  if(cmdMsg.confirm == true){
    cmdMsg.confirm = false;
    if(cmdMsg.request == START && heatStatus == OFF){
      digitalWrite(OUT, ON);   
      delay(2000);
      digitalWrite(OUT, OFF);      
    } 
    else if(cmdMsg.request == STOP && heatStatus == ON){
      digitalWrite(OUT, ON);
      delay(2000);
      digitalWrite(OUT, OFF);        
      cmdMsg.request = WAIT;                
    }
  }
}

// Funzione di callback che viene eseguita al salvataggio della configurazione (WiFiManager)
void saveConfigCallback() {
  Serial.println("Congigurazione salvata in EEPROM");  
}

// Funzione di callback che viene eseguita alla connessione ad una rete WiFi
void onConnection(const WiFiEventStationModeConnected& evt) {
  Serial.println("\nWiFi connesso");     
  //myBot.wifiConnect(ssid, pass);
  myBot.setTelegramToken(token);
  Serial.print("\nTelegram test connessione ");
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

// Funzione di callback che viene eseguita alla disconnessione da una rete WiFi
void onDisconnection(const WiFiEventStationModeDisconnected& evt) {
  Serial.print("WiFi disconnected. Reason: ");  
  Serial.println(evt.reason); 
}

// Controllo dei messaggi ricevuti da Telegram
void checkTelegramKbd(){
  //TBMessage msg;
  #define TX_SIZE 120
  // Abiamo un nuovo messaggio?
  if (myBot.getNewMessage(msg)) {    
    char tx_buffer[TX_SIZE];
    // E' un messaggio di testo qualsiasi
    if (msg.messageType == CTBotMessageText) {       
      sprintf(tx_buffer, "Ciao %s.!\nStato bruciatore : %s\nTemperatura acqua : %02d.%02d°C\n",
              msg.sender.username.c_str(), heatStatus ? "ON" : "OFF", (int)ActualTemp, (int)(ActualTemp * 100) % 100);
      myBot.sendMessage(msg.sender.id, tx_buffer, myKbd);
    } 
    // E' una query, ovvero una richiesta da eseguire o un messaggio di conferma
    else if (msg.messageType == CTBotMessageQuery) {      
      if (msg.callbackQueryData.equals(START_CALLBACK)) {
        cmdMsg.request = START;          
        checkTempTime = millis();
        // Messaggio di "feedback" inviato alla ricezione della query
        myBot.endQuery(msg.callbackQueryID, "Comando di accensione bruciatore ricevuto..");    
        // Questo messaggio richiede conferma
        cmdMsg.confirm = false; 
        myBot.sendMessage(msg.sender.id, "Confermi l'esecuzione del comando?", goKbd);  
      } 
      else if (msg.callbackQueryData.equals(STOP_CALLBACK)) {        
        cmdMsg.request = STOP;    
        myBot.endQuery(msg.callbackQueryID, "Comando di spegnimento bruciatore ricevuto..");
        // Questo messaggio richiede conferma
        cmdMsg.confirm = false; 
        myBot.sendMessage(msg.sender.id, "Confermi l'esecuzione del comando?", goKbd);              
      }
      else if (msg.callbackQueryData.equals(STATE_CALLBACK)) {
        cmdMsg.confirm = true;    // Questo messaggio non richiede conferma    
        sprintf(tx_buffer, "Stato bruciatore: %s\nTemperatura acqua: %02d.%02d°C\n",
                            heatStatus ? "ON" : "OFF", (int)ActualTemp, (int)(ActualTemp*100)%100);
        myBot.endQuery(msg.callbackQueryID, "");                                  
        myBot.sendMessage(msg.sender.id, tx_buffer);   
      }
      else if (msg.callbackQueryData.equals(CONFIRM_CALLBACK)) {
        cmdMsg.confirm = true;      // Questo messaggio non richiede conferma                                      
        checkTempTime = millis();   // Faccio partire il timer per controllo temperatura
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
