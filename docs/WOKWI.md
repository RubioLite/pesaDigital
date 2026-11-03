# Guía de Wokwi — pesaDigital

Esta guía cubre la simulación de la **Etapa 0** y deja preparado el camino
para la Etapa 1 (HX711). Léela completa antes de tocar `diagram.json`.

---

## 1. Análisis de compatibilidad

### 1.1 Resumen

| Elemento | ¿Funciona en Wokwi? | Observaciones |
|---|---|---|
| ESP32 DevKit V1 (30 pines) | Sí | Parte `wokwi-esp32-devkit-v1` |
| GPIO 16 / 17 (HX711) | Sí | Libres en WROOM-32 sin PSRAM; en Wokwi son `RX2` / `TX2` |
| HX711 + celda 5 kg | Sí, simplificado | `wokwi-hx711` con `"type": "5kg"` |
| Librería `bogde/HX711` | Sí | El modelo responde al protocolo, pero no reproduce todo el chip |
| OLED SSD1306 I2C | Sí | Parte `board-ssd1306`, dirección `0x3C` |
| Librería `U8g2` | Sí | I2C hardware; buffer completo `_F_` |
| Botones con rebote | Sí | Wokwi simula rebotes; el antirrebote de 25 ms los absorbe |
| Buzzer con `tone()` | Aproximado | Se ve/oye actividad, pero no evalúes tono ni volumen exactos |
| `Preferences` (NVS) | Compila | Persistencia entre reinicios del simulador **no garantizada** |
| Monitor Serial | Sí | 115200 baudios |

**No hace falta cambiar ningún pin.** El mapa confirmado funciona tal cual.

### 1.2 Limitaciones importantes del HX711 simulado

El modelo de Wokwi **no es un HX711 completo**:

- No implementa el canal B ni la selección de ganancia 32/64/128.
- No reproduce el valor de 24 bits del chip real. Para `"type": "5kg"` el
  rango crudo es **0–2100**; para `"50kg"`, 0–21000.
- No modela ruido analógico, deriva térmica, vibración ni no linealidad de la
  celda.
- El deslizador entrega una carga ideal en kg.

Consecuencias prácticas:

- En Wokwi, **1 kg ≈ 420 cuentas** (2100 / 5). En hardware real el fondo de
  escala ronda los millones de cuentas. **Nunca copies la calibración de
  Wokwi a la placa real.**
- La simulación sirve para validar lógica, estados, UI, tiempos y manejo de
  errores; la exactitud y el ruido se validan en banco.
- El código de la Etapa 1 no debe asumir un rango crudo concreto: debe
  calibrarse con `zeroCounts` y `countsPerGram`, igual en simulación y en real.

### 1.3 Alimentación: única diferencia deliberada

Los modelos de Wokwi documentan **VCC a 5 V** para el HX711 y para el OLED.
Por eso `diagram.json` conecta ambos a `esp:VIN` (5 V).

En la placa real:

- HX711 a **3.3 V** para que DT nunca entregue 5 V al ESP32 (no es tolerante).
- OLED a 3.3 V (el módulo suele aceptar 3.3–5 V; con 3.3 V es suficiente).
- Los pines de datos no cambian.

Si en el futuro se alimenta el HX711 a 5 V en hardware real, hay que nivelar
DT con un divisor o un level shifter. **No lo hagas sin revisar el esquema.**

### 1.4 NVS / Preferences

Wokwi ejecuta el binario real, así que `Preferences` compila y funciona dentro
de una sesión. Lo que **no** está garantizado es que la partición NVS sobreviva
a recargar el proyecto o a reiniciar el simulador.

Plan de validación:

1. En simulación, guardar calibración y llamar a `ESP.restart()` dentro de la
   misma sesión: debe conservarse.
2. En hardware real, hacer 10 ciclos de apagado/encendido: debe conservarse.
3. La prueba definitiva de NVS es en placa, no en Wokwi.

### 1.5 Diferencias que no se pueden simular

| Tema | Wokwi | Placa real |
|---|---|---|
| Resolución del ADC | 0–2100 (5 kg) | 24 bits, millones de cuentas |
| Ruido / EMI | No | Sí; afecta la estabilidad |
| Deriva térmica | No | Sí; requiere warm-up |
| Timing exacto del HX711 | Simplificado | 10/80 SPS reales |
| Pull-ups I2C | Implícitos | Módulo o 4.7 kΩ externos |
| Corriente del buzzer | No | Puede requerir transistor |
| Persistencia NVS | No garantizada | Garantizada por NVS |
| Protección mecánica | No | Topes obligatorios |

---

## 2. Archivos que participan

| Archivo | Función |
|---|---|
| `platformio.ini` | Compila el firmware para `esp32dev` |
| `wokwi.toml` | Indica a Wokwi qué `.bin` y `.elf` cargar |
| `diagram.json` | Circuito: ESP32, HX711, OLED, botones, LED, buzzer |
| `.pio/build/esp32dev/firmware.bin` | Firmware compilado (lo genera `pio run`) |

Wokwi **no compila** el proyecto de PlatformIO: solo carga el binario. Por eso
siempre hay que ejecutar `pio run` antes de arrancar la simulación.

---

## 3. Paso a paso

### (a) Crear el proyecto en Wokwi

#### Opción recomendada: extensión de VS Code

1. Instala **Wokwi for VS Code** desde el marketplace de extensiones.
2. Abre en VS Code la carpeta raíz `pesaDigital` (no un archivo suelto).
3. Comprueba que en la raíz están `platformio.ini`, `wokwi.toml` y
   `diagram.json`.
4. Compila:

   ```bash
   pio run -e esp32dev
   ```

5. Abre la paleta de comandos con `F1` y elige **Wokwi: Start Simulator**.

La extensión lee `wokwi.toml`, carga el firmware y muestra el circuito. La
extensión es gratuita para uso personal/educativo; para uso comercial revisa
las condiciones de Wokwi.

#### Alternativa: editor web

El editor web de Wokwi compila un `sketch.ino` único, que no encaja con un
proyecto PlatformIO multiarchivo. Para reutilizar este firmware:

1. Entra en <https://wokwi.com> y crea un proyecto ESP32.
2. Sustituye el contenido de la pestaña `diagram.json` por el de este repo.
3. Compila localmente con `pio run -e esp32dev`.
4. En el editor web, abre la paleta (`F1`) y usa la opción de **cargar un
   firmware ya compilado** (`.bin`) si está disponible en tu versión.
5. Selecciona `.pio/build/esp32dev/firmware.bin`.

Si esa opción no aparece, usa la extensión de VS Code: es el camino soportado
para proyectos PlatformIO.

### (b) Pegar `diagram.json`

En VS Code el archivo ya está en la raíz. Puedes:

- abrirlo como texto y revisarlo, o
- ejecutar `F1` → **Wokwi: Open Diagram Editor** para verlo gráficamente.

Si lo creas desde cero en el editor web:

1. Abre la pestaña `diagram.json`.
2. Selecciona todo (`Ctrl+A`) y pega el contenido completo del archivo.
3. Guarda. Deben aparecer el ESP32, el HX711, el OLED, tres pulsadores, el
   buzzer, la resistencia y el LED, con sus cables.
4. No cambies los `id` (`esp`, `hx711`, `oled`, `btnTare`, …): el firmware no
   depende de ellos, pero las conexiones sí.

### (c) Cargar el firmware

1. Compila **siempre** antes de simular:

   ```bash
   pio run -e esp32dev
   ```

2. Verifica que existen:

   ```text
   .pio/build/esp32dev/firmware.bin
   .pio/build/esp32dev/firmware.elf
   ```

3. `F1` → **Wokwi: Start Simulator**.
4. Abre el monitor Serial de Wokwi a **115200** baudios. Debes ver algo como:

   ```text
   0 I boot: pesaDigital v0.1.0 | ESP32 DevKit V1 | ...
   0 I cfg: HX711 DT=16 SCK=17 | I2C SDA=21 SCL=22
   0 I oled: SSD1306 128x64 en 0x3C, I2C 400000 Hz
   0 I boot: listo: 6 tareas periodicas
   ```

5. Tras cambiar código: detén la simulación, ejecuta `pio run` y vuelve a
   iniciarla. Wokwi no recompila solo.

### (d) Usar el deslizador de la celda de carga

**En la Etapa 0 el deslizador todavía no hace nada visible.** Los pines del
HX711 quedan en un estado seguro (SCK a nivel bajo, DT como entrada), pero el
chip no se lee; es intencional. Al seleccionar el HX711 en Wokwi aparece un
control de carga en kg. Puedes moverlo, pero el OLED y el Serial no cambiarán
porque la lectura llega en la Etapa 1.

Qué sí puedes probar hoy:

- Que el OLED muestra las tres páginas.
- Que MODO cambia de página y que MODO largo activa `diag`.
- Que TARA y CAL registran por Serial que su lógica llega en la Etapa 3.
- Que el LED late cada 2 s y el buzzer pita al pulsar.
- Que el sistema sigue vivo si desconectas el OLED (parpadeo rápido + reintento).
- Que `status`, `pins`, `beep`, `page` y `diag` funcionan por Serial.

**Preparación para la Etapa 1 (aún no disponible):** el deslizador del HX711
entregará la carga simulada. Para el modelo `"5kg"`:

```text
0.0 kg  -> ~0 cuentas
1.0 kg  -> ~420 cuentas
5.0 kg  -> ~2100 cuentas
```

En la Etapa 1 el firmware mostrará el valor crudo y el estado del sensor. En
la Etapa 3 se podrá hacer el flujo completo:

1. Deslizador a 0 kg (báscula vacía).
2. Pulsar TARA (o enviar el comando de tara).
3. Colocar 1 kg con el deslizador.
4. Iniciar calibración con masa de 1000 g y confirmar.
5. Verificar que la pantalla muestra aproximadamente 1000 g.

No compares esa cifra con la placa real: son escalas de cuentas distintas.

### Escenarios de automatización (opcional)

Wokwi expone el control `load` (kg) del HX711 en sus *Automation Scenarios*.
En la Etapa 1 se puede usar para pruebas repetibles de tara y calibración sin
mover el ratón. Consulta la documentación de Wokwi para el formato del
escenario.

---

## 4. Verificación de la Etapa 0

Marca cada punto en la simulación:

- [ ] El OLED muestra la pantalla de arranque y luego Estado.
- [ ] El Serial muestra el banner y las 6 tareas registradas.
- [ ] MODO corto recorre Estado → Pines → Acerca de → Estado.
- [ ] MODO largo activa `diag on` y aparecen logs cada segundo.
- [ ] TARA y CAL avisan de que su lógica es de la Etapa 3.
- [ ] El LED hace un pulso corto cada 2 s.
- [ ] El buzzer pita al pulsar (sube el volumen del componente si hace falta).
- [ ] `help`, `status`, `pins`, `beep`, `page 1`, `diag on` responden.
- [ ] Al desconectar SDA del OLED, el LED parpadea rápido y el Serial sigue
      vivo; al reconectarlo, la pantalla se recupera en ≤ 4 s.
- [ ] No hay reinicios ni bloqueos tras varios minutos.

---

## 5. Problemas frecuentes

| Síntoma | Causa probable | Solución |
|---|---|---|
| “Firmware not found” | No se compiló o la ruta no coincide | `pio run -e esp32dev`; revisa `wokwi.toml` |
| Pantalla negra | OLED no detectado / dirección incorrecta | Revisa `0x3C`, SDA=21, SCL=22 y VIN; mira el log `oled` |
| La pantalla se queda en el arranque | Tareas no registradas | Mira el Serial; revisa `sched` |
| Los botones no responden | Cable a GND o pin equivocado | Usa `1.l` al GPIO y `2.l` a `GND.2` |
| El buzzer no suena | Volumen bajo o audio del navegador bloqueado | Sube `volume` del componente y permite audio |
| El deslizador no cambia el peso | Es la Etapa 0: no se lee el HX711 | Esperado; se habilita en la Etapa 1 |
| Wokwi pide licencia | Modo offline o uso comercial | Usa el modo online o revisa la licencia |
| El puerto Serial muestra basura | Baudios incorrectos | 115200 en el monitor |
| Falla la descarga de `espressif32@8.0.0` | PlatformIO/red | Actualiza PlatformIO Core y reintenta |

---

## 6. Qué habrá que validar sí o sí en la placa real

Aunque todo funcione en Wokwi, esto no queda demostrado por la simulación:

1. **Nivel eléctrico de DT:** el HX711 debe alimentarse a 3.3 V o nivelar DT.
2. **Resolución y ruido reales:** cuentas de 24 bits, ruido pico-pico en g.
3. **Timing real:** 10 SPS, `is_ready()`, recuperación tras desconexión.
4. **Calibración real:** cero, factor, linealidad, repetibilidad, ±2 g.
5. **Persistencia NVS:** 10 ciclos de apagado/encendido.
6. **I2C real:** pull-ups, longitud de cable, dirección 0x3C/0x3D.
7. **Botones físicos:** rebote real y sensación de pulsación larga.
8. **Buzzer:** corriente, transistor si hace falta, volumen.
9. **Deriva térmica y creep:** prueba de 24 h.
10. **Protección mecánica:** topes; el software no protege la celda.

---

## 7. Cambios respecto al diseño inicial

No se cambió ningún GPIO. Solo se documentan estas adaptaciones de simulación:

- HX711 y OLED a **5 V (`VIN`)** en Wokwi; en real, **3.3 V**.
- El HX711 de Wokwi es un modelo simplificado (0–2100 cuentas para 5 kg).
- La persistencia NVS se valida en placa, no en el simulador.
