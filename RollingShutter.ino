// 11-05-20
//Wemos d1 R2 with ide 165
/*Started on 200216
  references:
  https://thingsboard.io/docs/samples/esp8266/gpio/
  https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/
  http://onlineshouter.com/how-to-control-a-relay-from-a-web-page-with-nodemcu/
  https://jumpjack.wordpress.com/2017/06/10/appunti-esp8266-come-risolvere-problemi-di-stabilita/
  https://circuits4you.com/2016/12/16/esp8266-web-server-html/
*/

/*   For SMTP: 
  https://www.electronicshub.org/send-an-email-using-esp8266/
  https://app-eu.smtp2go.com/settings/users/
  https://www.base64encode.org/
	https://www.element14.com/community/people/neilk/blog/2019/02/22/send-an-email-from-esp8266

  SMTP2GO user:   dariomelucci@gmail.com    -> Base64encode ->  ZGFyaW9tZWx1Y2NpQGdtYWlsLmNvbQ==
  SMTP2GO Login pwd: bg142666
  SMTP2GO pwd:    dario95                   -> Base64encode ->  ZGFyaW85NQ==  
	
		ho inviato un request ad SMTP2GO il 16-04-2020 
			"Since i am an hobbist and for me it will be useful if my IoT devices based on ESP8266 
			could send mail for some events, my question is: may I have a way to avoid
			'550 that smtp username's account is not allowed to send' error?
			Thanks
			AntonioMelucci"
		dopo qualche minuto mi ha risposto Louise
			"Hi Antonio, 
				Thanks for getting in touch. I've enabled sending for your account 
				so you should be able to send now. 
			Kind regards,
			Louise
			SMTP2GO"     
*/
/* Activity step to send email:
    Step1:  client.connect(SMTP_SERVER,SMTP_PORT)         then wait for response 220 success
    Step2:  clinet.println("HELOmydomain.com")            then wait for response 250 success
    Step3:  clinet.println("AUTHLOGIN")                   then wait for response 250 success 
    Step4:  clinet.println("base64::encode(Sender_Login")) 
            clinet.println("base64::encode(Sender_Pwd"))  then wait for response 235 success
    Step5:  String mailFrom="MAIL FROM:<"+ String(From) + '>'
            client.println(mailFrom)
    Step6:  String mailFrom = "RCPT TO:<" + String(To) + '>'
            client.println(mailFrom)
    Step7   client.println("DATA")
            client.println(Message body)
            client.println(".")
            client.println(QUIT)
*/

//-------------------Included Libraries
#include <ESP8266WiFi.h>        //is required for doing all WiFi related functionalities such as connection, AP, etc.
#include <ESP8266WebServer.h>   //it handles all HTTP protocols
#include <ESP8266HTTPClient.h>  //for http request to IFTTT
#include <EEPROM.h>         //C:\Arduino165\hardware\tools\avr\avr\include\avr
//IFTTT connection:
//http://www.instructables.com/id/ESP8266-to-IFTTT-Using-Arduino-IDE/
//https://github.com/mylob/ESP-To-IFTTT
#include "DataToMaker.h"    //in questa stessa cartella
//#include <PubSubClient.h>   //C:\MyArduinoWorks\libraries\PubSubClient\src
//for NTP client
#include <NTPClient.h>
#include <WiFiUdp.h>
//invio e-mail


//-------------------Defines
#define SERIAL_DEBUG
#define TimeToReconnect 600
#define TimeToLooseWifi 200
#define TempoSerranda   40
#define Speed 9600  //bps di comunicazione
#define EEsize 32   //bytes reserved to EEprom data
#define mqtt_server "test.mosquitto.org"        //https://test.mosquitto.org/  
//To fire event to IFTTT (library DataToMaker is inside this folder)
//   https://maker.ifttt.com/trigger/rollingshutter/with/key/iRsU-c2LcNtn4HX6ZSD_BAiJpy6fYs7OSphOb7b2sc1
#define myIFTTTkey "iRsU-c2LcNtn4HX6ZSD_BAiJpy6fYs7OSphOb7b2sc1"    //account: atiliomelucci@gmail.com
#define myIFTTTevent "rollingshutter"


//-------------------collegamenti dei devices
#define PlsKeyLp A0         //Input analogico pulsante NC a muro per accensione lampada interna
#define Caldaia D0
#define DiagLed D4
#define UPcmd   D6          //Output Comando alza saracinesca
#define DWNcmd  D7          //Output Comando abbassa saracinesca
#define INTlamp D1          //Output Rele lampada interna
#define TLCcmd  D3          //Output Rele telecamera
#define EXTlamp D5          //Output Rele lampada sotto balcone
#define CitfIO  D2          //InputPullUp Pulsante remoto su citofono e uscita led rosso su citofono
#define PLSkey  D8          //Input Selettore a chiave (da chiudere su 3.3v)
#define K_CycleTime 500     //durata ms del ciclo principale
#define Alba      7
#define Tramonto  18 
#define TmaxIntLamp 3       //Minuti accensione lampada interna all'apertura da chiave       


//-------------------Global variables
String rootWebPage;
bool WiFiOn = true;     //If TRUE the internet link is OK, otherwise only key works.
bool RedBlink;          //Il led citofono lampeggia se serranda aperta e connessione internet assente
// Your WiFi credentials.Set password to "" for open networks.
const char* ssid = "2znirpUSN";
const char* password = "ta269179";
const IPAddress gateway_ip(192,168,4,1);
uint8_t ContaSec, ContaMin;
int  MemRele;   //Bit4-Stato telecamera
                //Bit3-Stato caldaia  
                //Bit2-Stato saracinesca        HIGH chiusa   LOW aperta  
                //Bit1-Stato Lampada esterna   
                //Bit0-Stato Lampada interna    HIGH spenta   LOW accesa
int Retry;
long StartTime, PrevTime;
// Variables to save date and time
String ServString ;
char msg[50];
int Ora, Minuti, MinPre ;
int StatusRQ;   //Comando proveniente da MQTT app su smartphone
int Status;           //Status int per essere memorizzato in EEprom
int OldStatus;        //Status = 0  Sistema a riposo
                      //Status = 1  Quando c'è il sole (la serranda NON si chiude da chiave)
                      //Status = 2  Serranda in movimento da chiave
                      //Status = 3  Lampada interna accesa da chiave dopo il crepuscolo per 5 minuti solo se WiFiOn = true
String Stati[10] = {"APERTA","CHIUSA","ExtON","ExtOFF","IntON","IntOFF","BoilON","BoilOFF","TLCon","TLCoff"};
//Account su SMTP2GO:
char SMTPserver[] = "mail.smtp2go.com"; // The SMTP Server
//  SMTP2GO user:   dariomelucci@gmail.com    -> Base64encode ->  ZGFyaW9tZWx1Y2NpQGdtYWlsLmNvbQ==
//  SMTP2GO Login pwd: bg142666
//  SMTP2GO pwd:    dario95                   -> Base64encode ->  ZGFyaW85NQ==  
char  Base64User[] = "ZGFyaW9tZWx1Y2NpQGdtYWlsLmNvbQ==";
char  Base64Pwd[]  = "ZGFyaW85NQ=="; 


//-------------------Classes
// Create an instance of the server, specify the port to listen on as an argument
ESP8266WebServer Iserver(80);             //Web server is on port 80, you can use other ports also, default HTTP port is 80
WiFiClient espClient;
//PubSubClient mqttClient(Iclient);       //-- https://pubsubclient.knolleary.net/api.html#setserver
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
DataToMaker IFTTTevent(myIFTTTkey, myIFTTTevent);    //for IFTTT pourpose
HTTPClient IhttpCLI;


//-------------------------StartUP routine
void setup(){
  Serial.begin(Speed);          // Open native serial communications:
// commit EEsize bytes of ESP8266 flash (for "EEPROM" emulation)
// this step actually loads the content (EEsize bytes) of flash into a EEsize-byte-array cache in RAM
  EEPROM.begin(EEsize);
  EEPROM.get(0,MemRele);
//
  pinMode(PlsKeyLp,INPUT);
//Stato precedente di rele lampada interna
  pinMode(INTlamp,OUTPUT);
  digitalWrite(INTlamp,HIGH);
  //if(bitRead(MemRele,0) == 0) Status = 3;     //Il processore s'è resettato con lampada interna accesa. Riparte il timer a spegnersi
//Stato precedente di rele lampada esterna
  pinMode(EXTlamp,OUTPUT);
  digitalWrite(EXTlamp,bitRead(MemRele,1));
//Stato precedente di rele caldaia.     Il relè di caldaia è a logica negata (LOW = eccitato) 
  pinMode(Caldaia,OUTPUT);
  digitalWrite(Caldaia,bitRead(MemRele,3));
//Stato precedente di rele telecamera.  Il relè di telecamera è a logica negata (LOW = eccitato) 
  pinMode(TLCcmd,OUTPUT);
  digitalWrite(TLCcmd,bitRead(MemRele,4));
//Rele alza serranda
  pinMode(UPcmd,OUTPUT);                        
  digitalWrite(UPcmd,HIGH);
//Rele abbassa serranda
  pinMode(DWNcmd, OUTPUT);                       
  digitalWrite(DWNcmd,HIGH);

  pinMode(DiagLed,OUTPUT);         //Led blu sulla scheda
  pinMode(PLSkey,INPUT);           //input selettore a chiave
  pinMode(CitfIO,OUTPUT); 
  Status = 0;
/* for wdog manipulation
https://techtutorialsx.com/2017/01/21/esp8266-watchdog-functions/
  ESP.wdtDisable();
  ESP.wdtEnable();
  ESP.wdtFeed();   */  
  ESP.wdtEnable(6000);
//Preparo la pagina HTML
  rootWebPage.reserve(600);
  ESPdiag();
  WiFiConnection();
  //---WiFiOn == FALSE se perduta connessione di rete
  if(WiFiOn == true){      
  //mqttClient.setServer(mqtt_server, 1883);
  //mqttClient.setCallback(callback);
// Initialize a NTPClient to get time
    timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
    timeClient.setTimeOffset(3600);
    //byte ret = sendEmail();
    startWebServer();
  }
}

//-------------------------Cyclic Jobs
void loop(){
  PrevTime = millis();
  RedBlink = !RedBlink;   //--Toggle serve quando WiFiOn = false
  if(WiFiOn == true){
    if(!WiFi.isConnected()) ESP.restart();
    //MQTTconn();
    extracTime();  
    digitalWrite(DiagLed,!digitalRead(DiagLed));
    composeWebPage();
  }
//Led rosso su citofono  
  pinMode(CitfIO,OUTPUT);
  if(WiFiOn == true) digitalWrite(CitfIO,!bitRead(MemRele,2));    //acceso fisso a serranda aperta, WiFi connesso  
  else{
    if(!bitRead(MemRele,2)){
      digitalWrite(CitfIO,RedBlink);  
    }
  }

//Tentativo di riconnessione al WiFi
  if(WiFiOn == false){
    Retry++;
    Serial.println(Retry); 
    if(Retry > TimeToReconnect){
      ESP.restart();
    }
  }
  
////---Dummy loop     
  StartTime = millis();
  while((millis() - StartTime) < K_CycleTime){
    ESP.wdtFeed(); 
  }
////---

//Una volta al minuto:
  if(Minuti != MinPre){
    Serial.println(Minuti);
    ContaMin++;
    //Una volta ogni TmaxIntLamp minuti
    if(ContaMin >= TmaxIntLamp){
      ContaMin = 0; 
      CheckRouterComm();
      //Spegnimento lampada interna a tempo se NTPserver è connesso 
      if(Status == 3){
        Status = 0; 
        digitalWrite(INTlamp,HIGH);
        bitSet(MemRele,0); 
      }
      //durante il giorno la lampada esterna va tenuta spenta
        if(Ora > Alba && Ora < Tramonto){
        digitalWrite(EXTlamp,HIGH);
        debugln("ExtOFF");
        bitSet(MemRele,1);
      }  
    }
    MinPre = Minuti;
  }
   
//Comando serranda da pulsante su citofono con cicli debounce
/*     GND----\/\/\/-----|<|-------D2(CitfIO input NO pullUp)
                              |-----------PLS N.O.-------3.3v
*/
  pinMode(CitfIO,INPUT);
  if(digitalRead(CitfIO) == 1){
    delay(300);
    if(digitalRead(CitfIO) == 1){
      delay(300); 
      if(digitalRead(CitfIO) == 1) Status = 2;
    }
  }
  if(WiFiOn == true){ 
//Reset della scheda se si blocca la pagina web 
    Iserver.handleClient();           //In main loop we handle client request
    /*Serial.print(millis());
    debug("  ");
    Serial.print(PrevTime);
    debug("  ");
    Serial.println(millis() - PrevTime); */
    if((millis() - PrevTime) > 10*K_CycleTime){
      //ESP.restart(); 
      Iserver.stop();
      delay(1000);
      startWebServer(); 
    }
  }
//Comando impulsivo da PLS NC a muro per accensione lampade interne se la saracinesca è già aperta
  if(bitRead(MemRele,2)== 0){
    int raw = analogRead(PlsKeyLp);
    if(raw < 500) LampadaInTempo();
  }
//Comando impulsivo da chiave alla saracinesca  Status = 1 di giorno,  Status = 2 dopo il tramonto
  if(digitalRead(PLSkey) == 1){
    Status = 2; 
    if(Ora > Alba && Ora < Tramonto) Status = 1;      //durante il giorno la saracinesca NON si chiude da chiave
    ContaSec = 0;
  }
//Apertura per 15 secondi e nuovo stato in EEprom
  if(bitRead(MemRele,2)== 1 && (Status > 0)){ 
    digitalWrite(UPcmd,LOW); 
    ContaSec++;
    Serial.println(ContaSec + 100);
    if(ContaSec > TempoSerranda){
      bitClear(MemRele,2);
      digitalWrite(UPcmd,HIGH);
      WriteEE(0,MemRele);
      ContaSec = 0;
      if(WiFiOn == true){
        IFTTTevent.setValue(1,Stati[0]);
        if(IFTTTevent.connect()) IFTTTevent.post(); 
      } 
      else Status = 0;
    }
//Se si apre da chiave o da citofono dopo il tramonto, si accende lampada interna per 3 minuti  
     if((Status == 2) && (WiFiOn == true)) LampadaInTempo();
  }
//Chiusura per 15 secondi e nuovo stato in EEprom se il sole è tramontato
//Viene spenta anche la lampada interna
  if(bitRead(MemRele,2)== 0 && (Status == 2)){ 
    digitalWrite(DWNcmd,LOW); 
    ContaSec++;
    Serial.println(ContaSec + 200);
    if(ContaSec > TempoSerranda){
      bitSet(MemRele,2);
      digitalWrite(DWNcmd,HIGH);
      bitSet(MemRele,0);
      digitalWrite(INTlamp,HIGH);
      Status = 0;
      ContaSec = 0;
      WriteEE(0,MemRele);
      if(WiFiOn == true){
        IFTTTevent.setValue(1,Stati[1]);
        if(IFTTTevent.connect()) IFTTTevent.post(); 
      }
    }
  }    
}
 
//=======================================================================
//=========================== SUBROUTINES ===============================
//=======================================================================
////////////////////
void composeWebPage(){
  rootWebPage = "<!DOCTYPE html><html><head><style>";
  rootWebPage += "header{";
  rootWebPage += "background: linear-gradient(to right, #1e5799 0%,#7db9e8 100%);";
  rootWebPage += "color: #fff;";
  rootWebPage += "padding:10px;";
  rootWebPage += "text-align: center;";
  rootWebPage += "vertical-align: middle;";
  rootWebPage += "}";
  
  rootWebPage += "body{";
  rootWebPage += "padding:15px;";
  rootWebPage += "color: #5e5e5e;";
  rootWebPage += "font-family: Helvetica,Arial,sans-serif;";
  rootWebPage += "font-variant: small-caps;";
  rootWebPage += "font-size:1em;";
  rootWebPage += "text-align: center;";
  rootWebPage += "}";
  
  rootWebPage += "footer{";
  rootWebPage += "background: linear-gradient(to left, #1e5799 0%,#7db9e8 100%);";
  rootWebPage += "color: #fff;";
  rootWebPage += "padding:15px;";
  rootWebPage += "text-align: right;";
  rootWebPage += "vertical-align: bottom;";
  rootWebPage += "}"; 
  
  rootWebPage += "h2{"; 
  rootWebPage += "padding:10px;";
  rootWebPage += "text-align: center;";
  rootWebPage += "vertical-align: middle;";
  rootWebPage += "font-size:2em;";
  rootWebPage += "}";
  
  rootWebPage += "</style></head><body><header>:: Ing Melucci _______";

  rootWebPage += String(Ora,DEC);
  rootWebPage += " : ";
  if(Minuti < 10) rootWebPage += "0";
  rootWebPage += String(Minuti,DEC);
  rootWebPage += "</header>";
//body
  rootWebPage += "<h2>";
  if(bitRead(MemRele,2) == 1){
    rootWebPage += "CHIUSA"; 
    //comando APRE serranda
    rootWebPage += "<h2><a href='?a=4'/> APRI    </a></h2>";  
  }
  else {
    rootWebPage += "APERTA"; 
 //Lampada interna solo se la saracinesca è aperta
    rootWebPage += "<h2>INTlamp___ <a href='?a=";
    if(bitRead(MemRele,0) == 1) rootWebPage += "2'/> ACCENDI </a></h2>";
    else rootWebPage += "3'/> SPEGNI </a></h2>";
    //rootWebPage += "<h2>INTlamp <a href='?a=2'/> ON    </a><a href='?a=3'/>   OFF</a></h2>"; 
  }

//Lampada esterna
  rootWebPage += "<h2>EXTlamp___ <a href='?a=";
  if(bitRead(MemRele,1) == 1) rootWebPage += "0'/> ACCENDI </a></h2>";
  else rootWebPage += "1'/> SPEGNI </a></h2>";
  //rootWebPage += "<h2>EXTlamp <a href='?a=0'/> ON    </a><a href='?a=1'/>   OFF</a></h2>"; 

//Caldaia
  rootWebPage += "<h2>CALDAIA___ <a href='?a=";
  if(bitRead(MemRele,3) == 1) rootWebPage += "6'/> ACCENDI </a></h2>";
  else rootWebPage += "7'/> SPEGNI </a></h2>";

//Telecamera
  rootWebPage += "<h2>TLC_______ <a href='?a=";
  if(bitRead(MemRele,4) == 1) rootWebPage += "8'/> ACCENDI </a></h2>";
  else rootWebPage += "9'/> SPEGNI </a></h2>";
  
//footer
  rootWebPage += "<footer>powerd by AntonioMelucci</br></footer></body></html>";
}


////////////////////When client request a web page by entering ESP IP address which 
//                  data to be sent is handled by subroutine and that subroutine name 
//                  is defined in Iserver.on(path,subroutine_name).
void startWebServer() {
  Iserver.on("/", handleRoot);
  Iserver.begin();              //To start the server use this command
  //Le prossime 3 righe su  https://github.com/esp8266/Arduino/issues/2907
  //                        https://www.evemilano.com/cache-control/
  //                        https://www.keycdn.com/blog/http-cache-headers
	Iserver.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); 
  Iserver.sendHeader("Pragma", "no-cache"); 
  Iserver.sendHeader("Expires", "-1");
  debug("SERVER BEGIN!!");
}


////////////////////
void handleRoot(){
//Telecamera ACCENDI (Logica negata) 
  if (Iserver.arg(0)[0] == '8'){ 
    digitalWrite(TLCcmd,LOW);    //   http://dario95.ddns.net:28083/?a=8
    bitClear(MemRele,4);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(2,Stati[8]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Telecamera SPEGNI (Logica negata) 
  if (Iserver.arg(0)[0] == '9'){
    digitalWrite(TLCcmd,HIGH);   //   http://dario95.ddns.net:28083/?a=9
    bitSet(MemRele,4);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(2,Stati[9]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Caldaia ACCENDI (Logica negata)  
  if (Iserver.arg(0)[0] == '6'){ 
    digitalWrite(Caldaia,LOW);    //   http://dario95.ddns.net:28083/?a=6
    bitClear(MemRele,3);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(2,Stati[6]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Caldaia SPEGNI (Logica negata)
  if (Iserver.arg(0)[0] == '7'){
    digitalWrite(Caldaia,HIGH);   //   http://dario95.ddns.net:28083/?a=7
    bitSet(MemRele,3);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(2,Stati[7]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Lampada Esterna ACCENDI (Logica negata)   
  if (Iserver.arg(0)[0] == '0'){ 
    digitalWrite(EXTlamp,LOW);    //   http://dario95.ddns.net:28083/?a=0
    ContaMin = 0;       //<--Perchè di giorno la lampada si spegne a tempo
    bitClear(MemRele,1);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(2,Stati[2]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Lampada Esterna SPEGNI (Logica negata)  
  if (Iserver.arg(0)[0] == '1'){
    digitalWrite(EXTlamp,HIGH);   //   http://dario95.ddns.net:28083/?a=1
    bitSet(MemRele,1);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(2,Stati[3]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Lampada Interna ACCENDI (Logica negata) 
  if (Iserver.arg(0)[0] == '2'){
    digitalWrite(INTlamp,LOW);    //   http://dario95.ddns.net:28083/?a=2
    bitClear(MemRele,0);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(3,Stati[4]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Lampada Interna SPEGNI (Logica negata) 
  if (Iserver.arg(0)[0] == '3'){ 
    digitalWrite(INTlamp,HIGH);   //   http://dario95.ddns.net:28083/?a=3
    bitSet(MemRele,0);
    WriteEE(0,MemRele);
    IFTTTevent.setValue(3,Stati[5]);
    if(IFTTTevent.connect()) IFTTTevent.post(); 
  }
//Apre serranda
  if (Iserver.arg(0)[0] == '4') Status = 1;                   //   http://dario95.ddns.net:28083/?a=4
//Chiude serranda (solo da ALEXA, no Telegram, no WebBrowser
  if (Iserver.arg(0)[0] == '5') Status = 2;                   //   http://dario95.ddns.net:28083/?a=5
    
  Iserver.send(200, "text/html", rootWebPage); 
}

/*
////////////////////http request to IFTTT webhook
void IFTTTwebhook(){
  HTTPClient http;                        //Declare an object of class HTTPClient
  http.begin("http://maker.ifttt.com/trigger/RollingShutter/with/key/JnHyZz88-9z7Fgyd56ETp");  //Specify request destination
  int httpCode = http.GET(); 
  debugi(httpCode);
  if (httpCode > 0){                      //Check the returning code
    String payload = http.getString();    //Get the request response payload
    Serial.println(payload);              //The response corresponds to a JSON payload.   
  }
  http.end();   //Close connection  
}


//++++++++++++ publish: "RollingShutFBK"   subscribe: "RollingShutCMD" 
void MQTTconn(){
  uint8_t tryNum = 0;
  while (!mqttClient.connected()){
    digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
    //debug("Connecting MQTT server...");
    // Attempt to connect
    if (mqttClient.connect("ESP8266Client")){
      //debugln("connected");
      // Once connected, publish an announcement...
      snprintf (msg, 75, "RollingShutter #%ld", Status);   
      mqttClient.publish("RollingShutFBK", msg);
      // ... and resubscribe
      mqttClient.subscribe("RollingShutCMD");
    } 
    else{
      debug("Ko, rc=");
      debugi(mqttClient.state());
      debugln(" try in 10 seconds");
      // Wait 5 seconds before retrying
      for(int8_t x=0; x<=50; x++){
        digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
        delay(200);
      }
      tryNum++;
      if(tryNum > 2) ESP.restart();   //https://github.com/esp8266/Arduino/issues/1722      
    }
  }
} 


//++++++++++++will handle the incoming messages for the topics subscribed ("NSUinTopic").
void callback(char* topic, byte* payload, unsigned int length){
  int8_t i;
  debug("Incoming [");
  debug(topic);
  debug("] ");
  for (i = 0; i < length; i++) {
    debugc((char)payload[i]);
  }
  debug("---");
  debugi(i);
  debugln();
  // Switch on the LED if an 1 was received as first character
  if (payload[0] == '0'){
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED GPIO1 off by making the voltage HIGH
    StatusRQ = 0;
  }
  if (payload[0] == '1'){
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED GPIO1 on (Note that LOW is the voltage level
    StatusRQ = 1;
  } 
  if (payload[0] == '4'){
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED GPIO1 on (Note that LOW is the voltage level
    StatusRQ = 4;
    StartTime = millis();
  } 
  if (payload[0] == '5'){
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED GPIO1 on (Note that LOW is the voltage level
    StatusRQ = 5;
  } 
  if (payload[0] == '6'){
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED GPIO1 on (Note that LOW is the voltage level
    StatusRQ = 6;
  }
  //Only for diagnosis on each rele:
  if (payload[0] == '7') StatusRQ = 7;
  if (payload[0] == '8') StatusRQ = 8;
  if (payload[0] == '9') StatusRQ = 9;
} */

////////////////////  Connessione al router con indirizzo fisso
void WiFiConnection(){
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);   //This command is used to connect to your WiFi Access point.
  WiFi.config(IPAddress(192, 168, 4, 37), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  StartTime = 0;
  while(WiFi.status() != WL_CONNECTED){  
    delay(250);  
    debug("."); 
    digitalWrite(DiagLed,!digitalRead(DiagLed));
    Retry++;
    if(Retry > TimeToLooseWifi){
      WiFiOn = false;         //Internet connection lost
      debugln(" ");
      debugln("WiFiOff");
      Retry = 0;
      return ;
    }
  }  
  debugln("");  
  debugln("connected: ");  
  debugln(ssid);  
  debugln("WiFi ok");  
// Avvia il server  
  Iserver.begin();  
  debugln("Server avviato");  
// Stampa l'indirizzo IP  
  debug("URL : ");  
  debug("http://");  
  Serial.print(WiFi.localIP()); // Restituisce lo IP della scheda  
  debugln("/");   
} 

 
////////////////////  https://media.readthedocs.org/pdf/arduino-esp8266/docs_to_readthedocs/arduino-esp8266.pdf
void ESPdiag(){
  Serial.println(ESP.getChipId());                //returns the ESP8266 chip ID as a 32-bit integer.
  Serial.println(ESP.getFlashChipSpeed());        //returns the flash chip frequency, in Hz.
  Serial.println(ESP.getFreeSketchSpace());       //returns the free sketch space as an unsigned 32-bit integer
}


////////////////////Recupero ora e minuti del giorno
void extracTime(){
  timeClient.update(); 
  ServString = timeClient.getFormattedDate();
  // The formattedDate comes with the following format:  2018-05-28T16:00:13Z
  // We need to extract date and time
  //Serial.println(formattedDate);
  uint8_t split = ServString.indexOf('T'); 
  //dayStamp = formattedDate.substring(0, split);
  Ora = (ServString.substring(split+1, ServString.length()-7)).toInt();  //ora del giorno
  Minuti = (ServString.substring(split+4, ServString.length()-4)).toInt();  //Minuti dell'ora
  //debugi(Ora);
}


//////////////////// Accensione lampada interna a tempo e invio mail a mezzo SMTP2GO
void LampadaInTempo(){
  digitalWrite(INTlamp,LOW);    //   http://dario95.ddns.net:28083/?a=2
  bitClear(MemRele,0); 
  Status = 3;
  ContaMin = 0;
  byte ret = sendEmail();   
}

      
////////////////////Richiedo la pagina html iniziale al router
void CheckRouterComm(){
  IhttpCLI.begin("http://192.168.4.1/login"); //HTTP 
  debug("[HTTP] GET...\n"); 
  int httpCode = IhttpCLI.GET();    // start connection and send HTTP header
  // httpCode will be negative on error
  if(httpCode > 0){
    //HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    // file found at server
    if(httpCode == HTTP_CODE_OK){
      String payload = IhttpCLI.getString(); 
      debugln(payload); 
    }
  }
  else{
    Serial.printf("[HTTP] GET... failed, error: %s\n", IhttpCLI.errorToString(httpCode).c_str());
    WiFiConnection();
  }
}


//////////////////http://esp8266.github.io/Arduino/versions/2.0.0/doc/libraries.html#eeprom
void WriteEE(int loc,int value){
//replace values in byte-array cache with modified data 
//no changes made to flash, all in local byte-array cache
  EEPROM.put(loc,value);
// actually write the content of byte-array cache to hardware flash.
// flash write occurs if and only if one or more byte
// in byte-array cache has been changed, but if so, ALL EEsize bytes are 
// written to flash
  EEPROM.commit();  
}


////////////////// https://www.electronicshub.org/send-an-email-using-esp8266/
//  If you use F() you can move constant strings to the program memory instead of the ram. 
//  This will take up space that will decrease the amount of other code you can write. But it will free up dynamic ram.
byte sendEmail(){
	//Step1:  client.connect(SMTP_SERVER,SMTP_PORT)         then wait for response 220 success
  if (espClient.connect(SMTPserver, 2525) == 1) debugln("SMTPconnected");
  else {
    debugln("SMTPconnFail"); 
    return 0;
  }
  if(!emailResp()) return 0;
	
  //Step2:  clinet.println("HELOmydomain.com")            then wait for response 250 success
  debugln(F("EHLO"));
  espClient.println("EHLO www.example.com");
  if(!emailResp()) return 0;
 
  /*Serial.println(F("Sending TTLS"));
  espClient.println("STARTTLS");
  if (!emailResp()) 
  return 0;*/
	
  //Step3:  clinet.println("AUTH LOGIN")                  then wait for response 250 success 
  debugln(F("auth"));
  espClient.println("AUTH LOGIN");
  if(!emailResp()) return 0;
		
  //Step4a:  clinet.println("base64::encode(Sender_Login"))  
  debugln(F("User"));
  // Change this to your base64, ASCII encoded username
  //For example, the email address test@gmail.com would be encoded as dGVzdEBnbWFpbC5jb20=
  espClient.println(Base64User); //base64, ASCII encoded Username
  if (!emailResp()) return 0;
  //Step4b:		clinet.println("base64::encode(Sender_Pwd"))  then wait for response 235 success
  debugln(F("Pwd"));
  // change to your base64, ASCII encoded password
  //  For example, if your password is "testpassword" (excluding the quotes), it would be encoded as dGVzdHBhc3N3b3Jk
  espClient.println(Base64Pwd);//base64, ASCII encoded Password
  if(!emailResp()) return 0;
  
	//Step5:  String mailFrom="MAIL FROM:<"+ String(From) + '>'
	//				client.println(mailFrom)
  debugln(F("From"));
  espClient.println(F("MAIL From: dariomelucci@gmail.com"));		// change to sender email address also, Not a real address 
  if(!emailResp()) return 0;
		
  //Step6:  String mailFrom = "RCPT TO:<" + String(To) + '>'
	//				client.println(mailFrom)
  debugln(F("To"));
  espClient.println(F("RCPT To: linaleggieri36@gmail.com"));
  if(!emailResp()) return 0;
	
	//Step7a  client.println("DATA")
  debugln(F("DATA"));
  espClient.println(F("DATA"));
  if(!emailResp()) return 0;
		
	//Step7b	client.println(Message body)
  debugln(F("Sending email"));
  // change to recipient address
  espClient.println(F("To:  linaleggieri36@gmail.com"));
  // change to your address
  espClient.println(F("From: dariomelucci@gmail.com"));
  espClient.println(F("Subject: RollingShutter\r\n"));
  espClient.println(F("Lampada Interna Accesa.\n"));
  //espClient.println(F("Second line of the test e-mail."));
  //espClient.println(F("Third line of the test e-mail."));

  //Step7c	client.println(".") this terminates the email 
  espClient.println(F("."));
  if(!emailResp()) return 0;

	//Step7d	client.println(QUIT)
  debugln(F("QUIT"));
  espClient.println(F("QUIT"));
  if(!emailResp()) return 0;
  //
  espClient.stop();
  debugln(F("disconnected"));
  return 1;
}


////////////////// https://www.electronicshub.org/send-an-email-using-esp8266/
byte emailResp(){
  byte responseCode;
  byte readByte;
  int loopCount = 0;
  while (!espClient.available()){
    delay(1);
    loopCount++;
    // Wait for 20 seconds and if nothing is received, stop.
    if (loopCount > 20000){
      espClient.stop();
      debugln("Timeout");
      return 0;
    }
  }
  responseCode = espClient.peek();
  while (espClient.available()){
    readByte = espClient.read();
    Serial.write(readByte);
  }
  if (responseCode >= '4'){
    //  efail();
    return 0;
  }
  return 1;
}



void debug(String message){
#ifdef SERIAL_DEBUG
  Serial.print(message);
#endif
}
void debugc(char message){
#ifdef SERIAL_DEBUG
  Serial.print(message);
#endif
}
void debugi(int message){
#ifdef SERIAL_DEBUG
  Serial.print(message);
#endif
}
void debugln(String message){
#ifdef SERIAL_DEBUG
  Serial.println(message);
#endif
}
void debugln(){
#ifdef SERIAL_DEBUG
  Serial.println();
#endif
}
