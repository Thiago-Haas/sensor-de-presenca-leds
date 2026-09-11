# Sensor De Presença LEDS

Projeto PlatformIO para ESP32 DOIT DevKit V1, usando Arduino e HC-SR04.
Acende um LED quando detecta um objeto entre 2 e 80 cm e mantém o LED aceso
até 2 segundos após a última detecção. Funciona para passagem em ambos os sentidos,
sem contar pessoas nem classificar entrada ou saída. O sensor mede distância:
objetos, a própria porta e paredes próximas também podem acioná-lo.

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

## Ajustar à porta

Em `src/main.cpp`, ajuste `kDetectionDistanceCm` (inicialmente 80 cm) e
`kLedHoldMs` (inicialmente 2000 ms). Posicione o sensor voltado para a área de
passagem e escolha um limite menor que a distância até o obstáculo fixo ao fundo.
Confira as distâncias no monitor serial com a passagem livre e durante uma passagem.
Leituras inválidas não renovam o tempo do LED. Uma pessoa parada na área mantém
o LED aceso enquanto houver leituras dentro do limite.

Teste na bancada: passagem livre deixa o LED apagado; uma mão a 20–50 cm acende
o LED; ao remover a mão, ele apaga após cerca de 2 segundos. Depois verifique
passagens nos dois sentidos e ajuste a distância para a instalação real.

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
