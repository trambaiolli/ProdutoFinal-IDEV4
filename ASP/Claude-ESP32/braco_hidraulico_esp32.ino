/*
  BRAÇO HIDRÁULICO CONTROLADO POR TABLET - versão ESP32
  ESP32 DevKit (ESP32-WROOM-32) + 2 drivers L298N + 3 motores CC com redução.
  O Bluetooth BLE é o do próprio ESP32 (não precisa de módulo externo).

  Cada motor move o êmbolo de uma seringa de controle (fuso + suporte móvel).
  A água leva o movimento até a seringa atuadora da articulação.

  LIGAÇÕES (GPIO do ESP32):
    Motor 1 (ex.: ombro)   -> Driver A, canal A : EN=25(PWM)  IN1=26  IN2=27
    Motor 2 (ex.: cotovelo)-> Driver A, canal B : EN=32(PWM)  IN1=33  IN2=16
    Motor 3 (ex.: garra)   -> Driver B, canal A : EN=17(PWM)  IN1=18  IN2=19
    Fins de curso (um lado no pino, outro no GND, contato NA):
        Motor 1: recuado=4   avançado=13
        Motor 2: recuado=14  avançado=21
        Motor 3: recuado=22  avançado=23
    GND comum entre ESP32, drivers e fonte dos motores.
    Os pinos 0, 2, 5, 12 e 15 (de boot) e 34-39 (só entrada) NÃO são usados de propósito.

  BLE (Nordic UART Service):
    Nome:      "Braco"
    Serviço:   6E400001-B5A3-F393-E0A9-E50E24DCCA9E
    RX (tablet -> ESP32, escrita):      6E400002-...
    TX (ESP32 -> tablet, notificação):  6E400003-...

  PROTOCOLO (texto, cada comando termina com \n) - o mesmo que o app web usa:
    M1:F      motor 1 avança            M1:R   motor 1 recua        M1:S  motor 1 para
    M1:V200   velocidade do motor 1 (PWM 80..255)
    X         PARA TUDO                 H      "estou aqui" (heartbeat)      ?  pede status
  Respostas:
    ST:<motor>:<dir>:<pwm>:<fcRecuado>:<fcAvancado>   (dir: 1 avança, -1 recua, 0 parado)
    LIM:<motor>:MIN|MAX    parou por fim de curso
    WD                     parou por perda de comunicação
    TMAX:<motor>           parou por tempo máximo
    OK:X | PRONTO | ERRO

  Teste sem o tablet: Monitor Serial a 115200 baud, "Nova linha", e digite M1:F, M1:S, X...
  (comandos vindos do USB não ativam o watchdog de comunicação; o limite de tempo e os
   fins de curso continuam valendo)

  Placa no Arduino IDE: "ESP32 Dev Module" (pacote "esp32 by Espressif Systems").
  Se der "Sketch too big", use Tools > Partition Scheme > "Huge APP".
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#if __has_include(<BLE2902.h>)
  #include <BLE2902.h>
  #define TEM_BLE2902 1
#endif

// ---------------------------------------------------------------- CONFIGURAÇÃO
#define UUID_SERVICO "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UUID_RX      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UUID_TX      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
const char *NOME_BLE = "Braco";

const uint8_t NUM_MOTORES = 3;

const uint8_t PIN_EN[NUM_MOTORES]  = {25, 32, 17};   // PWM (ENA/ENB do driver)
const uint8_t PIN_IN1[NUM_MOTORES] = {26, 33, 18};
const uint8_t PIN_IN2[NUM_MOTORES] = {27, 16, 19};

const uint8_t PIN_FC_RECUADO[NUM_MOTORES]  = {4, 14, 22};   // fim de curso "recuar"
const uint8_t PIN_FC_AVANCADO[NUM_MOTORES] = {13, 21, 23};  // fim de curso "avançar"

const uint32_t PWM_FREQ = 5000;   // Hz
const uint8_t  PWM_BITS = 8;      // 0..255

const unsigned long TIMEOUT_COM_MS = 1500;   // sem sinal do app por esse tempo => para tudo
const unsigned long TEMPO_MAX_MS   = 15000;  // tempo máximo contínuo de um motor ligado
const uint8_t  PWM_MIN    = 80;              // abaixo disso o motor costuma não girar
const uint8_t  PWM_PADRAO = 200;

// Se um motor andar ao contrário do esperado, troque o valor para true:
const bool INVERTER[NUM_MOTORES] = {false, false, false};

// PWM: o núcleo ESP32 3.x e o 2.x têm APIs diferentes
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define PWM_INICIAR(i)      ledcAttach(PIN_EN[i], PWM_FREQ, PWM_BITS)
  #define PWM_ESCREVER(i, v)  ledcWrite(PIN_EN[i], (v))
#else
  #define PWM_INICIAR(i)      do { ledcSetup((i), PWM_FREQ, PWM_BITS); ledcAttachPin(PIN_EN[i], (i)); } while (0)
  #define PWM_ESCREVER(i, v)  ledcWrite((i), (v))
#endif

// ---------------------------------------------------------------- ESTADO
int8_t        direcao[NUM_MOTORES]     = {0, 0, 0};
uint8_t       pwmMotor[NUM_MOTORES]    = {PWM_PADRAO, PWM_PADRAO, PWM_PADRAO};
unsigned long inicioMotor[NUM_MOTORES] = {0, 0, 0};

BLEServer         *servidor = nullptr;
BLECharacteristic *carTx    = nullptr;

volatile bool          bleConectado   = false;
volatile bool          bleDesconectou = false;
volatile unsigned long ultimoSinalBle = 0;
bool                   origemBle      = false;   // último motor foi ligado por BLE? (watchdog só vale para BLE)

// fila de bytes recebidos por BLE (o callback só enfileira; quem processa é o loop)
const uint16_t RX_TAM = 256;
volatile char     rxFila[RX_TAM];
volatile uint16_t rxIni = 0, rxFim = 0;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

const uint8_t LINHA_TAM = 24;
char    linhaBle[LINHA_TAM];
uint8_t nBle = 0;
char    linhaUsb[LINHA_TAM];
uint8_t nUsb = 0;

// ---------------------------------------------------------------- BLE
class CbServidor : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    bleConectado = true;
    ultimoSinalBle = millis();
  }
  void onDisconnect(BLEServer *) override {
    bleConectado = false;
    bleDesconectou = true;
  }
};

class CbRx : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    auto v = c->getValue();            // std::string (core 2.x) ou String (core 3.x)
    size_t n = v.length();
    const char *p = v.c_str();
    portENTER_CRITICAL(&rxMux);
    for (size_t i = 0; i < n; i++) {
      uint16_t prox = (rxFim + 1) % RX_TAM;
      if (prox == rxIni) break;        // fila cheia: descarta o resto
      rxFila[rxFim] = p[i];
      rxFim = prox;
    }
    portEXIT_CRITICAL(&rxMux);
    ultimoSinalBle = millis();
  }
};

bool rxPop(char &c) {
  bool ok = false;
  portENTER_CRITICAL(&rxMux);
  if (rxIni != rxFim) {
    c = rxFila[rxIni];
    rxIni = (rxIni + 1) % RX_TAM;
    ok = true;
  }
  portEXIT_CRITICAL(&rxMux);
  return ok;
}

// ---------------------------------------------------------------- UTILITÁRIOS
void enviar(const char *texto) {
  Serial.println(texto);
  if (bleConectado && carTx) {
    char buf[40];
    int n = snprintf(buf, sizeof(buf), "%s\n", texto);
    carTx->setValue((uint8_t *)buf, n);
    carTx->notify();
    delay(8);                          // dá tempo da pilha BLE enviar antes da próxima notificação
  }
}

bool fcRecuado(uint8_t i)  { return digitalRead(PIN_FC_RECUADO[i])  == LOW; }
bool fcAvancado(uint8_t i) { return digitalRead(PIN_FC_AVANCADO[i]) == LOW; }

void enviarStatus(uint8_t i) {
  char buf[32];
  snprintf(buf, sizeof(buf), "ST:%u:%d:%u:%u:%u", i + 1, direcao[i], pwmMotor[i],
           fcRecuado(i) ? 1 : 0, fcAvancado(i) ? 1 : 0);
  enviar(buf);
}

void enviarLimite(uint8_t i, bool max) {
  char b[16];
  snprintf(b, sizeof(b), "LIM:%u:%s", i + 1, max ? "MAX" : "MIN");
  enviar(b);
}

// ---------------------------------------------------------------- MOTORES
void aplicar(uint8_t i) {
  int8_t d = direcao[i];
  if (INVERTER[i]) d = -d;

  if (d > 0) {
    digitalWrite(PIN_IN1[i], HIGH);
    digitalWrite(PIN_IN2[i], LOW);
    PWM_ESCREVER(i, pwmMotor[i]);
  } else if (d < 0) {
    digitalWrite(PIN_IN1[i], LOW);
    digitalWrite(PIN_IN2[i], HIGH);
    PWM_ESCREVER(i, pwmMotor[i]);
  } else {
    PWM_ESCREVER(i, 0);
    digitalWrite(PIN_IN1[i], LOW);
    digitalWrite(PIN_IN2[i], LOW);
  }
}

void pararMotor(uint8_t i, bool avisar = true) {
  direcao[i] = 0;
  aplicar(i);
  if (avisar) enviarStatus(i);
}

void pararTodos() {
  for (uint8_t i = 0; i < NUM_MOTORES; i++) pararMotor(i, false);
}

void iniciarMotor(uint8_t i, int8_t d, bool viaBle) {
  // não anda contra o fim de curso
  if (d > 0 && fcAvancado(i)) { enviarLimite(i, true);  pararMotor(i); return; }
  if (d < 0 && fcRecuado(i))  { enviarLimite(i, false); pararMotor(i); return; }

  // inversão de sentido: para um instante antes (protege driver e motor)
  if (direcao[i] != 0 && direcao[i] != d) {
    pararMotor(i, false);
    delay(80);
  }
  origemBle = viaBle;
  direcao[i] = d;
  inicioMotor[i] = millis();
  aplicar(i);
  enviarStatus(i);
}

// ---------------------------------------------------------------- COMANDOS
void processar(char *s, bool viaBle) {
  if (s[0] == 'H') return;                       // heartbeat: só atualiza ultimoSinalBle

  if (s[0] == 'X') {                             // parada geral
    pararTodos();
    enviar("OK:X");
    for (uint8_t i = 0; i < NUM_MOTORES; i++) enviarStatus(i);
    return;
  }

  if (s[0] == '?') {
    for (uint8_t i = 0; i < NUM_MOTORES; i++) enviarStatus(i);
    return;
  }

  if (s[0] == 'M' && s[1] >= '1' && s[1] <= '0' + NUM_MOTORES && s[2] == ':') {
    uint8_t i = s[1] - '1';
    char c = s[3];
    if (c == 'F')      iniciarMotor(i, +1, viaBle);
    else if (c == 'R') iniciarMotor(i, -1, viaBle);
    else if (c == 'S') pararMotor(i);
    else if (c == 'V') {
      int v = constrain(atoi(s + 4), PWM_MIN, 255);
      pwmMotor[i] = (uint8_t)v;
      if (direcao[i] != 0) aplicar(i);
      enviarStatus(i);
    } else enviar("ERRO");
    return;
  }

  enviar("ERRO");
}

void alimentar(char c, char *buf, uint8_t &n, bool viaBle) {
  if (c == '\n' || c == '\r') {
    if (n > 0) {
      buf[n] = 0;
      processar(buf, viaBle);
      n = 0;
    }
  } else if (n < LINHA_TAM - 1) {
    buf[n++] = c;
  } else {
    n = 0;   // linha longa demais: descarta
  }
}

// ---------------------------------------------------------------- SEGURANÇA
void verificarSeguranca() {
  unsigned long agora = millis();
  bool algumLigado = false;

  for (uint8_t i = 0; i < NUM_MOTORES; i++) {
    if (direcao[i] == 0) continue;
    algumLigado = true;

    if (direcao[i] > 0 && fcAvancado(i)) {
      pararMotor(i);
      enviarLimite(i, true);
    } else if (direcao[i] < 0 && fcRecuado(i)) {
      pararMotor(i);
      enviarLimite(i, false);
    } else if (agora - inicioMotor[i] > TEMPO_MAX_MS) {
      pararMotor(i);
      char b[16]; snprintf(b, sizeof(b), "TMAX:%u", i + 1); enviar(b);
    }
  }

  // perdeu a comunicação com o tablet => para tudo
  if (algumLigado && origemBle && (agora - ultimoSinalBle > TIMEOUT_COM_MS)) {
    pararTodos();
    enviar("WD");
  }
}

// ---------------------------------------------------------------- SETUP / LOOP
void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < NUM_MOTORES; i++) {
    pinMode(PIN_IN1[i], OUTPUT);
    pinMode(PIN_IN2[i], OUTPUT);
    digitalWrite(PIN_IN1[i], LOW);
    digitalWrite(PIN_IN2[i], LOW);
    PWM_INICIAR(i);
    PWM_ESCREVER(i, 0);
    pinMode(PIN_FC_RECUADO[i], INPUT_PULLUP);
    pinMode(PIN_FC_AVANCADO[i], INPUT_PULLUP);
  }
  pararTodos();

  BLEDevice::init(NOME_BLE);
  servidor = BLEDevice::createServer();
  servidor->setCallbacks(new CbServidor());

  BLEService *servico = servidor->createService(UUID_SERVICO);

  BLECharacteristic *carRx = servico->createCharacteristic(
      UUID_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  carRx->setCallbacks(new CbRx());

  carTx = servico->createCharacteristic(UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
#ifdef TEM_BLE2902
  carTx->addDescriptor(new BLE2902());
#endif

  servico->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(UUID_SERVICO);
  adv->setScanResponse(true);
  adv->start();

  Serial.println("PRONTO (BLE anunciando como \"Braco\")");
}

void loop() {
  // desconectou: para tudo e volta a anunciar
  if (bleDesconectou) {
    bleDesconectou = false;
    pararTodos();
    Serial.println("BLE desconectado");
    delay(300);
    servidor->getAdvertising()->start();
  }

  // comandos do BLE
  char c;
  while (rxPop(c)) alimentar(c, linhaBle, nBle, true);

  // comandos do USB (teste pelo Monitor Serial)
  while (Serial.available()) alimentar((char)Serial.read(), linhaUsb, nUsb, false);

  verificarSeguranca();
}
