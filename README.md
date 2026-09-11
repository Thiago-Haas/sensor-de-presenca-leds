# Sensor De Presença LEDS

Projeto PlatformIO para ESP32 DOIT DevKit V1, Arduino, HC-SR04 e MQTT.
LED D18, Echo D2 e Trigger D4. Registra uma **estimativa de altura** por passagem,
sem contagem de pessoas nem identificação do sentido de deslocamento.

## Instalação para medir altura

Monte o sensor acima da porta, horizontal, com os transdutores apontados para o
chão. Meça a distância da face dos transdutores ao chão em centímetros.
A estimativa é `altura do sensor - distância até o topo da pessoa`.
Exemplo: sensor a 220 cm e distância de 45 cm resultam em aproximadamente 175 cm.
Esse exemplo não é a calibração da sua instalação.

Cabelo, chapéu, postura, reflexos, velocidade e posição lateral afetam a medição.
Não é um instrumento antropométrico: valide com pessoas de altura conhecida.
Uma passagem muito rápida pode não gerar leituras suficientes. Pessoas juntas
podem gerar um único registro. O sensor não distingue pessoas de outros objetos.

## Configuração local

Copie `include/sensor_config.example.h` para `include/sensor_config.h` e configure:

- `SENSOR_HEIGHT_CM`: distância real ao chão, inicialmente **216 cm**, editável; zero desativa alturas.
- `WIFI_SSID` e `WIFI_PASSWORD`: rede Wi-Fi de 2,4 GHz.
- `MQTT_HOST`: IP do computador com o broker, nunca `localhost` na ESP32.
- `MQTT_PORT`, `MQTT_USER` e `MQTT_PASSWORD`: configuração do broker.
- `DEVICE_ID`: identificador único com letras, números e hífen; padrão `porta-01`.
- `MIN_HEIGHT_CM`: limite inferior de detecção, inicialmente 50 cm.

`sensor_config.h` é ignorado pelo Git. Sem ele, compila com altura de 216 cm e rede desativada. Com altura zero,
o LED mantém o comportamento simples de detectar até 80 cm.

## Ligações

Faça as ligações com a alimentação desligada. Os nomes D18, D2 e D4 correspondem
aos GPIOs 18, 2 e 4 da placa.

| Componente | Ligação |
| --- | --- |
| LED comum, ânodo (+) | D18 através de resistor de 330 ohms |
| LED, cátodo (-) | GND |
| HC-SR04 VCC | Alimentação de 5 V |
| HC-SR04 GND | GND comum à ESP32 |
| HC-SR04 Trig | D4 |
| HC-SR04 Echo | D2 através do divisor de tensão abaixo |

**Não conecte o Echo de 5 V diretamente ao D2.** Os GPIOs da ESP32 não toleram
5 V. Use um divisor resistivo (resistores de 1%):

```text
HC-SR04 Echo ---- resistor 1 kohm ----+---- D2 (GPIO 2)
                                    |
                              resistor 2 kohms
                                    |
                                   GND
```

O divisor reduz 5 V para aproximadamente 3,33 V. Use uma alimentação de 5 V
adequada para o sensor, com GND comum. Se usar o pino VIN/5V da DevKit alimentada
por USB, confirme a identificação e disponibilidade de 5 V na sua variante.
Nunca aplique 5 V ao pino 3V3. A saída D18 é para um LED indicador comum com resistor;
fitas ou LEDs de potência precisam de circuito de acionamento próprio.

GPIO 2 participa da seleção de boot da ESP32. Se houver dificuldade para gravar,
desligue a alimentação, desconecte temporariamente o Echo do D2 e tente novamente;
reconecte com a alimentação desligada após a gravação.

## Compilar e gravar

Abra esta pasta no VS Code com a extensão PlatformIO IDE. Use **Build**,
**Upload** e **Monitor**, ou execute no terminal do PlatformIO:

```sh
pio run
pio run --target upload
pio device monitor
```

O monitor usa 115200 baud. Use cabo USB de dados. Se necessário, selecione a porta
com `upload_port` e `monitor_port` no `platformio.ini`.

## Medição e MQTT

A leitura ocorre a cada 70 ms com timeout de 30 ms. A mediana de três leituras
reduz picos isolados. O firmware guarda a maior altura filtrada da passagem;
exige pelo menos três amostras de presença e quatro leituras de chão (tolerância
15 cm) para emitir o registro. Perda de eco por 2 segundos descarta a passagem.
O LED fica aceso até 2 segundos depois da última presença detectada.

A conexão usa uma tarefa separada para não interromper a leitura durante
reconexões. Há fila em RAM de 20 registros, mais um em envio. Fila cheia descarta
novos registros com aviso serial; reiniciar perde a fila. Publicação QoS 0:
não há garantia de entrega. Eventos não são retidos; o estado usa retained/LWT.
O protótipo usa MQTT sem TLS apenas em rede local confiável.

| Tópico padrão | Conteúdo |
| --- | --- |
| `porta/porta-01/altura` | JSON com `event_id`, `height_cm`, `sensor_height_cm`, `uptime_ms`, `duration_ms`, `estimated` |
| `porta/porta-01/status` | `online` / `offline` (conexão MQTT, não saúde do sensor) |

`uptime_ms` é tempo desde o boot, não data/hora. O dashboard acrescenta
`received_at` em UTC; registros enviados após reconexão terão horário de
recebimento posterior à passagem real.

## Dashboard instalado nesta máquina

Painel: <http://localhost:1880/dashboard/alturas>.
Editor: <http://localhost:1880> (aba Alturas da porta).
FlowFuse Dashboard 1.31.0 instalado e fluxo ativado no Node-RED local.
Broker do Node-RED: `127.0.0.1:1883`. Histórico:
`/home/haas/.node-red/data/alturas.jsonl`.
Exportação adaptada: `dashboard/flows-local.json`.

O teste MQTT → Node-RED → histórico foi concluído; o registro simulado foi removido.
Para permitir que a ESP32 conecte pela rede, execute uma vez:

```sh
sudo bash /home/haas/Documents/GitHub/sensor-de-presenca-leds/dashboard/habilitar-mqtt-lan.sh
```

Esse comando configura o Mosquitto no endereço local `10.120.34.240:1883`,
mantém localhost e reinicia o serviço. Usa acesso sem senha na rede de protótipo.
Se o IP da máquina mudar, atualize a configuração do Mosquitto e a ESP32.
O arquivo ignorado `include/sensor_config.h` foi criado com esse IP e 216 cm;
preencha Wi-Fi e grave o firmware para receber medições reais.

## Dashboard local: Mosquitto + Node-RED

Com Docker Engine e Compose instalados no computador, execute:

```sh
cd dashboard
docker compose up -d --build
```

Abra <http://localhost:1880/dashboard/alturas>. O painel mostra conexão, gráfico
e as últimas 100 alturas recebidas. O histórico completo é acrescentado ao
arquivo `/data/alturas.jsonl` no volume persistente do Node-RED. Para exportar:

```sh
docker compose cp nodered:/data/alturas.jsonl ./alturas.jsonl
```

Gráfico e tabela usam memória e são reiniciados junto com o Node-RED; o arquivo
permanece no volume, mas não é recarregado automaticamente no painel.
Não use `docker compose down -v` se quiser preservar os registros.

O editor e o painel ficam acessíveis apenas no próprio computador. O broker
escuta na porta 1883 da rede local, sem senha, para o protótipo. Não exponha
essa porta à internet. Para usar um broker existente, configure as credenciais
na ESP32 e no nó MQTT do Node-RED. O fluxo usa `porta-01`; ajuste os dois tópicos
se alterar `DEVICE_ID`. Também é possível importar `dashboard/flows.json` em
Node-RED existente com `@flowfuse/node-red-dashboard` instalado.

## Validação na bancada

1. Configure a altura e observe a distância ao chão no monitor serial.
2. Passe um objeto de altura conhecida e confirme a estimativa após liberar o chão.
3. Verifique que ficar parado gera só um registro ao sair e o LED permanece aceso.
4. Faça passagens nos dois sentidos e compare com medidas manuais.
5. Desconecte a rede e confira a continuidade do LED; reconecte e verifique a fila.
6. Confira o dashboard e exporte o JSONL para verificar os registros.

O projeto foi preparado sem gravar a placa. O funcionamento físico e o dashboard
em execução precisam ser validados na instalação real.

## Git e GitHub

Repositório local com branch `main`. O repositório remoto desejado é
`sensor-de-presenca-leds`, **privado**. Para publicar, crie um repositório privado
vazio em <https://github.com/new> e execute nesta pasta:

```sh
git remote add origin https://github.com/SEU_USUARIO/sensor-de-presenca-leds.git
git push -u origin main
```

Substitua `SEU_USUARIO` pelo login do GitHub. A publicação requer autenticação.

## Referências

- [Placa no PlatformIO](https://docs.platformio.org/en/latest/boards/espressif32/esp32doit-devkit-v1.html)
- [HC-SR04 de 5 V e datasheet](https://www.sparkfun.com/ultrasonic-distance-sensor-hc-sr04.html)
- [Limites dos GPIOs da ESP32](https://docs.espressif.com/projects/esp-faq/en/latest/hardware-related/hardware-design.html)
- [ESP32: configuração de boot](https://documentation.espressif.com/esp32_datasheet_en.html)

- [PubSubClient](https://pubsubclient.knolleary.net/api)
- [FlowFuse Dashboard](https://dashboard.flowfuse.com/getting-started)
- [Node-RED com Docker](https://nodered.org/docs/getting-started/docker)
