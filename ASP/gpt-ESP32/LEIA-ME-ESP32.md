# Braço Hidráulico ESP32

Conversão do projeto Arduino Uno + HM-10 para ESP32 DevKit V1 / ESP32-WROOM-32.

## Arquitetura

Tablet/PC -> Wi-Fi -> ESP32 -> 2x TB6612FNG -> 3 motores -> fusos -> seringas.

O ESP32 cria sua própria rede Wi-Fi e pode servir o painel diretamente. O HM-10 e SoftwareSerial não são necessários.

## Rede

- SSID: `BracoHidraulico`
- Senha: `12345678`
- Endereço padrão do AP: `http://192.168.4.1`

Altere SSID/senha no firmware antes de uso permanente, se desejado.

## Pinagem

| Função | GPIO ESP32 |
|---|---:|
| Motor 1 PWMA | 25 |
| Motor 1 AIN1 | 26 |
| Motor 1 AIN2 | 27 |
| Motor 2 PWMB | 14 |
| Motor 2 BIN1 | 16 |
| Motor 2 BIN2 | 17 |
| Motor 3 PWMA | 33 |
| Motor 3 AIN1 | 18 |
| Motor 3 AIN2 | 19 |
| STBY dos dois drivers | 23 |
| M1 fim avanço | 32 |
| M1 fim recuo | 13 |
| M2 fim avanço | 21 |
| M2 fim recuo | 22 |
| M3 fim avanço | 4 |
| M3 fim recuo | 5 |

### TB6612FNG

- VCC lógico: use nível compatível com o ESP32/TB6612FNG (3,3 V é adequado para a lógica do TB6612FNG).
- VM: fonte própria dos motores, dentro dos limites do driver e dos motores.
- GND: ESP32, fonte dos motores e os dois drivers em comum.
- Driver 1: canal A = Motor 1; canal B = Motor 2.
- Driver 2: canal A = Motor 3.
- STBY dos dois drivers vai ao GPIO 23.
- Mantenha o pull-down de 10 kΩ em cada STBY para GND.
- Não alimente os motores pelo ESP32.

### Fins de curso

Use COM e NC:
- COM -> GND
- NC -> GPIO indicado

O firmware usa INPUT_PULLUP:
- contato normal fechado = LOW;
- fim acionado ou fio aberto = HIGH.

Ajuste o interruptor para atuar antes do limite mecânico do êmbolo.

## Instalação

1. Instale o suporte de placas ESP32 na Arduino IDE.
2. Abra `esp32/braco_hidraulico_esp32/braco_hidraulico_esp32.ino`.
3. Selecione a placa correspondente ao seu ESP32 (por exemplo ESP32 Dev Module).
4. Selecione a porta.
5. Mantenha VM dos motores desligado.
6. Compile e carregue.
7. Abra o Monitor Serial em 115200.
8. O ESP32 mostrará o IP do painel.
9. No tablet, conecte ao Wi-Fi `BracoHidraulico`.
10. Abra `http://192.168.4.1`.

## Primeiro teste

1. Deixe a fonte VM desligada.
2. Abra o painel.
3. Pressione manualmente os seis fins de curso e confira os indicadores.
4. Desacople os motores dos fusos.
5. Ligue VM e use potência baixa.
6. Pressione PARAR TODOS uma vez para armar o controle.
7. Teste Motor 1, depois Motor 2 e Motor 3.
8. Confirme avanço/recuo e limites.
9. Acople um mecanismo por vez.
10. Só depois teste movimentos simultâneos.

## Segurança implementada

- timeout independente de 450 ms por motor;
- renovação do comando pelo painel a cada 150 ms;
- fim de curso independente por sentido;
- parada global;
- pausa de 120 ms ao inverter;
- após timeout/comando inválido, o eixo fica desarmado até receber S;
- STBY desabilitado quando nenhum motor está ativo.

`PARAR TODOS` é software. Use uma chave física acessível para cortar a alimentação VM dos motores. Fim de curso não detecta colisão entre partes do braço.

## Pasta web

`web/index.html`, `web/style.css` e `web/app.js` são uma versão separada do painel para desenvolvimento. O firmware `.ino` já contém uma versão incorporada do painel, portanto esses três arquivos não são necessários para o uso básico.

## Observação

Esta conversão foi criada a partir das especificações do LEIA-ME original. Os arquivos-fonte completos do firmware/painel Arduino anterior não estavam disponíveis, portanto não é uma conversão linha a linha. Teste a montagem fisicamente com carga desacoplada antes do uso.
