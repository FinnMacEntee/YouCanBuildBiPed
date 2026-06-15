#include "WiFi.h"

void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);
  Serial.println(WiFi.macAddress());
}
 //this is a comment test
void loop(){
  Serial.println(WiFi.macAddress());
  delay(20000);

}

//mac adress for arduino nano esp32 #1: 20:6E:F1:31:66:D4
//mac adress for arduino nano esp32 #2: 20:6E:F1:30:BC:50