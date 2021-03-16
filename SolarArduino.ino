/*
 * SOLAR ARDUINO
 * Il mio primo (e ultimo) sistema per il monitoraggio dei miei pannelli da 30V in giardino
 * La mia idea era quella di monitorare tensione e corrente dei pannelli, delle batterie e dell'inverter per
 * avere una specie di accumulo casalingo (2 pannelli da 100W che facevano 100W insieme se erano di buon umore,
 * 3 batterie da auto recuperate di cui solo due esplose in seguito, un grid-tie-inverter cinese morto in inverno.
 * Se l'inverter non fosse morto mi sarei ripagato tutto in circa 60-65 anni. Peccato...
 * 
 * Comunque il sistema era composto da un Arduino MEGA che acquisiva le misure tramite 3 ACS712 per le correnti e 
 * 3 partitori di tensione.
 * Le misure poi venivano convertite e spedite tramite seriale su USB in formato JSON ad un Raspberry Zero che poi
 * rispediva tramite MQTT al mio server Rasberry in casa.
 * 
 * Comunque oltre la misura di tensione dei partitori di tensione al raspberry inviavo anche la tensione stessa.
 * Al secondo Raspberry bruciato credo di poter affermare che 25V su una presa USB siano un po' troppi.
 * 
 * 1.3.4 - Aumentato ritardo a 30s
 *         Impostato baudrate a 4800 per non dover modificarlo in tasmota
 * 1.3.3 - Aumentato ritardo invio misure a 15s una dall'altra
 *         Ridotto misure inviate. I valori verranno elaborati esternamente
 * 1.3.3 - 
 * 1.3.2 - Modifiche temporanee
 * 1.3.1 - Aggiunta la potenza in modo da scaricare un po' di calcolo dal raspberry
 * 1.3 - Aggiunto annidamenti ai dati in formato JSON
 * 1.2 - OK Conversione dati in formato Json. Vai su https://arduinojson.org/v6/assistant/ per calcolare la dimensione degli oggetti Json
 * 1.1 - OK DHT11
 * 1.0 - OK Acquisizione misure tensione e corrente
 */
 
#include <SimpleDHT.h>
#include <ArduinoJson.h>

#define MSG_DELAY 30000 //Ritardo usato nell'invio dei messaggi. Preferisco non intasare le comunicaziani per Node-Red. In ogni caso devo avere un ritardo di 2s tra una lettura e l'altra al DHT11

int panelTensionPin = A0;
int batteryTensionPin = A1;
int loadTensionPin = A2;

float panelTension = 0;
float batteryTension = 0;
float loadTension = 0;

int panelCurrentPin = A3;
int batteryCurrentPin = A4;
int loadCurrentPin = A5;

float panelCurrent = 0;
float batteryCurrent = 0;
float loadCurrent = 0;

//Sensore temperatura interno quadro (DHT11)
int dht11Pin = 2;
SimpleDHT11 dht11(dht11Pin);

//Sensore temperatura esterno quadro
float outTemperature = 0;
float outHumidity = 0;
float outPressure = 0;
float outLuminosity = 0;

int inTemperature = 0;
int inHumidity = 0;

void setup(){
  Serial.begin(9600);
}

void loop(){

  //float loadPower = 0;
  //float solarPanelPower = 0;
  
  readCurrents();
  readTensions();
  readInTemperature();

//  loadPower = loadTension * loadCurrent;
//  solarPanelPower = panelTension * panelCurrent;

  //Formatto tutti i dati in formato JSON e li pubblico sulla seriale
  DynamicJsonDocument solarPanel(110);
  solarPanel["object"] = "solar_panel";
  JsonObject solarPanelValue = solarPanel.createNestedObject("values");
  solarPanelValue["tension"] = panelTension;
  solarPanelValue["current"] = panelCurrent;
  //solarPanelValue["power"] = solarPanelPower;
  
  DynamicJsonDocument battery(110);
  battery["object"] = "battery";
  JsonObject batteryValue = battery.createNestedObject("values");
  batteryValue["tension"] = batteryTension;
  batteryValue["current"] = batteryCurrent;
  
  DynamicJsonDocument load(110);
  load["object"] = "load";
  JsonObject loadValue = load.createNestedObject("values");
  loadValue["tension"] = loadTension;
  loadValue["current"] = loadCurrent;
  //loadValue["power"] = loadPower;
  
  DynamicJsonDocument electricalCabinet(250);
  electricalCabinet["object"] = "electricalCabinet";
  JsonObject electricalCabinetInsideValue = electricalCabinet.createNestedObject("inside");
  electricalCabinetInsideValue["temperature"] = inTemperature;
  electricalCabinetInsideValue["humidity"] = inHumidity;

  //Scrivo sulla seriale i dati acquisiti in formato JSon
  serializeJson(solarPanel, Serial);
  Serial.println();
  delay(MSG_DELAY);
  serializeJson(battery, Serial);
  Serial.println();
  delay(MSG_DELAY);
  serializeJson(load, Serial);
  Serial.println();
  delay(MSG_DELAY);
  serializeJson(electricalCabinet, Serial);
  Serial.println();
  delay(MSG_DELAY);
}

void readInTemperature(){

  //Codice preso dall'esempio della libreria SimpleDHT. Non prendo in considerazione gli errori
  byte temperature = 0;
  byte humidity = 0;
  dht11.read(&temperature, &humidity, NULL);
  inTemperature = (int)temperature;
  inHumidity = (int)humidity;
}

void readCurrents(){
  
  //Leggo i valori dei sensori e converto la misure in A
  /*
  panelCurrent = (analogRead(panelCurrentPin) - SENSOR_RELATIVE_ZERO) * MAX_SENSOR_CURRENT / 512.0;
  batteryCurrent = (analogRead(batteryCurrentPin) - SENSOR_RELATIVE_ZERO) * MAX_SENSOR_CURRENT / 512.0;
  loadCurrent = (analogRead(loadCurrentPin) - SENSOR_RELATIVE_ZERO) * MAX_SENSOR_CURRENT / 512.0; */
  panelCurrent = analogRead(panelCurrentPin);
  batteryCurrent = analogRead(batteryCurrentPin);
  loadCurrent = analogRead(loadCurrentPin);
  
}

void readTensions(){
  
  //Leggo i valori e converto le misure in V
  /*
  panelTension = MAX_SENSOR_TENSION - (analogRead(panelTensionPin) * MAX_SENSOR_TENSION / 1024.0);
  batteryTension = MAX_SENSOR_TENSION - (analogRead(batteryTensionPin) * MAX_SENSOR_TENSION / 1024.0);
  loadTension = MAX_SENSOR_TENSION - (analogRead(loadTensionPin) * MAX_SENSOR_TENSION / 1024.0);
  */
  //Modifica: acquisisco i valori diretti per gestirlicon Raspberry
  panelTension = analogRead(panelTensionPin);
  batteryTension = analogRead(batteryTensionPin);
  loadTension = analogRead(loadTensionPin);
}
