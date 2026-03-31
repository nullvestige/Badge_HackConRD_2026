# Badge HackConRD 2026 — v1.0

Firmware del Badge de la conferencia de hackers HackConRD 2026 desarrollado por Junior Restituyo.

---

## Hardware

| Componente | Detalles |
|---|---|
| MCU | ESP32-C3 — RISC-V monohilo ~160 MHz, sin FPU por hardware, ~400 KB RAM |
| Pantalla | Adafruit ST7789, TFT 240×240, RGB565 |
| Radio | CC1101 @ 433 MHz (comparte bus SPI con la pantalla) |
| LEDs | 12× WS2812B (FastLED, pin 20) |
| Buzzer | Pasivo (FUET 5V), pin 21 |
| Botones | 4 botones — ARRIBA (UP), ABAJO (DOWN), VOLVER (BACK), OK |

Para más detalles sobre los pines y librerías requeridas, consultar el documento **[HackConRD Badge - Guia Pines y Librerias.pdf](HackConRD%20Badge%20-%20Guia%20Pines%20y%20Librerias.pdf)** incluido en este repositorio.

---

## Estructura del proyecto

```
Badge_HackCon_2026/
├── Badge_HackCon_2026.ino           # Firmware principal
├── games/
│   ├── Dino.h                  # Juego Dino Run
│   ├── Maze.h                  # Juego Doom Maze
│   ├── HackerMon.h             # Juego Hackermon
│   └── StarInvaders.h         # Juego Star Invaders
├── img/
│   └── *.h                     # Imágenes del screensaver (22 imágenes)
└── README.md
```

---

## Librerías requeridas

Instalar desde el **Gestor de Librerías** de Arduino IDE (`Sketch → Include Library → Manage Libraries`):

| Librería |
|---|
| Adafruit GFX Library by Adafruit |
| Adafruit ST7789 and ST7735 Library by Adafruit |
| FastLED by Daniel Garcia |
| QRCodeGenerator by Felix Erdmann |
| Preferences (incluida con el core ESP32, no requiere instalación manual) |

### Board (placa)

1. Ir a `Tools → Board → Boards Manager`, buscar **ESP32** e instalar **esp32 by Espressif Systems**.
2. Seleccionar la placa: `Tools → Board → ESP32 Arduino → ESP32C3 Dev Module`.

---

## Flashear con Arduino IDE

### Configuración de Tools

| Opción | Valor |
|---|---|
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | **Enabled** (importante para monitor serial y upload sin problemas) |
| CPU Frequency | 160 MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Huge APP 3MB No OTA/ 1MB SPIFFS |
| Upload Speed | 921600 |
| Port | COMx (el puerto que aparezca al conectar el badge) |

### Pasos para subir el firmware

1. Conectar el badge por USB.
2. Verificar que el puerto aparezca en `Tools → Port`. Si no aparece, instalar los drivers CH340 o CP210x según el chip USB de tu badge.
3. Compilar con `Sketch → Verify/Compile` (Ctrl+R) para confirmar que no hay errores.
4. Subir con `Sketch → Upload` (Ctrl+U).
5. Si el upload falla, mantener presionado el botón BOOT del ESP32-C3 mientras se inicia la carga y soltar cuando aparezca *Connecting...*.

> **Nota:** Si usas un ESP32-C3 con USB nativo, asegúrate de tener **USB CDC On Boot: Enabled**. Sin esto el monitor serial puede no funcionar y el upload puede fallar.

---

## Flashear con arduino-cli

Proyecto **Arduino** para `esp32:esp32:esp32c3`.

```sh
# Compilar
arduino-cli compile --fqbn esp32:esp32:esp32c3 "custom badge"

# Flashear
arduino-cli upload -p COM<N> --fqbn esp32:esp32:esp32c3 "custom badge"

# Monitor serial
arduino-cli monitor -p COM<N> --config baudrate=115200
```

---

## Capacidades

### Juegos

- **Doom Maze** — laberinto con motor de raycaster 3D tipo Doom, gemas, minimapa, música y efectos de sonido
- **Dino Run** — corredor infinito con dinosaurio, obstáculos, patos desde el aire y música de fondo (Marcha Imperial)
- **Hackermon** — RPG de mundo abierto, con batallas por turnos, sistema de XP, múltiples mapas, sistema de rango y pantalla de título con mejor rango persistido
- **Star Invaders** — clásico arcade de invasores espaciales

#### Controles comunes para juegos

| Combo | Acción |
|---|---|
| DOWN + BACK (hold) | Salir del juego → menú de juegos |

#### Doom Maze

| Botón | Acción |
|---|---|
| UP | Avanzar |
| DOWN | Retroceder |
| OK | Girar derecha |
| BACK | Girar izquierda |
| DOWN + BACK | Salir al menú |

#### Dino Run

| Botón | Acción |
|---|---|
| UP | Saltar |
| DOWN (hold) | Agacharse |
| DOWN + BACK | Salir al menú |

#### Hackermon

| Botón/Combo | Acción |
|---|---|
| UP / DOWN / BACK / OK | Moverse en el mapa |
| DOWN + BACK | Salir al menú |

#### Star Invaders

| Botón/Combo | Acción |
|---|---|
| UP | Mover nave izquierda |
| DOWN | Mover nave derecha |
| OK | Disparar |
| OK (título / game over) | Iniciar / Reintentar |
| BACK (título / game over) | Volver al menú de juegos |
| DOWN + BACK (hold) | Salir al menú durante el juego |

---

### Sistema RF (433 MHz)

- **Fox Hunting** — caza del zorro: mide RSSI en tiempo real con barra de señal tricolor (azul = lejos, verde = cerca, rojo = muy cerca). El verde de "cerca" está fijo en verde neón independientemente del tema de UI.
- **Sistema de amigos (Friends RF)** — intercambio peer-to-peer de perfiles por RF; guarda historial en memoria no volátil

### Módulos de aprendizaje

- Contenido educativo interactivo navegable desde el menú principal

### Perfil de usuario

- Nick de 7 caracteres
- Rol de badge (BadgeRole)
- Persistencia en NVS bajo el namespace `"hackcon"`

### Protector de pantalla

- **21 imágenes disponibles**

### LEDs

- 12 LEDs WS2812B con temas de color por modo
- Brillo configurable, guardado en NVS

### Temas de UI

Cuatro temas de color seleccionables desde Configuración: **Verde** (predeterminado), **Rojo**, **Azul**, **Blanco**.

---

## Configuración persistente (NVS)

| Clave | Descripción |
|---|---|
| `nick` | Apodo del portador |
| `bro` | Contador de amigos encontrados |
| `ledbr` | Brillo de LEDs |
| `rfprof` | Perfil de radio |
| `foxf100` | Frecuencia del fox × 100 |
| `buzz` | Buzzer habilitado/deshabilitado |
| `uitheme` | Tema de UI |
| `keyvrf` | Clave para "The Key" |
| `frhist` | Historial de amigos RF |
| `questal` | Estado de quest/logros |
| `dinohi` | Puntuación máxima del Dino Run |
| `pkm_rank` | Mejor rango alcanzado en Hackermon (0–5) |

---

## Gestión de imágenes del protector de pantalla

Las imágenes se almacenan como arrays de bytes en archivos `.h` dentro de la carpeta `img/`. Actualmente hay **21 imágenes**. A continuación se explica cómo agregar o eliminar imágenes.

### Agregar una imagen nueva

#### 1. Preparar la imagen

- La imagen debe ser **cuadrada** (la pantalla es 240×240).
- Preferiblemente **comprimida** (JPG con calidad media) para reducir el tamaño del archivo `.h` resultante y no saturar la flash del ESP32.

#### 2. Convertir a C Hex

1. Ir a [https://www.digole.com/tools/PicturetoC_Hex_converter.php](https://www.digole.com/tools/PicturetoC_Hex_converter.php)
2. Seleccionar la imagen con el botón *Choose File*.
3. En **Image Width** poner `240` y en **Image Height** poner `240` (*resize to 240x240*).
4. En **Used for** seleccionar **65K Color**.
5. No tocar ninguna otra opción.
6. Presionar el botón de conversión y esperar a que genere el código.
7. **Copiar todo el contenido generado** (solo los datos hex, sin incluir ninguna declaración de variable).

#### 3. Crear el archivo `.h`

Crear un archivo nuevo en la carpeta `img/` con nombre descriptivo, por ejemplo `img/minombre_saver.h`, y pegar ahí el contenido copiado. El archivo debe quedar solo con los datos hex, sin declaración de array.

#### 4. Registrar la imagen en el firmware

Abrir `Badge_HackConRD_2026.ino` y hacer dos cambios:

**a)** Alrededor de la línea 866, agregar la declaración del array junto a las demás (respetar el mismo patrón):

```cpp
static const uint8_t saverBits_minombre[] PROGMEM = {
#include "img/minombre_saver.h"
};
```

**b)** En el array `SAVERS[]` (alrededor de la línea 940), agregar una entrada al final:

```cpp
{ "Mi Nombre",  saverBits_minombre,  false, false },
```

> Los campos son: `nombre visible en menú`, `array de datos`, `showOverlay` (mostrar nick/rol encima), `isMono` (imagen monocromática). Para imágenes a color usar `false, false`.

---

### Eliminar una imagen existente

Tomando como ejemplo que se quiere eliminar **Bender** (`bender_saver.h`):

#### 1. Borrar el archivo `.h`

Eliminar el archivo `img/bender_saver.h` de la carpeta `img/`.

#### 2. Eliminar el bloque PROGMEM en el `.ino`

Buscar en `Badge_HackConRD_2026.ino` el bloque que declara esa imagen (alrededor de la línea 887) y eliminarlo completo:

```cpp
// Eliminar estas líneas:
static const uint8_t saverBits_bender[] PROGMEM = {
#include "img/bender_saver.h"
};
```

#### 3. Eliminar la entrada del array `SAVERS[]`

Buscar dentro del array `SAVERS[]` (alrededor de la línea 948) la línea correspondiente y eliminarla:

```cpp
// Eliminar esta línea:
{ "Bender",     saverBits_bender,     false, false },
```

> **Importante:** el firmware calcula `SAVER_COUNT` automáticamente con `sizeof(SAVERS)`, por lo que no hay ningún contador que actualizar manualmente. Si se deja el `#include` del `.h` pero se borró el archivo, el proyecto no compilará.
