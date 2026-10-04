/* Arduino Uno + HM-10 9600 + DOIS TB6612FNG: três motores.
   M:1,A,89\n / M:2,R,89\n / M:3,S,0\n / S\n (parar todos).
   Renovar cada motor a cada 150 ms; timeout independente de 450 ms.
   Seis limites NC: COM ao GND, NC ao pino. HIGH = acionado/fio aberto.
*/
#include <SoftwareSerial.h>
#include <string.h>
SoftwareSerial ble(10, 11); // RX <- HM TX; TX -> divisor -> HM RX
const byte PIN_STBY = 13; // STBY dos dois drivers, resistor externo 10k para GND
const unsigned long COMMAND_TIMEOUT_MS = 450, REVERSAL_PAUSE_MS = 120;
const unsigned long TELEMETRY_SLOT_MS = 100; // Um motor por slot; ciclo completo 300 ms
const byte MAX_PWM = 178;
struct Axis {
  byte pwmPin, in1Pin, in2Pin, limitAPin, limitRPin;
  bool inverted;
  char target, running, lastDirection, blockedDirection;
  byte pwm;
  bool armed;
  unsigned long lastCommand, stoppedAt;
};
// Motor 1 = driver 1 canal A; motor 2 = driver 1 canal B; motor 3 = driver 2 canal A.
Axis axes[3] = {
  {3, 7, 8, 2, 4, false, 'S','S','S','S', 0, false, 0, 0},
  {5, A2, A3, 9, 12, false, 'S','S','S','S', 0, false, 0, 0},
  {6, A4, A5, A0, A1, false, 'S','S','S','S', 0, false, 0, 0}
};
char line[20]; byte lineLength = 0; bool discardLine = false;
unsigned long lastTelemetry = 0; byte telemetryIndex = 0;
bool limitA(const Axis &a) { return digitalRead(a.limitAPin) == HIGH; }
bool limitR(const Axis &a) { return digitalRead(a.limitRPin) == HIGH; }
void motorOff(Axis &a) {
  analogWrite(a.pwmPin, 0);
  digitalWrite(a.in1Pin, LOW); digitalWrite(a.in2Pin, LOW);
  if (a.running != 'S') a.stoppedAt = millis();
  a.running = 'S';
}
void stopAxis(Axis &a) { a.target = 'S'; a.pwm = 0; motorOff(a); }
void stopAll(bool rearm) {
  for (byte i = 0; i < 3; i++) {
    stopAxis(axes[i]); axes[i].armed = rearm;
    if (rearm) { axes[i].blockedDirection = 'S'; axes[i].lastCommand = millis(); }
  }
  digitalWrite(PIN_STBY, LOW);
}
void parseCommand() {
  line[lineLength] = '\0';
  if (strcmp(line, "S") == 0) { stopAll(true); return; }
  bool valid = lineLength >= 7 && lineLength <= 9 && line[0] == 'M' && line[1] == ':' &&
    line[2] >= '1' && line[2] <= '3' && line[3] == ',' &&
    (line[4] == 'A' || line[4] == 'R' || line[4] == 'S') && line[5] == ',';
  unsigned int pwm = 0;
  if (valid) {
    for (byte i = 6; i < lineLength; i++) {
      if (line[i] < '0' || line[i] > '9') { valid = false; break; }
      pwm = pwm * 10 + (line[i] - '0');
    }
  }
  if (!valid || pwm > MAX_PWM || (line[4] == 'S' && pwm != 0)) { stopAll(false); return; }
  Axis &a = axes[line[2] - '1'];
  if (line[4] == 'S' || pwm == 0) {
    stopAxis(a); a.blockedDirection = 'S'; a.armed = true; a.lastCommand = millis(); return;
  }
  if (!a.armed) return; // Exige S após boot, timeout ou mensagem inválida.
  if (line[4] == a.blockedDirection) { stopAxis(a); return; }
  a.blockedDirection = 'S'; a.lastCommand = millis(); a.target = line[4]; a.pwm = pwm;
}
void readBluetooth() {
  for (byte count = 0; count < 32 && ble.available(); count++) {
    char c = ble.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!discardLine) parseCommand();
      lineLength = 0; discardLine = false;
    } else if (!discardLine) {
      if (lineLength < sizeof(line) - 1) line[lineLength++] = c;
      else { discardLine = true; stopAll(false); }
    }
  }
}
void serviceMotor(Axis &a) {
  unsigned long now = millis();
  if (a.armed && (unsigned long)(now - a.lastCommand) > COMMAND_TIMEOUT_MS) {
    stopAxis(a); a.armed = false;
  }
  if (a.target == 'S' || (a.target == 'A' && limitA(a)) || (a.target == 'R' && limitR(a))) {
    if (a.target != 'S') a.blockedDirection = a.target;
    stopAxis(a); return;
  }
  if (a.running != 'S' && a.target != a.running) motorOff(a);
  if (a.running == 'S' && a.lastDirection != 'S' && a.target != a.lastDirection &&
      (unsigned long)(now - a.stoppedAt) < REVERSAL_PAUSE_MS) return;
  bool forward = (a.target == 'A') != a.inverted;
  digitalWrite(a.in1Pin, forward ? HIGH : LOW);
  digitalWrite(a.in2Pin, forward ? LOW : HIGH);
  analogWrite(a.pwmPin, a.pwm);
  a.running = a.target; a.lastDirection = a.target;
}
void telemetry(byte i) {
  const Axis &a = axes[i];
  ble.print(F("T:")); ble.print(i + 1); ble.print(','); ble.print(a.running); ble.print(',');
  ble.print(limitA(a) ? 1 : 0); ble.print(','); ble.print(limitR(a) ? 1 : 0); ble.print('\n');
}
void setup() {
  pinMode(PIN_STBY, OUTPUT); digitalWrite(PIN_STBY, LOW);
  for (byte i = 0; i < 3; i++) {
    Axis &a = axes[i];
    pinMode(a.pwmPin, OUTPUT); pinMode(a.in1Pin, OUTPUT); pinMode(a.in2Pin, OUTPUT);
    pinMode(a.limitAPin, INPUT_PULLUP); pinMode(a.limitRPin, INPUT_PULLUP);
    motorOff(a);
  }
  ble.begin(9600);
}
void loop() {
  readBluetooth();
  bool anyRunning = false;
  for (byte i = 0; i < 3; i++) { serviceMotor(axes[i]); anyRunning |= axes[i].running != 'S'; }
  digitalWrite(PIN_STBY, anyRunning ? HIGH : LOW);
  // Se todos parados: standby (alta impedância), sem freio mecânico.
  // Se outro motor gira, o canal parado fica com IN1/IN2/PWM baixos: não sustenta carga.
  if ((unsigned long)(millis() - lastTelemetry) >= TELEMETRY_SLOT_MS) {
    lastTelemetry = millis(); telemetry(telemetryIndex); telemetryIndex = (telemetryIndex + 1) % 3;
  }
}
