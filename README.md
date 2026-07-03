# 🔥 SoldaPonto ESP8266

Sistema de controle para **máquina de solda ponto** desenvolvido em ESP8266, com display OLED colorido, configuração via Wi-Fi, atualização OTA e encoder rotativo para ajuste fino dos parâmetros.

---

## 🚀 Funcionalidades

- ✅ Controle de ciclo e tempo da solda em milissegundos
- ✅ Interface gráfica no display OLED SSD1331
- ✅ Encoder rotativo com botão:
  - Clique curto: alterna entre **edição de ciclo** e **tempo**
  - Pressione e segure por 5s: verifica atualização OTA direto do GitHub
- ✅ Contador de soldas realizadas salvo na EEPROM
- ✅ Portal de configuração via Wi-Fi em modo AP
- ✅ Atualização de firmware OTA via GitHub Releases
- ✅ Watchdog integrado para travamentos
- ✅ Configurações salvas na EEPROM

---

## 📸 Interface Display OLED

Mostra:

- Ciclo (`cl`)
- Tempo (`ms`)
- Status do Wi-Fi (`WiFi OK` ou `X`)
- Contador de soldas (`S:123`)

Durante edição, o valor pisca indicando qual parâmetro está sendo alterado.

---

## 🕹️ Controles via Encoder

| Ação | Resultado |
| --- | --- |
| Girar | Aumenta ou diminui o valor selecionado |
| Clique curto | Alterna entre **Ciclo → Tempo → Normal** |
| Clique longo, 5s | Verifica e executa atualização OTA |

---

## ⚙️ Pinagem

| Função | Pino ESP8266 | Descrição |
| --- | --- | --- |
| Trigger | D6 / GPIO12 | Botão de acionamento da solda |
| Triac / SSR | RX / GPIO3 | Saída para acionar o Triac ou relé |
| Encoder CLK | D3 / GPIO0 | Encoder sinal CLK |
| Encoder DT | D4 / GPIO2 | Encoder sinal DT |
| Encoder SW | D0 / GPIO16 | Botão do encoder |
| OLED SCK | D7 / GPIO13 | Display SPI Clock |
| OLED MOSI | D5 / GPIO14 | Display SPI Data |
| OLED CS | D8 / GPIO15 | Chip Select |
| OLED RES | D1 / GPIO5 | Reset do display |
| OLED DC | D2 / GPIO4 | Comando/Data |

---

## ⚠️ Importante sobre o Encoder

Se ao girar o encoder a contagem estiver **invertida** — direita diminui e esquerda aumenta — basta trocar fisicamente os fios dos pinos CLK e DT ou inverter a ordem dos pinos no código:

```cpp
encoder = new RotaryEncoder(pin_Encoder_DT, pin_Encoder_CLK, RotaryEncoder::LatchMode::TWO03);
```

---

## 🌐 Portal Wi-Fi

Para entrar no modo de configuração:

1. Segure o botão do encoder durante a energização.
2. Acesse a rede Wi-Fi:

```text
SoldaPontoConfig
```

3. Abra no navegador:

```text
http://192.168.4.1
```

No portal é possível:

- Configurar rede Wi-Fi
- Acessar atualização OTA manual
- Resetar Wi-Fi
- Resetar EEPROM

---

## 🔥 Atualização OTA Automática

Toda vez que conecta no Wi-Fi, o firmware verifica se existe versão nova no GitHub Releases:

```text
https://github.com/JorgeBeserra/SoldaPonto/releases
```

Também é possível verificar manualmente segurando o botão do encoder por 5 segundos.

---

## 💾 Salvamento na EEPROM

Dados armazenados:

- SSID e senha do Wi-Fi
- Tempo de solda
- Ciclo
- Contador de soldas
- Byte de validação da EEPROM

---

## 🏗️ Dependências

Instale as seguintes bibliotecas no Arduino IDE:

- Adafruit GFX Library
- Adafruit SSD1331 OLED Driver
- RotaryEncoder
- ESP8266WiFi
- ESP8266WebServer
- ESP8266HTTPUpdateServer
- ESP8266HTTPClient
- ESP8266httpUpdate
- WiFiClientSecureBearSSL
- DNSServer
- EEPROM

---

## 🔗 Link do Projeto

[Repositório no GitHub →](https://github.com/JorgeBeserra/SoldaPonto)

---

## ✍️ Autor

- 🚀 Jorge Souza
- 🌐 [github.com/JorgeBeserra](https://github.com/JorgeBeserra)
