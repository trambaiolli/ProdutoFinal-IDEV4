# Braço hidráulico: painel web para três motores e Arduino

Este pacote contém todos os arquivos do painel em HTML, CSS e JavaScript, firmware do Arduino e ponte de configuração do Bluetooth. Controla **três motores CC independentes**, cada um com avanço, recuo, PWM e dois fins de curso. Um botão global para todos. Cada motor movimenta o êmbolo de sua própria seringa de controle. A posição real, a força e a velocidade não são medidas.

Você pode associar Motor 1, 2 e 3 às articulações desejadas. Esses nomes não supõem uma mecânica específica. Três circuitos hidráulicos exigem pelo menos três pares de seringas, um par por motor. Se houver um quarto movimento independente, como rotação da base ou garra adicional, ele não será controlado por este código sem ampliação.

## 1. Hardware definido para estes códigos

| Componente | Quantidade / condição |
|---|---|
| Arduino Uno R3 / ATmega328P e cabo USB | 1 |
| HM-10 BLE UART | 1; serviço FFE0 e característica FFE1 com escrita/notificação; UART 9600 |
| Breakout TB6612FNG | **2**; dois canais do driver 1 e um canal do driver 2 |
| Motor CC com redução | **3**; compatíveis com fonte e drivers |
| Fuso, porca móvel, guias e suporte rígido | **3 conjuntos**, um por motor |
| Microinterruptores de fim de curso | **6**; COM e NC (normalmente fechado) |
| Fonte dos motores | Tensão nominal dos motores e capacidade para os três partirem simultaneamente |
| Chave física da alimentação de potência | 1; acessível durante os testes |
| Resistores de 1 kΩ e 2 kΩ | 1 de cada para divisor do TX Bluetooth |
| Resistores de 10 kΩ | 2, um de STBY a GND em cada driver |
| Tablet ou computador | Navegador compatível com Web Bluetooth BLE; exemplo considera Chrome no Android |
| Circuitos hidráulicos | 3, com seringas sem agulhas, mangueiras preenchidas e estrutura |

**O firmware é para TB6612FNG.** Não use a pinagem como se fosse L298N. Verifique a corrente de partida/travamento de cada motor e a dissipação total do driver, especialmente dos dois canais no mesmo chip. A ficha Toshiba consultada informa VM de operação de 2,5 a 13,5 V; limites de corrente dependem da tensão, duração e dissipação. Corrente de pico não é capacidade contínua. Se os motores excederem esses limites, é necessário outro driver e adaptação do circuito/código. A fonte precisa suportar a soma das correntes, não apenas a de um motor.

O módulo Bluetooth genérico do infográfico agora é definido como **HM-10 UART BLE FFE0/FFE1**. HC-05/HC-06 Bluetooth clássico não funciona neste painel. Módulos vendidos como HM-10 podem ter firmware/UUID diferentes. Confira o componente recebido. O breakout Keyestudio KS0174 documenta FFE0/FFE1 e alimentação de 5 V; um HM-10 sem placa reguladora não deve receber 5 V. Siga a ficha da sua placa.

## 2. Ligações dos dois drivers e três motores

Faça as ligações com a fonte dos motores desligada. Arduino alimentado por USB; motores alimentados por fonte de potência própria. A nomenclatura A01/A02 em alguns breakouts corresponde a AO1/AO2 na ficha.

| Arduino / origem | Destino | Função |
|---|---|---|
| Arduino 5V | VCC dos dois TB6612 | Alimentação lógica |
| Fonte dos motores positiva | VM dos dois TB6612, através da chave física | Potência, tensão nominal dos motores |
| Fonte negativa + Arduino GND | GND dos dois drivers e HM-10 | Referência comum |
| **D3** | Driver 1 PWMA | PWM Motor 1 |
| **D7** | Driver 1 AIN1 | Sentido Motor 1 |
| **D8** | Driver 1 AIN2 | Sentido Motor 1 |
| Driver 1 AO1/AO2 | Terminais Motor 1 | Potência |
| **D5** | Driver 1 PWMB | PWM Motor 2 |
| **A2** | Driver 1 BIN1 | Sentido Motor 2 |
| **A3** | Driver 1 BIN2 | Sentido Motor 2 |
| Driver 1 BO1/BO2 | Terminais Motor 2 | Potência |
| **D6** | Driver 2 PWMA | PWM Motor 3 |
| **A4** | Driver 2 AIN1 | Sentido Motor 3 |
| **A5** | Driver 2 AIN2 | Sentido Motor 3 |
| Driver 2 AO1/AO2 | Terminais Motor 3 | Potência |
| **D13** | STBY dos dois drivers | Habilitação compartilhada |
| Resistor 10 kΩ em cada driver | STBY → GND | Desabilitação quando pino não acionado |
| Driver 2 BIN1, BIN2 e PWMB | GND | Canal B sem uso |

Não alimente os motores pelo 5V do Uno. Não una VM ao 5V lógico. Confira os capacitores e limites da placa real. A0–A5 funcionam como pinos digitais no sketch. A4/A5 não ficam disponíveis para I²C nesta pinagem. D0/D1 permanecem livres para USB; D10/D11 são do Bluetooth. **Esta pinagem substitui a versão anterior para um motor.**

## 3. Ligações do Bluetooth

| Origem | Destino |
|---|---|
| HM-10 TX | Arduino D10 (recepção SoftwareSerial) |
| Arduino D11 | Resistor de 1 kΩ → ponto central → HM-10 RX |
| Ponto central do divisor | Resistor de 2 kΩ → GND |
| HM-10 GND | GND comum |
| HM-10 VCC | Alimentação especificada para o módulo/breakout |

O divisor reduz o TX de aproximadamente 5 V para 3,3 V. Ele é para **sinal**, não para alimentar o módulo. TX e RX são cruzados. O firmware usa `SoftwareSerial(10, 11)` a 9600 baud; os pinos 0/1 ficam livres para carregar o sketch por USB. Para módulos com exigências elétricas diferentes, use conversor de nível compatível com a ficha do fabricante.

## 4. Seis fins de curso

| Motor | Avanço: NC ao pino | Recuo: NC ao pino | COM dos dois |
|---|---|---|---|
| 1 | **D2** | **D4** | GND |
| 2 | **D9** | **D12** | GND |
| 3 | **A0** | **A1** | GND |

Use COM e NC, não NO. Cada pino usa INPUT_PULLUP: contato NC fechado lê LOW; acionamento ou fio aberto lê HIGH. Ajuste cada interruptor para atuar antes do extremo físico do êmbolo. O limite bloqueia somente seu motor naquele sentido; permite afastar-se no sentido oposto. A ligação detecta abertura de fio, mas não todas as falhas, como curto ao GND. Fios devem ser curtos e bem fixados.

## 5. Configurar o HM-10

1. Desligue VM (fonte do motor) e mantenha o Bluetooth sem conexão com celular/tablet.
2. Na Arduino IDE, abra `arduino/configurar_hm10/configurar_hm10.ino`.
3. Selecione **Arduino Uno** e a porta USB correspondente. Carregue o sketch.
4. Abra Monitor Serial em **9600 baud** e **Nenhum final de linha**.
5. Envie `AT`. Um módulo compatível deve responder `OK`. A ponte não acrescenta CR/LF.
6. Para firmware Huamao que documente esses comandos, envie um por vez:

| Comando | Finalidade |
|---|---|
| `AT+ROLE0` | Periférico BLE |
| `AT+IMME0` | Inicialização automática |
| `AT+NAMEBracoHidraulico` | Nome anunciado |
| `AT+BAUD0` | UART 9600 nos firmwares que usam esse mapeamento |
| `AT+RESET` | Reiniciar módulo |

**Confira a sintaxe no manual da versão do seu HM-10 antes de alterar parâmetros.** Esses comandos não são universais para clones, HC-08 ou outras versões. O objetivo indispensável é: periférico BLE, transmissão UART transparente a 9600, serviço FFE0, característica FFE1 com escrita e notificação. O nome é opcional: o painel permite selecionar manualmente o dispositivo.

Se `AT` não responder: revise energia/GND e TX/RX, confirme que não há conexão BLE aberta e consulte o baud de fábrica. Se ele for diferente, ajuste `BLE_BAUD` no sketch de configuração para o valor atual. Para módulos a 115200, a SoftwareSerial no Uno pode não ser confiável: configure com um adaptador USB-UART compatível com os níveis elétricos do módulo e depois retorne a 9600. Não envie comandos de outro fabricante às cegas. Não precisa parear nas configurações do Android: a seleção BLE ocorre na página.

## 6. Carregar o programa de controle

1. Abra `arduino/braco_hidraulico/braco_hidraulico.ino` na Arduino IDE.
2. Selecione Arduino Uno e a porta correta.
3. A biblioteca SoftwareSerial acompanha o suporte AVR do Arduino; não precisa de biblioteca BLE no Uno, pois o HM-10 faz a ponte UART.
4. Clique em Verificar e depois Carregar. A0–A5 são usados como pinos digitais neste projeto; não há leitura analógica nesses pinos.
5. Mantenha VM desligado até concluir os testes de comunicação e limites.

O sketch de configuração é temporário. Ele deve ser substituído pelo sketch de controle para operar o painel.

## 7. Arquivos do painel

| Arquivo | Responsabilidade |
|---|---|
| `web/index.html` | Estrutura dos três conjuntos de controles e indicadores |
| `web/style.css` | Layout responsivo, cores e tamanho dos botões |
| `web/app.js` | Conexão BLE, comandos, repetição, telemetria e parada |

Não precisa de Node, framework, API na nuvem nem banco de dados. O navegador comunica diretamente com o módulo próximo, enquanto a página pode ser servida por um servidor estático.

### Teste no computador

Com Python 3 instalado, abra um terminal na pasta do pacote:

```bash
python -m http.server 8000 --directory web --bind 127.0.0.1
```

Abra `http://localhost:8000` no próprio computador em navegador compatível. O computador também precisa de Bluetooth BLE. `localhost` funciona como contexto seguro para desenvolvimento.

### Uso no tablet

Sirva os três arquivos da pasta `web` em uma hospedagem estática **HTTPS**. O `index.html` deve ser a página inicial, e `style.css` e `app.js` devem permanecer na mesma pasta. Alternativamente, configure um servidor local HTTPS com certificado confiável pelo tablet. Este pacote não publica automaticamente a página.

Abra a URL HTTPS no Chrome do Android, habilite Bluetooth e conceda as permissões solicitadas pelo navegador/sistema. **Abrir `http://IP-DO-PC:8000` no tablet não equivale a localhost e normalmente bloqueia Web Bluetooth.** Não dependa de `file://` para uso operacional. A página deve ser aberta diretamente, não dentro de prévia/iframe de outra aplicação.

Este exemplo não promete funcionamento no Safari/iPad: a disponibilidade depende da API Web Bluetooth do navegador. O painel verifica `navigator.bluetooth` e contexto seguro e informa quando indisponíveis. Para iPad sem suporte, será preciso outro transporte ou aplicativo nativo, não apenas mudar o CSS.

## 8. Primeiro uso: testar um motor por vez

1. Energize Arduino e HM-10, com VM dos dois drivers desligado.
2. Abra o painel, toque **Conectar Bluetooth** e selecione seu módulo. Aguarde a telemetria dos **três motores**; a conexão BLE sozinha não libera os movimentos.
3. Pressione manualmente cada um dos seis limites e confira o indicador do motor correspondente. Se aparecerem ativos sem serem pressionados, revise COM/NC e GND.
4. Desacople os três motores dos fusos. Ligue a fonte e comece com PWM baixo. Teste Avançar, Recuar e Parar do Motor 1; depois repita no 2 e no 3. Soltar o botão deve parar apenas aquele motor.
5. Corrija o sentido de cada motor com fonte desligada: troque os dois fios da saída correspondente ou altere o campo `inverted` de `false` para `true` na linha daquele motor em `axes[3]`, e recarregue. Os limites devem continuar correspondendo ao avanço/recuo real.
6. Acople um mecanismo por vez e ajuste os limites antes dos extremos. Faça pequenos movimentos sem carga pesada.
7. Verifique limite de avanço e recuo de cada eixo; os demais motores não devem ser afetados pelo limite daquele eixo.
8. Teste **PARAR TODOS**, perder Bluetooth e trocar de aba. Todos devem parar. Não há retomada automática após reconexão.
9. Somente após os testes individuais experimente movimentos simultâneos, mantendo todos os mecanismos à vista e respeitando a capacidade da fonte/driver e as colisões mecânicas.

No tablet, os botões de diferentes motores aceitam toques simultâneos. Cada botão exige segurar; não existe movimento contínuo por um simples clique. No computador, mouse normalmente opera um botão por vez; teclado permite segurar Espaço/Enter no botão focado, com Escape para parar todos. Fins de curso limitam o êmbolo; **não impedem colisão entre partes do braço**. Não há planejamento de trajetória ou coordenação automática.

PWM ajusta potência, sem sensor de velocidade. Motor travado não deve ser resolvido apenas aumentando PWM. PARAR é comando por software, não emergência certificada. Tenha uma chave física na fonte. Quando todos param, STBY fica baixo e as saídas em alta impedância; com outro motor ativo, o canal parado recebe entradas/PWM baixos. Isso não fornece sustentação mecânica ou freio de segurança. Garanta que a mecânica não solte ou mova a carga sozinha na parada.

## 9. Protocolo de três motores

| Mensagem ASCII, final LF (`\n`) | Ação |
|---|---|
| `S\n` | Parar todos e habilitar recepção de novos movimentos |
| `M:1,A,89\n` | Avançar Motor 1 com PWM 89/255 |
| `M:2,R,89\n` | Recuar Motor 2 com PWM 89/255 |
| `M:3,S,0\n` | Parar somente Motor 3 |
| `T:2,S,0,1\n` | Telemetria Motor 2: parado, avanço livre, recuo ativo |

O painel converte 10–70% em PWM 26–178. O exemplo 89 é aproximadamente 35%. Cada mensagem tem menos de 20 bytes. O Arduino envia uma linha de telemetria a cada 100 ms, alternando os três motores: ciclo completo em aproximadamente 300 ms. A leitura JavaScript trata fragmentos e mensagens agrupadas BLE.

A cada 150 ms o painel envia o estado de cada motor, com escritas BLE serializadas. Paradas têm prioridade sobre movimentos pendentes. Quando todos estão parados, envia apenas `S`. Não acumula uma fila de movimentos antigos. Há **timeout independente de 450 ms por motor** desde o último comando válido destinado a ele; mensagens de outro motor não mantêm um eixo em movimento. O tempo efetivo inclui o processamento do loop.

Cada motor exige um comando S após boot, timeout ou comando inválido, antes de mover novamente. Mensagem inválida para todos. A pausa de inversão é de 120 ms por motor. Um sentido bloqueado por limite fica travado até receber S ou comando oposto. Com qualquer telemetria ausente por mais de 1,6 s, o painel para e desconecta. A proteção principal em perda de comunicação é o timeout no próprio Arduino, que independe de o navegador conseguir enviar PARAR.

O comando `S` global inicial é enviado ao conectar. O painel só libera avanço/recuo quando os três estados T chegam. Nunca assume posição real: os indicadores são estado do comando/saída e contatos dos limites, não movimento medido.

## 10. Problemas comuns

| Sintoma | Verificação |
|---|---|
| Conectar desabilitado | HTTPS/localhost e navegador com Web Bluetooth |
| HM-10 não aparece | BLE ligado, alimentação, fora de conexão com outro aplicativo |
| FFE0/FFE1 não encontrados | Firmware/UUID incompatível ou clone |
| Conecta, mas não libera movimentos | Firmware dos três motores, UART 9600, TX/RX, GND e notificações |
| Limites aparecem ativos | Fio aberto, COM/NO em vez de COM/NC ou GND ausente |
| Apenas um motor funciona | Pinagem, canal A/B, fonte e limites do motor específico |
| Motor ou Arduino reinicia | Corrente total, queda de tensão/ruído, fonte e desacoplamento |
| Sentido invertido | Fios da saída ou campo `inverted` daquele motor |
| Para ao tentar mover três motores | Capacidade da fonte/driver, interferência ou comandos não renovados a tempo; revise o registro e UART |

## 11. Validação e limites do pacote

O JavaScript foi verificado quanto à sintaxe e a interface/protocolo foi testada com Bluetooth simulado. A lógica do firmware foi compilada e exercitada em C++ com camada de simulação: três eixos independentes, parada global, timeout por eixo, limites, reversão e mensagens inválidas. Isso não substitui a compilação na Arduino IDE para AVR nem os testes nos seus módulos, drivers e motores. Não houve acesso físico ao hardware.

## Referências técnicas

- Web Bluetooth, requisitos de contexto seguro, seleção por gesto e notificações: https://developer.chrome.com/docs/capabilities/bluetooth
- HM-10 Keyestudio KS0174, interface BLE e alimentação do breakout: https://docs.keyestudio.com/projects/KS0174/en/latest/docs/KS0174.html
- SoftwareSerial, Arduino: https://docs.arduino.cc/learn/built-in-libraries/software-serial/
- TB6612FNG, ficha Toshiba: https://toshiba.semicon-storage.com/info/TB6612FNG_datasheet_en_20141001.pdf?did=10660&prodName=TB6612FNG

As ligações são específicas dos componentes descritos. O infográfico é conceitual; use esta pinagem e as fichas dos módulos reais para a montagem.
