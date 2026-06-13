/*
  **** ESP Now code from: ****
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-esp32-arduino-ide/  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

int pot1;
int pot2;
int pot3;
//pin set up
int buttonPin = 5;
#define BUTTON_PIN D2 // D2 pin connected to button

int prev_state = HIGH; // the previous state from the input pin
int button_state;     // the current reading from the input pin

#include <esp_now.h>
#include <WiFi.h>

// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x20, 0x6E, 0xF1, 0x31, 0x66, 0xD4};    // example 20:6E:F1:31:66:D4 (currently correct for esp#2 to send to #1)

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  int a;    // pot1
  int b;    // pot2
  //adding new ports
  int c; //pot3
  int d; //pot4

} struct_message;

// Create a struct_message called myData
struct_message myData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
 
//setup pins
//pinMode(buttonPin, INPUT); old
//digitalWrite(buttonPin, HIGH); old
pinMode(BUTTON_PIN, INPUT_PULLUP);


  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}
 
void loop() {

        // read pots
        pot1 = analogRead(A6);      // pot pin
        pot2 = analogRead(A7);      // pot pin
        pot3 = 0;
//        pot3 = digitalRead(buttonPin);
//      Serial.println(digitalRead(buttonPin));
// new button code
  button_state = digitalRead(BUTTON_PIN);

  if(prev_state == LOW && button_state == HIGH)
  pot3 = 1;
 //   Serial.println("The state changed from LOW to HIGH");

  // save the last state
  prev_state = button_state;


        // Set values to send
        myData.a = pot1;
        myData.b = pot2;
        myData.c = pot3;
       // myData.d = D2;
        // Send message via ESP-NOW
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
       Serial.print(myData.a);
       Serial.print(" ");
       Serial.print(myData.b);
       Serial.print(" ");
       Serial.print(myData.c);
       Serial.print(" ");
       Serial.println(myData.d);
        delay(500); 
      }
      
  
  

