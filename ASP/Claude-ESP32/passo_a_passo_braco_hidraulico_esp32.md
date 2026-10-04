# Braço hidráulico controlado por tablet (ESP32) — passo a passo

**Arquitetura:** Tablet (interface web) → Bluetooth BLE → **ESP32** → driver de motor (ponte H) → motor com redução → fuso → seringa de controle → mangueira com água → seringa atuadora → articulação.

O ESP32 já traz Bluetooth BLE embutido, então **não é preciso módulo Bluetooth externo** (nada de HM-10, divisor de tensão ou resistores).

O projeto usa **3 circuitos hidráulicos independentes**, cada um com seu motor:

| Circuito | Motor | Articulação |
|---|---|---|
| 1 | Motor 1 | Ombro / base |
| 2 | Motor 2 | Cotovelo |
| 3 | Motor 3 | Garra |

---

## 1. Lista de materiais (3 circuitos)

**Eletrônica**
- 1× **ESP32 DevKit com ESP32-WROOM-32** (placa de 30 ou 38 pinos, com USB).
  > ⚠️ Prefira o **WROOM-32**. Nas placas com **WROVER** (têm PSRAM) os pinos GPIO16 e GPIO17 não ficam livres, e este projeto usa os dois. Os modelos ESP32-S3, C3 e S2 têm outros pinos e **não** servem sem alterar o código.
- 2× driver ponte H L298N (3 dos 4 canais serão usados). Alternativa: 2× TB6612FNG
- 3× motor CC 12 V com caixa de redução (algo entre 30 e 100 rpm é suficiente)
- 1× fonte 12 V / 3 A (baixa tensão) para os motores
- 6× microchave fim de curso (2 por motor)
- 1× capacitor eletrolítico 470 µF / 25 V (na entrada da fonte dos motores)
- 3× resistor 10 kΩ (opcional, recomendado): ligados entre cada pino EN e o GND, mantêm o motor desligado enquanto o ESP32 reinicia
- 1× carregador USB 5 V / 1 A (ou power bank) para alimentar o ESP32
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

### 4.1 Tabela de pinos (GPIO do ESP32)

| Função | GPIO | Destino |
|---|---|---|
| Motor 1 – velocidade (PWM) | 25 | Driver A · ENA |
| Motor 1 – direção | 26 / 27 | Driver A · IN1 / IN2 |
| Motor 2 – velocidade (PWM) | 32 | Driver A · ENB |
| Motor 2 – direção | 33 / 16 | Driver A · IN3 / IN4 |
| Motor 3 – velocidade (PWM) | 17 | Driver B · ENA |
| Motor 3 – direção | 18 / 19 | Driver B · IN1 / IN2 |
| Fim de curso motor 1 (recuado / avançado) | 4 / 13 | chave → GND |
| Fim de curso motor 2 (recuado / avançado) | 14 / 21 | chave → GND |
| Fim de curso motor 3 (recuado / avançado) | 22 / 23 | chave → GND |
| Terra | GND | GND dos drivers, das chaves e da fonte dos motores |
| Alimentação do ESP32 | USB | carregador/power bank 5 V |

Os pinos GPIO 0, 2, 5, 12 e 15 (que influenciam a inicialização do ESP32) e 34–39 (só entrada, sem resistor interno) foram evitados de propósito. Não mude os pinos para esses sem checar.

### 4.2 Detalhes importantes

- **Lógica 3,3 V:** o ESP32 trabalha com 3,3 V. As entradas IN1–IN4 e ENA/ENB do L298N reconhecem 3,3 V como nível alto, então a ligação é direta. **Nunca ligue nenhum sinal de 5 V a um pino do ESP32** (os pinos não toleram 5 V).
- **Drivers L298N:** retire os jumpers de **ENA** e **ENB** (senão o PWM não controla a velocidade) e ligue os pinos de enable aos GPIO de PWM da tabela. Opcional: um resistor de 10 kΩ de cada pino EN para o GND.
- **Fonte dos motores:** 12 V no borne de potência dos dois drivers (em paralelo) e **GND da fonte ligado ao GND do ESP32** (GND comum — obrigatório).
- Coloque o capacitor de 470 µF entre + e − da fonte, perto dos drivers.
- **Fins de curso:** use os contatos NA (normalmente abertos) entre o pino e o GND. O firmware usa o resistor interno de pull-up, então "pressionado = LOW"; não precisa de resistor externo.
- **Alimentação do ESP32:** pelo USB, com carregador ou power bank separados dos motores. Motores geram ruído e queda de tensão que podem reiniciar o ESP32 se dividirem a mesma fonte sem cuidado.
- Mantenha toda a eletrônica **longe da água**, de preferência em uma caixa separada da hidráulica.

## 5. Etapa 4 — Gravar o firmware no ESP32

1. Instale o **Arduino IDE 2.x** (arduino.cc/en/software).
2. Instale o suporte ao ESP32: *Ferramentas → Placa → Gerenciador de placas*, procure **esp32** e instale **"esp32 by Espressif Systems"**. (Se não achar, em *Arquivo → Preferências* acrescente em "URLs adicionais" `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.)
3. Coloque o arquivo `braco_hidraulico_esp32.ino` dentro de uma pasta chamada **`braco_hidraulico_esp32`** (o Arduino IDE exige a pasta com o mesmo nome do arquivo) e abra o arquivo.
4. *Ferramentas → Placa → esp32 → **ESP32 Dev Module***; escolha a porta (COM/ttyUSB) do ESP32.
5. Clique em **Carregar**. Se aparecer "Connecting…", segure o botão **BOOT** da placa até começar a gravar.
   - Se der erro **"Sketch too big"**, vá em *Ferramentas → Partition Scheme* e escolha **Huge APP**.
6. Abra o **Monitor Serial** (**115200** baud, "Nova linha") e confira a mensagem `PRONTO (BLE anunciando como "Braco")`.

> Não é preciso instalar nenhuma biblioteca extra: o Bluetooth usa a biblioteca BLE que já vem com o pacote do ESP32.

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

> Comandos digitados no Monitor Serial **não** ativam a parada por perda de comunicação (não há heartbeat). O limite de 15 s e os fins de curso continuam valendo.

## 6. Etapa 5 — Verificar o Bluetooth

1. Com o ESP32 ligado, abra no tablet o app **nRF Connect** (gratuito). O dispositivo **Braco** deve aparecer na lista.
2. Conecte e confira o serviço **6E400001-B5A3-F393-E0A9-E50E24DCCA9E** (Nordic UART). É exatamente o que a interface espera.
3. Para mudar o nome, edite `NOME_BLE` no `.ino` **e** `NOME_BLE` no `index.html` e grave de novo.

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

1. Ligue a fonte 12 V e o ESP32. Abra a página, toque em **Conectar Bluetooth** e escolha **Braco**. O status muda para **Conectado**.
2. Cada **Circuito** tem três botões em coluna: **▲ avançar**, **▼ recuar** e **■ parar**, mais o controle de **velocidade**. O botão grande vermelho **PARAR TUDO** interrompe os três circuitos.
3. O **Modo segurar** faz o motor só andar enquanto o botão estiver pressionado (mais seguro durante os primeiros testes).
4. Comece com velocidade em ~50–60% e movimentos curtos. Observe se o braço dobra no sentido esperado; se não, inverta com `INVERTER[]`.
5. Os indicadores de fim de curso ficam vermelhos quando a chave é acionada.
6. Só um tablet conecta por vez. Se o app falhar em conectar, feche-o em outros aparelhos.

## 9. Protocolo de comunicação (referência)

Texto, um comando por linha (termina em `\n`), escrito na característica RX do serviço Nordic UART. As respostas chegam por notificação na característica TX.

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

- **Perda de conexão:** sem receber nada do tablet por 1,5 s com algum motor ligado por Bluetooth, para tudo (`WD`).
- **Desconexão do Bluetooth:** ao desconectar, todos os motores param na hora e o ESP32 volta a anunciar.
- **Fim de curso:** o motor para e não anda mais para aquele lado.
- **Tempo máximo:** nenhum motor fica ligado mais de 15 s seguidos (`TMAX`).
- **Inversão de sentido** com pausa de 80 ms para proteger o driver.
- **Ao fechar/minimizar o app**, ele envia `X`.

Cuidados físicos: não deixe a água chegar à eletrônica; confirme que os fins de curso param o êmbolo antes do fundo da seringa; não use fonte acima da tensão do motor; desligue a fonte antes de mexer nas ligações.

## 11. Problemas comuns

| Sintoma | Causa provável / solução |
|---|---|
| ESP32 não aparece na lista do Bluetooth | Firmware não gravado ou ESP32 sem energia; confira a mensagem `PRONTO` no Monitor Serial. Verifique também se o Bluetooth e a localização do tablet estão ligados (o Android exige para buscar BLE). |
| Não consegue gravar ("Connecting…" sem fim) | Segure o botão **BOOT** durante o início da gravação; use cabo USB com dados (não só de carga); instale o driver CP210x/CH340 da placa, se o computador não reconhecer a porta. |
| `Sketch too big` | *Ferramentas → Partition Scheme → Huge APP*. |
| Erro de compilação em `ledcAttach` ou `BLE2902` | Versão do pacote ESP32 muito antiga ou muito nova. O código aceita o pacote 2.x e o 3.x; atualize para o mais recente de cada série. |
| Aparece, mas dá erro ao conectar | Desconecte outros aparelhos ou o nRF Connect, que podem estar conectados ao ESP32. |
| Conecta mas nada se move | GND não está comum; fonte dos motores desligada; jumper ENA/ENB ainda no L298N; velocidade abaixo de ~35%. |
| Motor vibra mas não gira | PWM muito baixo ou fonte fraca (queda de tensão). Aumente a velocidade/corrente da fonte. |
| Movimento "elástico" no braço | Bolhas de ar na hidráulica. Refaça o enchimento. |
| Para sozinho depois de alguns segundos | Tempo máximo (`TEMPO_MAX_MS`), perda de sinal (`WD`) ou fim de curso. Veja o Registro no app. |
| ESP32 reinicia ao ligar motor | Queda/ruído na alimentação: alimente o ESP32 por fonte USB separada, confira o capacitor e o GND comum. |
| Botões físicos Avançar/Recuar da ilustração | Não estão incluídos. Todos os pinos seguros do ESP32 WROOM já estão em uso. Para adicioná-los use um expansor I²C (PCF8574) ou troque por um ESP32 com mais pinos. |
