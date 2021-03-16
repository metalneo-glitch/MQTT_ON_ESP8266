/*
 * TERMOSTATO
 * Il sistema è composto da un WeMos mini che acquisendo la temperatura da un DHT11
 * o da una temperatura esterna ricevuta tramite MQTT, comanda un relè per attivare
 * il riscaldamento
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <SimpleDHT.h>

/*
   WIFI configuration
   Static IP configuration
   MQTT server configuration
   MQTT topics
*/

const char* ssid = "SSID_name";
const char* password = "wifi_password";
const char* host = "WeMos Termostato";

IPAddress staticIP(192, 168, 0, 99); //ESP static ip
IPAddress gateway(192, 168, 0, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
IPAddress dns(192, 168, 0, 1);  //DNS

const char* mqtt_server = "192.168.0.70";
WiFiClient espClient;
PubSubClient client(espClient);

#define pinRelay 4 //This correspond to D2 on the WeMos

const char* mqtt_relay_requestedState_topicName = "bathroom/thermostat/relay/requestedState";
const char* mqtt_relay_status_topicName = "bathroom/thermostat/relay/status";
const char* mqtt_relay_error_topicName = "bathroom/thermostat/relay/error";

const char* mqtt_thermostat_mode_topicName = "bathroom/thermostat/mode";
const char* mqtt_thermostat_hysteresisUp_topicName = "bathroom/thermostat/hysteresisUp";
const char* mqtt_thermostat_hysteresisDown_topicName = "bathroom/thermostat/hysteresisDown";
const char* mqtt_thermostat_extTemp_topicName = "bathroom/thermostat/extTemp"; //Supplied external temperature
const char* mqtt_thermostat_edgeDetection_topicName = "bathroom/thermostat/edgeDetection";
const char* mqtt_thermostat_status_topicName = "bathroom/thermostat/status";

const char* mqtt_thermostat_setpoint_topicName = "bathroom/thermostat/setpoint";
const char* mqtt_thermostat_setpointLow_topicName = "bathroom/thermostat/setpointLow";
const char* mqtt_thermostat_setpointHigh_topicName = "bathroom/thermostat/setpointHigh";

const char* mqtt_thermostat_setpointSet_topicName = "bathroom/thermostat/setpointSet";
const char* mqtt_thermostat_setpointLowSet_topicName = "bathroom/thermostat/setpointLowSet";
const char* mqtt_thermostat_setpointHighSet_topicName = "bathroom/thermostat/setpointHighSet";

const char* mqtt_status_topicName = "bathroom/status";
const char* mqtt_error_topicName = "bathroom/error";

const char* mqtt_temperature_TopicName = "bathroom/temperature";
const char* mqtt_humidity_TopicName = "bathroom/humidity";

bool risingEdge = false;
bool fallingEdge = false;
bool started = false;
int intTemp = 0;
int extTemp = 0;
int lastTemp = 0;
int setpoint = 15;
int hysteresisUp = 0;
int hysteresisDown = 1;
const int setpointLow = 10;
const int setpointHigh = 25;

String modeString = "off";
String setpointString;
String setpointLowString;
String setpointHighString;

const int initial_state = LOW;
const int delay_check = 15000;

char* error = "0";
long lastMsg = 0;
long timeThermMsg = 0;
long now = 0;

/*
   Configurazine DHT11
*/
#define pinDHT11 2 //Corrisponde a D4
SimpleDHT11 dht11(pinDHT11);
char temp[10];
char umid[10];

void setup_wifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.config(staticIP, gateway, subnet);
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
  char buffer[length];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String topicStr(topic);
  String searchString;

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msgpayload[i] = (char)payload[i];
  }
  Serial.println();

  /*
     Faccio un confronto tra i messaggi ricevuti e i topic a cui sono iscritto
  */

  /*
     Requested relay status
     The number I receive decide what to do:
      0 - turn of
      1 - turn off
      2 - switch
  */
  
  if (strcmp(topic, mqtt_relay_requestedState_topicName) == 0) {
    //Per usare il pulsante da NodeRED eseguo uno switch dell'uscita inviando il carattere '2'
    if ((char)payload[0] == '2') {
      if (digitalRead(pinRelay)) {
        //Altrimenti lo disattivo
        digitalWrite(pinRelay, LOW);
        Serial.println("Uscita disattivata da NodeRED");
        client.publish(mqtt_relay_status_topicName, "off");
      } else {
        digitalWrite(pinRelay, HIGH);
        Serial.println("Uscita attivata da NodeRED");
        client.publish(mqtt_relay_status_topicName, "on");
      }
      Serial.println("Output switched");
    }
    
    //In alternativa accetto anche 1 e 0 per gestire l'uscita
    if ((char)payload[0] == '1') {
      digitalWrite(pinRelay, HIGH);
      Serial.println("Uscita attivata");
      client.publish(mqtt_relay_status_topicName, "on");
      Serial.println("Output turned on");
    } else if ((char)payload[0] == '0') {
      digitalWrite(pinRelay, LOW);
      Serial.println("Uscita disattivata");
      client.publish(mqtt_relay_status_topicName, "off");
      Serial.println("Output turned off");
    }
    client.publish(mqtt_status_topicName, "Changed relay state");
  }

  /*
     Temperature setpoint
  */
  if (strcmp(topic, mqtt_thermostat_setpoint_topicName) == 0) {
    setpoint = atoi(msgpayload);

    client.publish(mqtt_status_topicName, "Impostato nuovo setpoint");
  }
  
  /*
     Thermostat mode
     Based on the value start or stop working
     0 - thermostat in stand-by
     1 - thermostat working
  */
  
  if (strcmp(topic, mqtt_thermostat_mode_topicName) == 0) {
    modeString = msgpayload;
    Serial.print("Modalità operativa: "); Serial.println(modeString);
  }
  
  /*
   * Temperatura di riferimento esterna
   * Posso ricevere una temperatura esterna in modo da poter regolare su un altra sensore della casa, così posso tenere il WeMos nella caldaia in bagno 
   * e regolare sulla temperatura in soggiorno.
   */
  if (strcmp(topic, mqtt_thermostat_extTemp_topicName) == 0) {
    extTemp = atoi(msgpayload);
    client.publish(mqtt_status_topicName, "Temperatura acquisita");
  }
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
      client.subscribe(mqtt_relay_requestedState_topicName);
      client.subscribe(mqtt_thermostat_setpoint_topicName);
      //client.subscribe(mqtt_thermostat_setpointLow_topicName);
      //client.subscribe(mqtt_thermostat_setpointHigh_topicName);
      client.subscribe(mqtt_thermostat_mode_topicName);
      client.subscribe(mqtt_thermostat_extTemp_topicName);
      client.publish(mqtt_status_topicName, "Connected to MQTT Server");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void thermostat() {
  int validTemp = 0;
  char buffer[10];
  modeString.toCharArray(buffer, 10);
  /*
     La temperatura usata dal termostato deve essere valida e recente, altrimenti utilizza quella interna
  */
  if (extTemp > 0) {
    validTemp = extTemp;
  }
  else {
    validTemp = intTemp;
    error = "1";
    client.publish(mqtt_relay_status_topicName, "Invalid supplied temperature");
  }
  /*
     Detect direction edge
  */
  if (validTemp < lastTemp) {
    fallingEdge = true;
    risingEdge = false;
  }
  else if (validTemp > lastTemp) {
    fallingEdge = false;
    risingEdge = true;
  }
  else {
    fallingEdge = false;
    risingEdge = false;
  }
  /*
     Thermostat function
  */
  if (strcmp(buffer, "heat") == 0) {
    client.publish(mqtt_thermostat_status_topicName, "heat");

    if ((validTemp < (setpoint - hysteresisDown) && started) || ((validTemp < setpoint) && !started)) {
      digitalWrite(pinRelay, HIGH);
      Serial.println("Uscita attivata da funzione termostato");
      client.publish(mqtt_status_topicName, "Thermostat is heating");
    }
    else if ((validTemp >= (setpoint + hysteresisUp) && started) || ((validTemp >= setpoint) && !started)) {
      digitalWrite(pinRelay, LOW);
      Serial.println("Uscita disattivata da funzione termostato");
      client.publish(mqtt_status_topicName, "Thermostat is turned off");
    }

    Serial.print("Temp. precedente:"); Serial.println(lastTemp);
    Serial.print("Temp. validata:"); Serial.println(validTemp);
    Serial.print("Setpoint:"); Serial.println(setpoint);
    lastTemp = validTemp;
    started = true;
    Serial.println("Thermostat is working");
    client.publish(mqtt_status_topicName, "Thermostat is working");
  }
  else if (strcmp(buffer, "off") == 0) {
    /*
       If it's just turn off deactivate the relay
    */
    if (started) {
      digitalWrite(pinRelay, LOW);
    }
    started = false;
    Serial.println("Thermostat in standby");
    client.publish(mqtt_status_topicName, "Thermostat in stadby");
    client.publish(mqtt_thermostat_status_topicName, "off");
  }
  return;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(pinRelay, OUTPUT);
  digitalWrite(pinRelay, initial_state);
}

void loop() {
  error = "0";
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  /*
     Ciclicamente pubblico lo stato dei vari topic ad intervalli impostati dalla variabile "delay_check"
  */
  now = millis();

  if (now - timeThermMsg > 30000) {
    thermostat();
    timeThermMsg = now;
  }

  if (now - lastMsg > delay_check) {
    lastMsg = now;

    //PUBLICAZIONE STATO RELÉ
    if (digitalRead(pinRelay)) {
      client.publish(mqtt_relay_status_topicName, "on");
    } else {
      client.publish(mqtt_relay_status_topicName, "off");
    }

    /*
       TEMPERATURA E UMIDITÀ
       Acquisisco i valori dal DHT11
       Pubblico le misure su MQTT
    */
    byte temperature = 0;
    byte humidity = 0;
    int temp;
    int umid;
    char buffer[10];
    int err = SimpleDHTErrSuccess;
    if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
      Serial.print("Read DHT11 failed, err=");
      Serial.println(err);
      error = "1";
      client.publish("mqtt_error_topicName", error);
      Serial.println("Published error");
    }
    else {
      String tempString = String((int)temperature);
      intTemp = (int)temperature;
      String umidString = String((int)humidity);
      dtostrf(temperature, 0, 0, buffer);
      client.publish(mqtt_temperature_TopicName, buffer);
      dtostrf(humidity, 0, 0, buffer);
      client.publish(mqtt_humidity_TopicName, buffer);
    }
    Serial.print((int)temperature); Serial.print(" *C, ");
    Serial.print((int)humidity); Serial.println(" H");
    /*
       Setpoints
    */
    setpointString = String(setpoint);
    setpointString.toCharArray(buffer, 3);
    client.publish(mqtt_thermostat_setpointSet_topicName, buffer);

    setpointHighString = String(setpointHigh);
    setpointHighString.toCharArray(buffer, 3);
    client.publish(mqtt_thermostat_setpointHighSet_topicName, buffer);

    setpointLowString = String(setpointLow);
    setpointLowString.toCharArray(buffer, 3);
    client.publish(mqtt_thermostat_setpointLowSet_topicName, buffer);

    /*
       ANDAMENTO TEMPERATURA
       Pubblico su un topic l'andamento delle temperatura
    */
    if (risingEdge && !fallingEdge) {
      client.publish(mqtt_thermostat_edgeDetection_topicName, "Rising");
    } else if (!risingEdge && fallingEdge) {
      client.publish(mqtt_thermostat_edgeDetection_topicName, "Falling");
    } else if (!risingEdge && !fallingEdge) {
      client.publish(mqtt_thermostat_edgeDetection_topicName, "Stable");
    }

    //Pubblico lo stato di errore per primo e lascio un ritardo per permettere a logiche esterne di registrare l'evento
    client.publish(mqtt_relay_error_topicName, error);
    client.publish(mqtt_status_topicName, "Published all status");
  }
}
