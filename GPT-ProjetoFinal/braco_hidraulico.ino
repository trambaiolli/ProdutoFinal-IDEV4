/*
  BRAÇO HIDRÁULICO CONTROLADO POR TABLET
  Arduino Uno + módulo Bluetooth BLE (HM-10 / AT-09 / BT-05) + 3 motores CC com redução

  Cada motor move o êmbolo de uma seringa de controle (fuso + suporte móvel).
  A água leva o movimento até a seringa atuadora da articulação.

  LIGAÇÕES (resumo - veja a tabela completa no passo a passo):
    Motor 1 (ex.: ombro)  -> Driver A, canal A : EN=D5(PWM)  IN1=D7  IN2=D8
    Motor 2 (ex.: cotovelo)-> Driver A, canal B : EN=D6(PWM)  IN1=D9  IN2=D10
    Motor 3 (ex.: garra)  -> Driver B, canal A : EN=D3(PWM)  IN1=D11 IN2=D12
    Fins de curso (um lado no pino, outro no GND, contato NA):
        Motor 1: recuado=A0  avançado=A1
        Motor 2: recuado=A2  avançado=A3
        Motor 3: recuado=A4  avançado=A5
    Bluetooth BLE: TX do módulo -> D2 | RX do módulo <- D4 (com divisor 1k/2k, módulo é 3,3 V)
    GND comum entre Arduino, módulo BLE, drivers e fonte dos motores.

  PROTOCOLO (texto, cada comando termina com \n) - o mesmo que o app web usa:
    M1:F      motor 1 avança            M1:R   motor 1 recua        M1:S  motor 1 para
    M1:V200   velocidade do motor 1 (PWM 80..255)
    X         PARA TUDO                 H      "estou aqui" (heartbeat)      ?  pede status
  Respostas do Arduino:
    ST:<motor>:<dir>:<pwm>:<fcRecuado>:<fcAvancado>   (dir: 1 avança, -1 recua, 0 parado)
    LIM:<motor>:MIN|MAX    parou por fim de curso
    WD                     parou por perda de comunicação
    TMAX:<motor>           parou por tempo máximo
    OK:X | PRONTO | ERRO

  Dá para testar sem o tablet: abra o Monitor Serial (9600, "Nova linha") e digite M1:F, M1:S etc.
*/

#include <SoftwareSerial.h>

// ---------------------------------------------------------------- CONFIGURAÇÃO
const uint8_t NUM_MOTORES = 3;

const uint8_t PIN_EN[NUM_MOTORES]  = {5, 6, 3};     // PWM (ENA/ENB do driver)
const uint8_t PIN_IN1[NUM_MOTORES] = {7, 9, 11};
const uint8_t PIN_IN2[NUM_MOTORES] = {8, 10, 12};

const uint8_t PIN_FC_RECUADO[NUM_MOTORES]  = {A0, A2, A4};  // fim de curso "recuar"
const uint8_t PIN_FC_AVANCADO[NUM_MOTORES] = {A1, A3, A5};  // fim de curso "avançar"

const uint8_t  BLE_RX = 2;   // Arduino RECEBE  (liga no TX do módulo)
const uint8_t  BLE_TX = 4;   // Arduino ENVIA   (liga no RX do módulo, via divisor de tensão)
const long     BAUD_BLE = 9600;

const unsigned long TIMEOUT_COM_MS = 1500;   // sem sinal do app por esse tempo => para tudo
const unsigned long TEMPO_MAX_MS   = 15000;  // tempo máximo contínuo de um motor ligado
const uint8_t  PWM_MIN = 80;                 // abaixo disso o motor costuma não girar
const uint8_t  PWM_PADRAO = 200;

// Se um motor andar ao contrário do esperado, troque o valor para true:
const bool INVERTER[NUM_MOTORES] = {false, false, false};

// ---------------------------------------------------------------- ESTADO
SoftwareSerial ble(BLE_RX, BLE_TX);

int8_t        direcao[NUM_MOTORES] = {0, 0, 0};
uint8_t       pwmMotor[NUM_MOTORES] = {PWM_PADRAO, PWM_PADRAO, PWM_PADRAO};
unsigned long inicioMotor[NUM_MOTORES] = {0, 0, 0};
unsigned long ultimoSinal = 0;

char    linhaBle[24];
uint8_t nBle = 0;
char    linhaUsb[24];
uint8_t nUsb = 0;

// ---------------------------------------------------------------- UTILITÁRIOS
void enviar(const char *texto) {
  ble.println(texto);
  Serial.println(texto);
}

bool fcRecuado(uint8_t i)  { return digitalRead(PIN_FC_RECUADO[i])  == LOW; }
bool fcAvancado(uint8_t i) { return digitalRead(PIN_FC_AVANCADO[i]) == LOW; }

void enviarStatus(uint8_t i) {
  char buf[32];
  snprintf(buf, sizeof(buf), "ST:%u:%d:%u:%u:%u", i + 1, direcao[i], pwmMotor[i],
           fcRecuado(i) ? 1 : 0, fcAvancado(i) ? 1 : 0);
  enviar(buf);
}

// ---------------------------------------------------------------- MOTORES
void aplicar(uint8_t i) {
  int8_t d = direcao[i];
  if (INVERTER[i]) d = -d;

  if (d > 0) {
    digitalWrite(PIN_IN1[i], HIGH);
    digitalWrite(PIN_IN2[i], LOW);
    analogWrite(PIN_EN[i], pwmMotor[i]);
  } else if (d < 0) {
    digitalWrite(PIN_IN1[i], LOW);
    digitalWrite(PIN_IN2[i], HIGH);
    analogWrite(PIN_EN[i], pwmMotor[i]);
  } else {
    analogWrite(PIN_EN[i], 0);
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

void iniciarMotor(uint8_t i, int8_t d) {
  // não anda contra o fim de curso
  if (d > 0 && fcAvancado(i)) {
    char b[16]; snprintf(b, sizeof(b), "LIM:%u:MAX", i + 1); enviar(b);
    pararMotor(i);
    return;
  }
  if (d < 0 && fcRecuado(i)) {
    char b[16]; snprintf(b, sizeof(b), "LIM:%u:MIN", i + 1); enviar(b);
    pararMotor(i);
    return;
  }
  // inversão de sentido: para um instante antes (protege driver e motor)
  if (direcao[i] != 0 && direcao[i] != d) {
    pararMotor(i, false);
    delay(80);
  }
  direcao[i] = d;
  inicioMotor[i] = millis();
  aplicar(i);
  enviarStatus(i);
}

// ---------------------------------------------------------------- COMANDOS
void processar(char *s) {
  if (s[0] == 'H') return;                       // heartbeat: só atualiza ultimoSinal

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
    if (c == 'F')      iniciarMotor(i, +1);
    else if (c == 'R') iniciarMotor(i, -1);
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

void lerFonte(Stream &fonte, char *buf, uint8_t &n) {
  while (fonte.available()) {
    char c = fonte.read();
    ultimoSinal = millis();
    if (c == '\n' || c == '\r') {
      if (n > 0) {
        buf[n] = 0;
        processar(buf);
        n = 0;
      }
    } else if (n < sizeof(linhaBle) - 1) {
      buf[n++] = c;
    } else {
      n = 0;   // linha longa demais: descarta
    }
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
      char b[16]; snprintf(b, sizeof(b), "LIM:%u:MAX", i + 1); enviar(b);
    } else if (direcao[i] < 0 && fcRecuado(i)) {
      pararMotor(i);
      char b[16]; snprintf(b, sizeof(b), "LIM:%u:MIN", i + 1); enviar(b);
    } else if (agora - inicioMotor[i] > TEMPO_MAX_MS) {
      pararMotor(i);
      char b[16]; snprintf(b, sizeof(b), "TMAX:%u", i + 1); enviar(b);
    }
  }

  // perdeu a comunicação com o tablet => para tudo
  if (algumLigado && (agora - ultimoSinal > TIMEOUT_COM_MS)) {
    pararTodos();
    enviar("WD");
  }
}

// ---------------------------------------------------------------- SETUP / LOOP
void setup() {
  for (uint8_t i = 0; i < NUM_MOTORES; i++) {
    pinMode(PIN_EN[i], OUTPUT);
    pinMode(PIN_IN1[i], OUTPUT);
    pinMode(PIN_IN2[i], OUTPUT);
    pinMode(PIN_FC_RECUADO[i], INPUT_PULLUP);
    pinMode(PIN_FC_AVANCADO[i], INPUT_PULLUP);
  }
  pararTodos();

  Serial.begin(9600);
  ble.begin(BAUD_BLE);
  ultimoSinal = millis();
  enviar("PRONTO");
}

void loop() {
  lerFonte(ble, linhaBle, nBle);
  lerFonte(Serial, linhaUsb, nUsb);
  verificarSeguranca();
}
