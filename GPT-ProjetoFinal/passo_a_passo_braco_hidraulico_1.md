# Braço hidráulico controlado por tablet — passo a passo

**Arquitetura:** Tablet (interface web) → Bluetooth BLE → Arduino Uno → driver de motor (ponte H) → motor com redução → fuso → seringa de controle → mangueira com água → seringa atuadora → articulação.

O projeto usa **3 circuitos hidráulicos independentes**, cada um com seu motor. Sugestão de uso:

| Circuito | Motor | Articulação |
|---|---|---|
| 1 | Motor 1 | Ombro / base |
| 2 | Motor 2 | Cotovelo |
| 3 | Motor 3 | Garra |

---

## 1. Lista de materiais (3 circuitos)

**Eletrônica**
- 1× Arduino Uno (+ cabo USB)
- 1× módulo Bluetooth **BLE** HM-10 (ou clone AT-09 / BT-05)
  > ⚠️ O **HC-05 / HC-06 não serve**: são Bluetooth clássico e o navegador (Web Bluetooth) só fala BLE.
- 2× driver ponte H L298N (3 dos 4 canais serão usados). Alternativa: 2× TB6612FNG
- 3× motor CC 12 V com caixa de redução (algo entre 30 e 100 rpm é suficiente)
- 1× fonte 12 V / 3 A (baixa tensão) para os motores
- 6× microchave fim de curso (2 por motor)
- 1× resistor 1 kΩ + 1× resistor 2 kΩ (divisor de tensão para o RX do módulo BLE)
- 1× capacitor eletrolítico 470 µF / 25 V (na entrada da fonte dos motores)
- Fios, protoboard ou placa perfurada, conectores
- Tablet Android com Chrome (ou iPad com o app Bluefy)

**Mecânica / hidráulica**
- 3× fuso (ex.: T8, 150–200 mm) com castanha, guias lisas e mancais — ou haste roscada M8 com porca
- 3× acoplamento motor–fuso
- 6× seringas de 10 mL (3 de controle + 3 atuadoras) — use o mesmo diâmetro nos pares
- Mangueira de silicone/cristal de 4–6 mm, conexões e abraçadeiras
- Água (com algumas gotas de corante, ajuda a ver bolhas)
- Papelão grosso, palitos, cola quente / cola branca, parafusos

---

## 2. Etapa 1 — Montar o braço e a hidráulica

1. Corte em papelão: **base**, **2 segmentos do braço** e **garra** (dupla camada de papelão colada para dar rigidez). Una os segmentos com parafusos pequenos e arruelas (articulações livres).
2. Fixe uma **seringa atuadora** em cada articulação: o corpo da seringa em um segmento e a ponta da haste no segmento seguinte, de modo que empurrar/puxar a haste dobre a articulação.
3. Conecte cada seringa atuadora à sua **seringa de controle** com a mangueira. Cada par forma um circuito **fechado e independente**.
4. **Encha o circuito sem bolhas:** encha a seringa de controle com água, ligue a mangueira, empurre devagar até a água sair pela outra ponta, conecte a seringa atuadora com o êmbolo recuado e vá empurrando com a ponta para cima. Bolhas = movimento "mole" e impreciso.
5. Deixe o êmbolo da seringa de controle **na posição intermediária**, para poder avançar e recuar.

> Dica: se o par de seringas tiver o mesmo diâmetro, o curso da articulação é igual ao curso do motor (relação 1:1). Seringa atuadora menor = mais curso/mais velocidade, menos força.

## 3. Etapa 2 — Módulo de acionamento (motor + fuso + fins de curso)

1. Monte sobre uma base (papelão/MDF) para cada circuito: **motor com redução → acoplamento → fuso → castanha (suporte móvel)**, com uma guia lisa paralela para a castanha não girar junto com o fuso.
2. Prenda a **haste da seringa de controle** ao suporte móvel; o corpo da seringa fica fixo na base.
3. Instale **2 fins de curso** por módulo, um em cada extremidade do curso:
   - **Recuado:** acionado quando o suporte chega perto do motor.
   - **Avançado:** acionado quando o êmbolo está quase no fundo da seringa.
   - Posicione-os **antes** do limite mecânico da seringa (deixe ~3–5 mm de folga).
4. Gire o fuso à mão: o suporte deve correr sem travar de ponta a ponta e acionar as duas chaves.

## 4. Etapa 3 — Ligações elétricas

### 4.1 Tabela de pinos

| Função | Arduino | Destino |
|---|---|---|
| Motor 1 – velocidade (PWM) | D5 | Driver A · ENA |
| Motor 1 – direção | D7 / D8 | Driver A · IN1 / IN2 |
| Motor 2 – velocidade (PWM) | D6 | Driver A · ENB |
| Motor 2 – direção | D9 / D10 | Driver A · IN3 / IN4 |
| Motor 3 – velocidade (PWM) | D3 | Driver B · ENA |
| Motor 3 – direção | D11 / D12 | Driver B · IN1 / IN2 |
| Fim de curso motor 1 (recuado / avançado) | A0 / A1 | chave → GND |
| Fim de curso motor 2 (recuado / avançado) | A2 / A3 | chave → GND |
| Fim de curso motor 3 (recuado / avançado) | A4 / A5 | chave → GND |
| Bluetooth – recebe do módulo | D2 | TX do módulo BLE |
| Bluetooth – envia ao módulo | D4 | RX do módulo BLE (**via divisor**) |
| Alimentação do módulo BLE | 5 V | VCC (HM-10 em placa aceita 3,6–6 V) |
| Terra | GND | GND do módulo, dos drivers e da fonte |

Os pinos D0/D1 ficam livres para o USB (gravação e Monitor Serial).

### 4.2 Detalhes importantes

- **Divisor de tensão no RX do módulo BLE** (lógica 3,3 V):
  ```
  Arduino D4 ──[ 1 kΩ ]──┬── RX do módulo BLE
                         │
                      [ 2 kΩ ]
                         │
                        GND
  ```
- **Drivers L298N:** retire os jumpers de **ENA** e **ENB** (senão o PWM não controla a velocidade) e ligue os pinos de enable aos pinos PWM do Arduino. Mantenha o jumper de 5 V ligado se a fonte for ≤ 12 V e **não** ligue o pino "5V" do driver ao Arduino.
- **Fonte dos motores:** 12 V no borne de potência dos dois drivers (em paralelo) e **GND da fonte ligado ao GND do Arduino** (GND comum — obrigatório).
- Coloque o capacitor de 470 µF entre + e − da fonte, perto dos drivers.
- **Fins de curso:** use os contatos NA (normalmente abertos) entre o pino e o GND. O firmware usa `INPUT_PULLUP`, então "pressionado = LOW"; não precisa de resistor.
- Alimente o Arduino pelo USB do tablet/carregador ou por fonte própria (7–9 V no Vin); não pelo driver.
- Mantenha toda a eletrônica **longe da água**, de preferência em uma caixa separada da hidráulica.

## 5. Etapa 4 — Gravar o firmware no Arduino

1. Instale o **Arduino IDE** (arduino.cc/en/software).
2. Abra o arquivo `braco_hidraulico.ino`.
3. Ferramentas → Placa → **Arduino Uno**; escolha a porta correta.
4. Clique em **Carregar** (o módulo BLE usa D2/D4, não atrapalha a gravação).
5. Abra o **Monitor Serial** (9600 baud, "Nova linha") e confira a mensagem `PRONTO`.

### Teste sem tablet (obrigatório antes de conectar o braço)

Com os motores soltos do fuso (ou sem seringa), digite no Monitor Serial:

| Digite | Resultado esperado |
|---|---|
| `M1:F` | motor 1 gira (sentido "avançar") e responde `ST:1:1:200:...` |
| `M1:S` | motor 1 para |
| `M1:R` | motor 1 gira no sentido contrário |
| `M1:V120` | reduz a velocidade |
| `X` | para todos |

Repita para `M2` e `M3`. Se algum motor girar ao contrário do que o nome indica (avançar deveria empurrar o êmbolo para dentro da seringa), mude `INVERTER[...]` para `true` no código **ou** troque os dois fios do motor.

Teste também cada fim de curso: com o motor ligado, pressione manualmente a chave do lado para o qual ele anda — deve parar e responder `LIM:...`. Se não parar, confira a ligação ou se o motor está no sentido certo.

## 6. Etapa 5 — Configurar o módulo Bluetooth (opcional) e testar

1. O HM-10 vem pronto para uso (9600 baud, serviço `FFE0`, característica `FFE1`) — é exatamente o que o app espera.
2. Para verificar, use o app **nRF Connect** no tablet: o módulo deve aparecer e mostrar o serviço `0xFFE0`.
3. (Opcional) renomear o módulo: com o módulo ligado a um conversor USB-serial (9600 baud, sem fim de linha) e sem nenhum aparelho conectado, envie `AT+NAMEBraco`. Alguns clones usam `AT+NAME=Braco`.

## 7. Etapa 6 — Colocar a interface no tablet

O navegador só libera o Bluetooth em páginas **HTTPS** (ou `localhost`). Escolha uma opção:

**Opção A — GitHub Pages (recomendada, grátis)**
1. Crie uma conta e um repositório novo no GitHub.
2. Envie o arquivo `index.html` para o repositório.
3. Settings → Pages → Branch `main` → Save.
4. Abra no tablet o endereço `https://SEU-USUARIO.github.io/SEU-REPOSITORIO/`.

**Opção B — Netlify Drop**
1. Acesse app.netlify.com/drop e arraste a pasta contendo o `index.html`.
2. Use o link HTTPS gerado.

**Opção C — Rede local (sem internet)**
1. No computador, na pasta do arquivo: `python3 -m http.server 8000`.
2. No tablet (Chrome), acesse `chrome://flags/#unsafely-treat-insecure-origin-as-secure`, adicione `http://IP-DO-COMPUTADOR:8000`, ative e reinicie o Chrome.
3. Abra `http://IP-DO-COMPUTADOR:8000`.

Depois de carregada a primeira vez, use o menu do Chrome → **Adicionar à tela inicial** para ter um ícone de aplicativo.

**Compatibilidade:** Chrome no Android funciona. iPad/iPhone não aceitam Web Bluetooth no Safari: use o app gratuito **Bluefy**.

## 8. Etapa 7 — Usar e calibrar

1. Ligue a fonte 12 V e o Arduino. Abra a página, toque em **Conectar Bluetooth** e escolha o módulo (nome `HMSoft`, `BT05`, `Braco` etc.). O status muda para **Conectado**.
2. Cada **Circuito** tem **Avançar / Recuar / Parar** e controle de **velocidade**. O botão grande **PARAR TUDO** vermelho interrompe os três circuitos.
3. O **Modo segurar** faz o motor só andar enquanto o botão estiver pressionado (mais seguro durante os primeiros testes).
4. Comece com velocidade em ~50–60% e movimentos curtos. Observe se o braço dobra no sentido esperado; se não, inverta com `INVERTER[]`.
5. Os indicadores "Fim de curso" ficam vermelhos quando a chave é acionada.

## 9. Protocolo de comunicação (referência)

Texto, um comando por linha (termina em `\n`):

| Comando | Ação |
|---|---|
| `M1:F` / `M2:F` / `M3:F` | avançar |
| `M1:R` / `M2:R` / `M3:R` | recuar |
| `M1:S` / `M2:S` / `M3:S` | parar o motor |
| `M1:V200` | velocidade (PWM de 80 a 255) |
| `X` | parar tudo |
| `H` | heartbeat (o app envia a cada 0,5 s) |
| `?` | pedir o status |

Respostas: `ST:motor:dir:pwm:fcRecuado:fcAvancado`, `LIM:motor:MIN/MAX`, `WD`, `TMAX:motor`, `OK:X`, `PRONTO`.

## 10. Segurança embutida (o que o firmware já faz)

- **Perda de conexão:** sem receber nada do app por 1,5 s com algum motor ligado, para tudo (`WD`).
- **Fim de curso:** o motor para e não anda mais para aquele lado.
- **Tempo máximo:** nenhum motor fica ligado mais de 15 s seguidos (`TMAX`).
- **Inversão de sentido** com pausa de 80 ms para proteger o driver.
- **Ao fechar/minimizar o app**, ele envia `X`.

Cuidados físicos: não deixe a água chegar à eletrônica; confirme que os fins de curso param o êmbolo antes do fundo da seringa; não use fonte acima da tensão do motor; desligue a fonte antes de mexer nas ligações.

## 11. Problemas comuns

| Sintoma | Causa provável / solução |
|---|---|
| Módulo não aparece na lista | É HC-05/HC-06 (Bluetooth clássico) ou está pareado com outro aparelho. Use HM-10/BLE e desconecte outros. |
| Aparece, mas dá erro ao conectar | Módulo clone com UUID diferente: use o nRF Connect para ver o serviço/característica e ajuste `SERVICE_UUID` e `CHAR_UUID` no `index.html`. |
| Conecta mas nada se move | GND não está comum; fonte dos motores desligada; jumper ENA/ENB ainda no L298N; velocidade abaixo de ~35%. |
| Motor vibra mas não gira | PWM muito baixo ou fonte fraca (queda de tensão). Aumente a velocidade/corrente da fonte. |
| Movimento "elástico" no braço | Bolhas de ar na hidráulica. Refaça o enchimento. |
| Para sozinho depois de alguns segundos | Tempo máximo (`TEMPO_MAX_MS`), perda de sinal (`WD`) ou fim de curso. Veja o Registro no app. |
| Arduino reinicia ao ligar motor | Queda/ruído na alimentação: separe a alimentação do Arduino, adicione o capacitor e confira o GND. |
| Botões físicos Avançar/Recuar da ilustração | Não estão incluídos: o Uno já usa todos os pinos úteis. Para adicioná-los use um Arduino Mega ou um expansor I²C (PCF8574). |
