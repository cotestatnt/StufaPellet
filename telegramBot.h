#define START_CALLBACK    "START" 
#define STOP_CALLBACK     "STOP"
#define STATE_CALLBACK    "Stato attuale"
#define CONFIRM_CALLBACK  "Conferma"
#define CANCEL_CALLBACK   "Annulla"

// Instantiate the ssl client used to communicate with Telegram's web API
WiFiClientSecure sslPollClient;

// Telegram vars
String token = "488075445:AAFLd-B-spUviVfhMTQFWrRApG7t4gIPSWQ";
telegramCmd cmdMsg;


// Instantiate the client with secure token and client
TelegramBotClient myBot( token, sslPollClient);
      
// Instantiate a keybord with rows
TBCKeyBoard myKbd(2, false, true);
TBCKeyBoard goKbd(1, false, true);



// Function called on receiving a message
void onReceive (TelegramProcessError tbcErr, JwcProcessError jwcErr, Message* msg){      
  
  /*
    Serial.println("onReceive");
    Serial.print("tbcErr"); Serial.print((int)tbcErr); Serial.print(":"); Serial.println(toString(tbcErr));
    Serial.print("jwcErr"); Serial.print((int)jwcErr); Serial.print(":"); Serial.println(toString(jwcErr));
  
    Serial.print("UpdateId: "); Serial.println(msg->UpdateId);      
    Serial.print("MessageId: "); Serial.println(msg->MessageId);
    Serial.print("FromId: "); Serial.println(msg->FromId);
    Serial.print("FromIsBot: "); Serial.println(msg->FromIsBot);
    Serial.print("FromFirstName: "); Serial.println(msg->FromFirstName);
    Serial.print("FromLastName: "); Serial.println(msg->FromLastName);
    Serial.print("FromLanguageCode: "); Serial.println(msg->FromLanguageCode); 
    Serial.print("ChatId: "); Serial.println(msg->ChatId);
    Serial.print("ChatFirstName: "); Serial.println(msg->ChatFirstName);
    Serial.print("ChatLastName: "); Serial.println(msg->ChatLastName);
    Serial.print("ChatType: "); Serial.println(msg->ChatType);
    Serial.print("Text: "); Serial.println(msg->Text);
    Serial.print("Date: "); Serial.println(msg->Date);

    // Sending the text of received message back to the same chat
    // and add the custom keyboard to the message
    // chat is identified by an id stored in the ChatId attribute of msg    
    //client.postMessage(msg->ChatId, msg->Text, board);
    */
  
    char tx_buffer[TX_SIZE];
    int32_t id = msg->FromId;
    String message = msg->Text;
       
    Serial.printf("\nText: %s from id %d\n", message.c_str(), id);

    if (telegramId != id && id != 0 ){
      telegramId = id;
      saveConfig();
    }    
  
    if (message.equals(START_CALLBACK)) {
      cmdMsg.confirm = false;  
      cmdMsg.request = START;          
      checkTempTime = millis();                                  
      myBot.postMessage(id, "Confermi l'esecuzione del comando?", goKbd);  
    } 
    else if (message.equals(STOP_CALLBACK)) {        
      cmdMsg.confirm = false;                                   
      cmdMsg.request = STOP;            
      myBot.postMessage(id, "Confermi l'esecuzione del comando?", goKbd);              
    }
    else if (message.equals(STATE_CALLBACK)) {
      cmdMsg.confirm = false;        
      sprintf(tx_buffer, "Stato bruciatore: %s\nTemperatura acqua: %02d.%02d°C\n",
                          state[idxState], (int)ActualTemp, (int)(ActualTemp*100)%100);                                   
      myBot.postMessage(id, tx_buffer);   
    }
    else if (message.equals(CONFIRM_CALLBACK)) {
      cmdMsg.confirm = true;                                      
      checkTempTime = millis(); 
      if(cmdMsg.request != WAIT)
        myBot.postMessage(id, "Comando eseguito.", myKbd); 
      else
        myBot.postMessage(id, "Non c'è nessun comando da eseguire.", myKbd); 
    }
    else if (message.equals(CANCEL_CALLBACK)) {
      cmdMsg.confirm = false;                                  
      if(cmdMsg.request != WAIT)
        myBot.postMessage(id, "Comando annullato.", myKbd); 
      else
        myBot.postMessage(id, "Non c'è nessun comando da annullare.", myKbd); 
    } 
    else{            
      sprintf(tx_buffer, "Ciao %s.!\nStato bruciatore : %s\nTemperatura acqua : %02d.%02d°C\n",
              msg->FromFirstName.c_str(), state[idxState], (int)ActualTemp, (int)(ActualTemp * 100) % 100);
      myBot.postMessage(id, tx_buffer, myKbd);
    } 
    
      
}

// Function called if an error occures
void onError (TelegramProcessError tbcErr, JwcProcessError jwcErr){
  Serial.println("onError");
  Serial.print("tbcErr"); Serial.print((int)tbcErr); Serial.print(":"); Serial.println(toString(tbcErr));
  Serial.print("jwcErr"); Serial.print((int)jwcErr); Serial.print(":"); Serial.println(toString(jwcErr));
}


void initTelegramBot(){
  sslPollClient.setInsecure();
  // push() always returns the keyboard, so pushes can be chained 
  const String myKbdrow1[] = {START_CALLBACK, STOP_CALLBACK};
  const String myKbdrow2[] = {STATE_CALLBACK};
  const String goKbdrow1[] = {CONFIRM_CALLBACK, CANCEL_CALLBACK};
  myKbd
    .push(2, myKbdrow1)
    .push(1, myKbdrow2);
    
  goKbd
    .push(2, goKbdrow1);
  
  // Sets the functions implemented above as so called callback functions,
  // thus the client will call this function on receiving data or on error.
  myBot.begin(      
      onReceive,
      onError);   

}
