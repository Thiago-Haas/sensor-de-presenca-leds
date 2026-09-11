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
- Wi-Fi: preencha `WIFI_SSID` e `WIFI_PASSWORD` no arquivo `.env` na raiz.
  Use `.env.example` como modelo; `.env` é ignorado pelo Git.
- `MQTT_HOST`: IP do computador com o broker, nunca `localhost` na ESP32.
- `MQTT_PORT`, `MQTT_USER` e `MQTT_PASSWORD`: configuração do broker.
- `DEVICE_ID`: identificador único com letras, números e hífen; padrão `porta-01`.
- `MIN_HEIGHT_CM`: limite inferior de detecção, inicialmente 50 cm.

`sensor_config.h` é ignorado pelo Git. Sem ele, compila com altura de 216 cm e broker não configurado. Com altura zero,
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

## Tutorial rápido: Node-RED e dashboard

**Já configurado nesta máquina:** abra o [dashboard de alturas](http://localhost:1880/dashboard/alturas).
O [editor do Node-RED](http://localhost:1880) permite editar o fluxo.
Passe sob o sensor e libere a passagem: o registro aparece quando o sensor volta
a detectar o chão. Estar conectado ao MQTT, sozinho, não gera uma medição.

### Instalar em outra máquina (Ubuntu/Debian)

1. Instale o VS Code com PlatformIO IDE e uma versão do Node.js compatível com
   o [Node-RED](https://nodered.org/docs/getting-started/local).
2. Instale o broker e o Node-RED:

   ```sh
   sudo apt update
   sudo apt install mosquitto mosquitto-clients
   sudo systemctl enable --now mosquitto
   sudo npm install -g node-red
   node-red
   ```

   Mantenha esse terminal aberto. Nesta máquina, o Node-RED já roda como serviço;
   não é necessário iniciar outra instância.
3. Abra <http://localhost:1880>. No menu **Manage palette → Install**, instale
   `@flowfuse/node-red-dashboard` ([guia oficial](https://dashboard.flowfuse.com/getting-started.html)).
4. Use **Import → select a file** e selecione `dashboard/flows-local.json`.
   No nó MQTT, confirme `127.0.0.1`, porta `1883`. No nó de arquivo, ajuste
   `/home/haas/.node-red/data/alturas.jsonl` para uma pasta gravável do seu usuário.
   Clique em **Deploy**. Se o fluxo já estiver instalado, não importe novamente.
5. Abra <http://localhost:1880/dashboard/alturas> para ver conexão, gráfico e histórico.

### Conectar a ESP32

Na primeira configuração, copie `.env.example` para `.env` e
`include/sensor_config.example.h` para `include/sensor_config.h`.
Preencha o Wi-Fi no `.env` e o IP do computador em `MQTT_HOST`; mantenha
`SENSOR_HEIGHT_CM = 216.0f` ou ajuste à instalação. Ambos os arquivos locais
são ignorados pelo Git. Compile e grave pelo PlatformIO.

O Mosquitto precisa aceitar conexões da rede local. Para **esta máquina**, cujo
IP configurado é `10.243.75.195`, execute uma vez, na raiz do projeto:

```sh
sudo bash dashboard/habilitar-mqtt-lan.sh
```

Se já estiver conectado, não precisa repetir. O script aceita mudanças de IP da rede; atualize `MQTT_HOST` na ESP32
quando o endereço do notebook mudar. O protótipo usa MQTT sem senha/TLS em rede confiável.

**Sem dados?** Confira se os nós MQTT estão conectados, se o tópico é
`porta/porta-01/altura` e se o sensor está apontado para o chão. O monitor serial
mostra o JSON ao concluir uma passagem válida. Para acompanhar o broker:

```sh
mosquitto_sub -h localhost -t 'porta/porta-01/#' -v
```

O histórico fica em `/home/haas/.node-red/data/alturas.jsonl`. Gráfico e tabela
reiniciam com o Node-RED; o arquivo permanece, mas não é recarregado no painel.
Os horários mostrados são de recebimento no computador.

Alternativa com Docker: use `docker compose up -d --build` dentro de `dashboard/`,
em uma máquina sem os serviços locais ocupando as portas 1880 e 1883.

## Validação na bancada

1. Configure a altura e observe a distância ao chão no monitor serial.
2. Passe um objeto de altura conhecida e confirme a estimativa após liberar o chão.
3. Verifique que ficar parado gera só um registro ao sair e o LED permanece aceso.
4. Faça passagens nos dois sentidos e compare com medidas manuais.
5. Desconecte a rede e confira a continuidade do LED; reconecte e verifique a fila.
6. Confira o dashboard e exporte o JSONL para verificar os registros.

A compilação e o envio MQTT ao histórico foram verificados. Confira a precisão
das alturas na instalação real com uma medida de referência.

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

## Wi-Fi em .env

O PlatformIO lê `.env` antes de compilar e gera um cabeçalho em `.pio/`,
também ignorado pelo Git. Sem `.env`, SSID e senha ficam vazios.
Valores podem usar aspas simples ou duplas; são literais, sem interpolação.
Depois de alterar o Wi-Fi, compile e grave novamente. As credenciais são
incorporadas ao firmware, mas não são adicionadas aos arquivos versionados.

## Check de vida

O painel mostra **Vivo** ao receber `porta/porta-01/heartbeat`, enviado pela
ESP32 a cada 5 segundos. Após 15 segundos sem sinal, mostra **Sem resposta**;
o aviso MQTT de desconexão mostra **Offline**. O sinal também informa se houve
eco válido nos últimos 2 segundos. Eco válido não garante precisão da altura.

Grave o firmware atualizado para ativar o check. Até receber o primeiro sinal,
o painel mostra **Aguardando sinal de vida**, mesmo com MQTT conectado.

## Calibrar com o botão D19

Ligue um botão momentâneo entre **D19 e GND** (pull-up interno, sem aplicar 5 V).
Com o sensor no alto apontado para baixo e a passagem livre, pressione e solte.
A ESP32 começa a coletar 15 leituras estáveis assim que o botão é pressionado —
a altura calibrada corresponde a esse instante; mantenha a passagem livre por
cerca de 3 segundos. Durante esse processo, a detecção de passagens fica pausada.

O painel mostra **Distância sensor → chão**, o valor em cm e o resultado da
calibração. O valor é salvo na ESP32 e restaurado após reiniciar; substitui os
216 cm do parâmetro inicial. Para mudar novamente, pressione o botão com o chão livre.
Se não houver leituras suficientes em 5 segundos, elas variarem mais de 3 cm ou
falhar a gravação, o valor anterior é mantido. Um objeto parado sob o sensor pode
ser confundido com o chão: sempre deixe a passagem livre.

O tópico `porta/porta-01/calibration` publica o último valor com retenção MQTT;
consulte também o check de vida para saber se a placa está respondendo agora.
Grave o firmware atualizado antes de usar o botão.

Teste da lógica de calibração (no computador com g++):

```sh
g++ -std=c++11 -Iinclude tests/test_calibration.cpp -o /tmp/test_calibration
/tmp/test_calibration
```

## Rede do celular e recalibrações

Notebook e ESP32 devem alcançar o mesmo broker: no firmware, `MQTT_HOST` é o
IPv4 Wi-Fi do notebook (atualmente `10.243.75.195`), e no Node-RED é `127.0.0.1`.
Se trocar de rede, confira `hostname -I` e atualize o IP no firmware. O comando
`sudo bash dashboard/habilitar-mqtt-lan.sh` libera a porta 1883 para a rede local
sem depender de um IP fixo. Não basta o broker escutar apenas em `127.0.0.1`.

No monitor serial, mensagens `[WiFi]` e `[MQTT]` indicam em qual etapa a conexão
falhou; “Sinal de vida enviado” confirma o envio pelo firmware.

**Pode recalibrar quantas vezes precisar:** pressione e solte D19, espere cerca
de 3 segundos com a passagem livre e confira “calibração salva” no dashboard.
Cada nova calibração válida substitui a anterior. Segurar o botão não repete a
calibração; toques durante uma calibração em andamento são ignorados.
