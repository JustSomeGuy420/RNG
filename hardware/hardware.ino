// LIBRARY IMPORTS
#include <rom/rtc.h>
#include <WiFi.h>
#include <PubSubClient.h>

#ifndef STDLIB_H
#include <stdlib.h>
#endif

#ifndef STDIO_H
#include <stdio.h>
#endif

#ifndef ARDUINO_H
#include <Arduino.h>
#endif 
 
#ifndef ARDUINOJSON_H
#include <ArduinoJson.h>
#endif

// DEFINE VARIABLES
#define ARDUINOJSON_USE_DOUBLE      1 
// DEFINE THE PINS THAT WILL BE MAPPED TO THE 7 SEG DISPLAY BELOW, 'a' to 'g'
#define a     15
#define b     32
#define c     33
#define d     25
#define e     26
#define f     27
#define g     14
#define dp    12
/* Complete all others */

// DEFINE VARIABLES FOR TWO LEDs AND TWO BUTTONs. LED_A, LED_B, BTN_A , BTN_B
#define LED_A 4
#define LED_B 0
#define BTN_A 2
/* Complete all others */

// WiFi
const char *ssid = "Pixel_3847"; // Enter your WiFi name
const char *password = "12345678";  // Enter WiFi password

// MQTT Broker
const char *mqtt_server = "broker.emqx.io";
static const char* pubtopic       = "620162206";                    // Add your ID number here
static const char* subtopic[]     = {"620162206_sub","/elet2415"};  // Array of Topics(Strings) to subscribe to
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

// TASK HANDLES 
TaskHandle_t xMQTT_Connect          = NULL; 
TaskHandle_t xNTPHandle             = NULL;  
TaskHandle_t xLOOPHandle            = NULL;  
TaskHandle_t xUpdateHandle          = NULL;
TaskHandle_t xButtonCheckeHandle    = NULL; 

// FUNCTION DECLARATION   
void checkHEAP(const char* Name);   // RETURN REMAINING HEAP SIZE FOR A TASK
void initMQTT(void);                // CONFIG AND INITIALIZE MQTT PROTOCOL
unsigned long getTimeStamp(void);   // GET 10 DIGIT TIMESTAMP FOR CURRENT TIME
void callback(char* topic, byte* payload, unsigned int length);
void initialize(void);
bool publish(const char *topic, const char *payload); // PUBLISH MQTT MESSAGE(PAYLOAD) TO A TOPIC
void vButtonCheck( void * pvParameters );
void vUpdate( void * pvParameters ); 
void GDP(void);   // GENERATE DISPLAY PUBLISH

/* Declare your functions below */
void Display(unsigned char num);
int8_t getLEDStatus(int8_t LED);
void setLEDState(int8_t LED, int8_t state);
void toggleLED(int8_t LED);

//############### IMPORT HEADER FILES ##################
#ifndef NTP_H
#include "NTP.h"
#endif

#ifndef MQTT_H
#include "mqtt.h"
#endif

// Temporary Variables
uint8_t number = 0;

void setup() {
  // put your setup code here, to run once:
  // Set software serial baud to 115200;
  Serial.begin(115200);

  // GENERATE RANDOM SEED
  randomSeed(analogRead(36));

  // CONFIGURE THE ARDUINO PINS OF THE 7SEG AS OUTPUT
  pinMode(a,OUTPUT);
  pinMode(b,OUTPUT);
  pinMode(c,OUTPUT);
  pinMode(d,OUTPUT);
  pinMode(e,OUTPUT);
  pinMode(f,OUTPUT);
  pinMode(g,OUTPUT);
  pinMode(dp,OUTPUT);
  pinMode(LED_A,OUTPUT);
  pinMode(LED_B,OUTPUT);
  pinMode(BTN_A,INPUT_PULLUP);
  /* Configure all others here */

  // Test 7 Segment Display
  Display(8);


  // Connecting to a Wi-Fi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the Wi-Fi network");
  //connecting to a mqtt broker
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);
  while (!mqtt.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (mqtt.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public EMQX MQTT broker connected");

      const uint8_t size = sizeof(subtopic)/sizeof(subtopic[0]);
        for(int x = 0; x< size ; x++){
          mqtt.subscribe(subtopic[x]);
        }
    } else {
      Serial.print("failed with state ");
      Serial.print(mqtt.state());
      delay(2000);
    }
  }

  initialize();           // INIT NTP
  vButtonCheckFunction(); // UNCOMMENT IF USING BUTTONS THEN ADD LOGIC FOR INTERFACING WITH BUTTONS IN THE vButtonCheck FUNCTION
}

void loop() {
  // put your main code here, to run repeatedly:
  mqtt.loop();
}

/*void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}*/

//####################################################################
//#                          UTIL FUNCTIONS                          #       
//####################################################################
void vButtonCheck( void * pvParameters )  {
  configASSERT( ( ( uint32_t ) pvParameters ) == 1 );     
    
  for( ;; ) {
    // Add code here to check if a button(S) is pressed
    if(digitalRead(BTN_A) == LOW){
      // then execute appropriate function if a button is pressed
      GDP();
    }  

    vTaskDelay(200 / portTICK_PERIOD_MS);  
  }
}

void vUpdate( void * pvParameters )  {
  configASSERT( ( ( uint32_t ) pvParameters ) == 1 );    
          
  for( ;; ) {
    // Task code goes here.   
    // PUBLISH to topic every second.
    JsonDocument doc; // Create JSon object
    char message[1100]  = {0};

    // Add key:value pairs to JSon object
    doc["id"]         = "6200162206";
    doc["timestamp"]  = getTimeStamp();
    doc["number"]     = number;
    doc["ledA"]       = getLEDStatus(LED_A);
    doc["ledB"]       = getLEDStatus(LED_B);

    serializeJson(doc, message);  // Seralize / Covert JSon object to JSon string and store in char* array

    if(mqtt.connected() ){
      publish(pubtopic, message);
    }
    
      
  vTaskDelay(1000 / portTICK_PERIOD_MS);  
  }
}

unsigned long getTimeStamp(void) {
  // RETURNS 10 DIGIT TIMESTAMP REPRESENTING CURRENT TIME
  time_t now;         
  time(&now); // Retrieve time[Timestamp] from system and save to &now variable
  return now;
}

void callback(char* topic, byte* payload, unsigned int length) {
  // ############## MQTT CALLBACK  ######################################
  // RUNS WHENEVER A MESSAGE IS RECEIVED ON A TOPIC SUBSCRIBED TO
  
  Serial.printf("\nMessage received : ( topic: %s ) \n",topic ); 
  char *received = new char[length + 1] {0}; 
  
  for (int i = 0; i < length; i++) { 
    received[i] = (char)payload[i];    
  }

  // PRINT RECEIVED MESSAGE
  Serial.printf("Payload : %s \n",received);

 
  // CONVERT MESSAGE TO JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, received);  

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }


  // PROCESS MESSAGE
  const char* type = doc["type"];

  if (strcmp(type, "toggle") == 0){
    // Process messages with ‘{"type": "toggle", "device": "LED A"}’ Schema
    const char* led = doc["device"];

    if(strcmp(led, "LED A") == 0){
      /*Add code to toggle LED A with appropriate function*/
      toggleLED(LED_A);
    }
    if(strcmp(led, "LED B") == 0){
      /*Add code to toggle LED B with appropriate function*/
      toggleLED(LED_B);
    }

    // PUBLISH UPDATE BACK TO FRONTEND
    JsonDocument doc; // Create JSon object
    char message[800]  = {0};

    // Add key:value pairs to Json object according to below schema
    // ‘{"id": "student_id", "timestamp": 1702212234, "number": 9, "ledA": 0, "ledB": 0}’
    doc["id"]         = "620162206"; // Change to your student ID number
    doc["timestamp"]  = getTimeStamp();
    doc["number"]     = number;
    doc["ledA"]       = getLEDStatus(LED_A);
    doc["ledB"]       = getLEDStatus(LED_B);
    /*Add code here to insert all other variabes that are missing from Json object
    according to schema above
    */

    serializeJson(doc, message);  // Seralize / Covert JSon object to JSon string and store in char* array  
    publish(pubtopic, message);    // Publish to a topic that only the Frontend subscribes to.
          
  } 

}

bool publish(const char *topic, const char *payload){   
  bool res = false;
  try{
    res = mqtt.publish(topic,payload);
    // Serial.printf("\nres : %d\n",res);
    if(!res){
      res = false;
      throw false;
    }
  }
  catch(...){
  Serial.printf("\nError (%d) >> Unable to publish message\n", res);
  }
  return res;
}

//***** Complete the util functions below ******

void Display(unsigned char num){
  /* This function takes an integer between 0 and 9 as input. This integer must be written to the 7-Segment display */
  number = num;
  digitalWrite(a,LOW);
  digitalWrite(b,LOW);
  digitalWrite(c,LOW);
  digitalWrite(d,LOW);
  digitalWrite(e,LOW);
  digitalWrite(f,LOW);
  digitalWrite(g,LOW);

  delay(500);
  
  if(number == 0){
    digitalWrite(a,HIGH);
    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(d,HIGH);  
    digitalWrite(e,HIGH);
    digitalWrite(f,HIGH);
  }else if(number == 1){
    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
  }else if(number == 2){
    digitalWrite(a,HIGH);
    digitalWrite(b,HIGH);
    digitalWrite(d,HIGH);
    digitalWrite(e,HIGH);
    digitalWrite(g,HIGH);
  }else if(number == 3){
    digitalWrite(a,HIGH);
    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(d,HIGH);
    digitalWrite(g,HIGH);
  }else if(number == 4){

    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(f,HIGH);
    digitalWrite(g,HIGH);
  }else if(number == 5){
    digitalWrite(a,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(d,HIGH);
    digitalWrite(f,HIGH);
    digitalWrite(g,HIGH);
  }else if(number == 6){
    digitalWrite(a,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(d,HIGH);
    digitalWrite(e,HIGH);
    digitalWrite(f,HIGH);
    digitalWrite(g,HIGH);
  }else if(number == 7){
    digitalWrite(a,HIGH);
    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
  }else if(number == 8){
    digitalWrite(a,HIGH);
    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(d,HIGH);
    digitalWrite(e,HIGH);
    digitalWrite(f,HIGH);
    digitalWrite(g,HIGH);
  }else if(number == 9){
    digitalWrite(a,HIGH);
    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(d,HIGH);
    digitalWrite(f,HIGH);
    digitalWrite(g,HIGH);
  }else{
    digitalWrite(a,HIGH);
    digitalWrite(b,HIGH);
    digitalWrite(c,HIGH);
    digitalWrite(d,HIGH);
    digitalWrite(e,HIGH);
    digitalWrite(f,HIGH);
    digitalWrite(g,HIGH);
  }

  
}

int8_t getLEDStatus(int8_t LED) {
  // RETURNS THE STATE OF A SPECIFIC LED. 0 = LOW, 1 = HIGH
  if (digitalRead(LED) == LOW){
    return 0;
  }else{
    return 1;
  }
}

void setLEDState(int8_t LED, int8_t state){
  // SETS THE STATE OF A SPECIFIC LED
  digitalWrite(LED, state);
}

void toggleLED(int8_t LED){
  // TOGGLES THE STATE OF SPECIFIC LED
  if(getLEDStatus(LED) == LOW){
    setLEDState(LED, HIGH);
  }else{
    setLEDState(LED, LOW);
  }
}

void GDP(void){
  // GENERATE, DISPLAY THEN PUBLISH INTEGER

  // GENERATE a random integer 
  /* Add code here to generate a random integer and then assign 
     this integer to number variable below
  */

  // DISPLAY integer on 7Seg. by 
  /* Add code here to calling appropriate function that will display integer to 7-Seg*/
  Display(random(10));

  // PUBLISH number to topic.
  JsonDocument doc; // Create JSon object
  char message[1100]  = {0};

  // Add key:value pairs to Json object according to below schema
  // ‘{"id": "student_id", "timestamp": 1702212234, "number": 9, "ledA": 0, "ledB": 0}’
  doc["id"]         = "620162206"; // Change to your student ID number
  doc["timestamp"]  = getTimeStamp();
  doc["number"]     = number;
  doc["ledA"]       = getLEDStatus(LED_A);
  doc["ledB"]       = getLEDStatus(LED_B);
  /*Add code here to insert all other variabes that are missing from Json object
  according to schema above
  */

  serializeJson(doc, message);  // Seralize / Covert JSon object to JSon string and store in char* array
  publish(pubtopic, message);

}

