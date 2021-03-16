/*
 * Questo nodo raccoglie la temperatura/umidità ambientale con un DHT11 e comanda un relè
 * v1.0 - Temperatura + Relé
 * v1.1 - Rimosso il relè, aggiunto fotoresistenza, IP Statico, pubblica valore di errore
 *
 * OUTTOPIC:
 * soffitta/nightMode/status
 *  Stato modalità notturna per disabilitare il lampeggio del LED
 * sala/temperatura
 *  Temperatura soggiorno
 * sala/umidità
 *  Umidità soggiorno
 * sala/luminosità
 *  Luminosità soggiorno (percentuale)
 * sala/error
 *  In caso di errore lo stato del topic va a 1
 *  
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SimpleDHT.h>

// Parametri WIFI
const char* ssid = "SSID";
const char* password = "password_wifi";
const char* mqtt_server = "192.168.0.70";

//Static IP address configuration
IPAddress staticIP(192, 168, 0, 73); //ESP static ip
IPAddress gateway(192, 168, 0, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
//IPAddress dns(0, 0, 0, 0);  //DNS

//Inizializzo i pin per il relè e il DHT11
//int relay = 5; //Corrisponde a D1  **Abbandonato**
const int analogInPin = A0;  // ESP8266 Analog Pin ADC0 = A0
int pinDHT11 = 2; //Corrisponde a D4

//Variabili di stato
const char* error = "0"; //In caso di errore ritorna 1

SimpleDHT11 dht11(pinDHT11);
char temp[10];
char umid[10];

WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  char msgpayload[50];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msgpayload[i] = (char)payload[i];
  }
  //////  ********   Dropped   *************
  //Faccio un confronto fra il topic ricevuto e la stringa che mi interessa, in questo caso solo le richieste per il relè
  if (strcmp(topic, "sala/relay/requestedState") == 0){
    //Per usare il pulsante da NodeRED eseguo uno switch dell'uscita
    if ((char)payload[0] == '2') {
      if (digitalRead(relay)) {
        //Se il relè è eccitato lo disattivo
        digitalWrite(relay, 0);
        Serial.println("Uscita disattivata da NodeRED");
      } else {
        //Altrimenti lo attivo
        digitalWrite(relay, 1);
        Serial.println("Uscita attivata da NodeRED");
      }
    }
    
    //In alternativa accetto anche 1 e 0 per gestire l'uscita
    if ((char)payload[0] == '1'){
        digitalWrite(relay, 1);
        Serial.println("Uscita attivata");
    } else if ((char)payload[0] == '0'){
        digitalWrite(relay, 0);
        Serial.println("Uscita disattivata");
    }*/  
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Una volta connesso pubblico i dati
      //Lo stato dei relè viene pubblicato nel loop principale
      // ... and resubscribe
      //client.subscribe("sala/relay/requestedState");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  error = "0";
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  //Inizializzo variabili per il DHT11
  byte temperature = 0;
  byte humidity = 0;
  int temp;
  int umid;
  char buffer[10];
  int err = SimpleDHTErrSuccess;

  /*
   * LUMINOSITÀ
   * Il sensore legge i valori di luminosità
   * BUIO = 1024
   * LUCE = 0
   * 
   * Inverto la misura (0=buio, 100=luce)
   * Scalo la misura 0-100%
   * Pubblico la misura su MQTT
   */
   
  int luminosityValue = 0;  // Valore letto dal comparatore di tensione per la fotoresistenza
  int luminosity = 0; //Valore scalato 0-100%
  luminosityValue = analogRead(analogInPin);
  Serial.print("RAW value = ");  Serial.println(luminosityValue);
  if (luminosityValue < 0 || luminosityValue > 1024){
    error = "1";
  }
  else{
    luminosity = abs((luminosityValue - 1024)) * 100 / 1024;
    Serial.print("Luminosity = ");  Serial.print(luminosity); Serial.println("%");
    dtostrf(luminosity, 0, 0, buffer);
    client.publish("sala/luminosità", buffer);
  }

  /*
   * TEMPERATURA E UMIDITÀ
   * Acquisisco i valori dal DHT11
   * Pubblico le misure su MQTT
   */
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err=");
    Serial.println(err);
    error = "1";
  }
  //Pubblico lo stato di errore per primo e lascio un ritardo per permettere a logiche esterne di registrare l'evento
  client.publish("sala/error", error);
  delay(300);
  
  String tempString = String((int)temperature);
  String umidString = String((int)humidity);
  Serial.print((int)temperature); Serial.print(" *C, ");
  Serial.print((int)humidity); Serial.println(" H");
  dtostrf(temperature, 0, 0, buffer);
  client.publish("sala/temperatura", buffer);
  dtostrf(humidity, 0, 0, buffer);
  client.publish("sala/umidità", buffer);
  delay(1500);
}
