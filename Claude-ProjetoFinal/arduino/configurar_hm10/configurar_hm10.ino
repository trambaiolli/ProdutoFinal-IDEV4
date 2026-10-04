/* Carregue temporariamente com VM (fonte do motor) desligado.
   Monitor Serial: 9600, Nenhum final de linha.
   Envie AT; a ponte repassa literalmente, sem adicionar CR/LF.
   Depois recarregue braco_hidraulico.ino.
*/
#include <SoftwareSerial.h>
const unsigned long BLE_BAUD = 9600; // Troque SOMENTE se o módulo usar outro baud.
SoftwareSerial ble(10, 11);
void setup() {
  // Motor fica desabilitado também neste sketch de configuração.
  pinMode(13, OUTPUT); digitalWrite(13, LOW);
  Serial.begin(9600); ble.begin(BLE_BAUD);
  Serial.println(F("Ponte AT pronta. Bluetooth desconectado; sem final de linha."));
}
void loop() {
  while (Serial.available()) ble.write(Serial.read());
  while (ble.available()) Serial.write(ble.read());
}
