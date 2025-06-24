
# 🔥 SoldaPonto ESP8266

Sistema de controle para **máquina de solda ponto** desenvolvido em ESP8266, com display OLED colorido, configuração via Wi-Fi, atualização OTA e encoder rotativo para ajuste fino dos parâmetros.

---

## 🚀 Funcionalidades

- ✅ Controle de ciclo e tempo da solda (em milissegundos)
- ✅ Interface gráfica no display OLED SSD1331
- ✅ Encoder rotativo com botão:
  - Clique curto: alterna entre **edição de ciclo** e **tempo**
  - Pressione e segure (5s): inicia a **atualização OTA** direto do GitHub
- ✅ Contador de soldas realizadas (salvo na EEPROM)
- ✅ Portal de configuração via Wi-Fi (modo AP)
- ✅ Atualização de firmware OTA via GitHub
- ✅ Watchdog integrado para travamentos
- ✅ Todas as configurações salvas na EEPROM

---

## 📸 Interface Display OLED

- Mostra:
  - Ciclo (`cl`)
  - Tempo (`ms`)
  - Status do Wi-Fi (`WiFi OK` ou `X`)
  - Contador de soldas (`S:123`)
- Durante edição, o valor pisca indicando qual parâmetro está sendo alterado.

---

## 🕹️ Controles via Encoder

| Ação                  | Resultado                          |
| ----------------------| ---------------------------------- |
| **Girar**             | Aumenta ou diminui o valor        |
| **Clique curto**      | Alterna entre **Ciclo ➝ Tempo ➝ Normal** |
| **Clique longo (5s)** | Verifica e executa atualização OTA |

---

## ⚙️ Pinagem

| Função      | Pino ESP8266 | Descrição    |
| ------------| -------------| -------------|
| Trigger     | D6 (GPIO12)  | Botão de acionamento da solda |
| Triac       | RX (GPIO3)   | Saída para acionar o Triac ou relé |
| Encoder CLK | D3 (GPIO0)   | Encoder sinal CLK |
| Encoder DT  | TX (GPIO1)   | Encoder sinal DT |
| Encoder SW  | D0 (GPIO16)  | Botão do encoder |
| OLED SCK    | D7 (GPIO13)  | Display SPI Clock |
| OLED MOSI   | D5 (GPIO14)  | Display SPI Data |
| OLED CS     | D8 (GPIO15)  | Chip Select |
| OLED RES    | D1 (GPIO5)   | Reset do display |
| OLED DC     | D2 (GPIO4)   | Comando/Data |

---

## ⚠️ Importante sobre o Encoder

Se ao girar o encoder a contagem estiver **invertida** (direita diminui, esquerda aumenta), basta **trocar fisicamente os fios dos pinos CLK e DT**, ou **inverter a ordem dos pinos na criação do objeto no código**:

```cpp
encoder = new RotaryEncoder(pin_Encoder_DT, pin_Encoder_CLK, RotaryEncoder::LatchMode::TWO03);
```

---

## 🌐 Portal Wi-Fi (Modo Configuração)

- Pressione o botão do encoder durante a energização para entrar no modo de configuração Wi-Fi.
- Acesse pelo seu celular ou PC a rede chamada:

```
SoldaPontoConfig
```

- No navegador, digite:

```
http://192.168.4.1
```

- Tela do portal:

- 🔌 Configure a rede Wi-Fi
- 🚀 Atualização OTA manual
- ❌ Reset Wi-Fi
- 🗑️ Reset Total (EEPROM)

---

## 🔥 Atualização OTA Automática

- Toda vez que conecta no Wi-Fi, verifica automaticamente se há uma versão nova no seu GitHub:

```
https://github.com/JorgeBeserra/SoldaPonto/releases
```

- Ou manualmente segurando o botão do encoder por 5 segundos.

---

## 💾 Salvamento na EEPROM

- ✔️ Dados armazenados:
  - SSID e senha do Wi-Fi
  - Tempo de solda (ms)
  - Ciclo (ms)
  - Contador de soldas

---

## 🏗️ Dependências

Instale as seguintes bibliotecas no Arduino IDE:

- **Adafruit GFX Library**
- **Adafruit SSD1331 OLED Driver**
- **RotaryEncoder**
- **ESP8266WiFi**
- **ESP8266HTTPUpdateServer**
- **ESP8266httpUpdate**
- **ESP8266WebServer**
- **DNSServer**
- **EEPROM**

---

## 🔗 Link do Projeto

[Repositório no GitHub →](https://github.com/JorgeBeserra/SoldaPonto)

---

## ✍️ Autor

- 🚀 **Jorge Souza**
- 🌐 [github.com/JorgeBeserra](https://github.com/JorgeBeserra)
