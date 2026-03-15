# Badge HackConRD 2026 — v2.1

Badge de conferencia hacker con firmware para **ESP32-C3**, diseñado para el evento HackConRD 2026. Fork del proyecto originalmente desarrollado por Junior Restituyo.

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

---

## Estructura del proyecto

```
HackCon_2026_v2_1/
├── HackCon_2026_v2_1.ino      # Firmware principal
├── games/
│   ├── Dino.h                  # Juego Dino Run
│   ├── Maze.h                  # Juego Maze Runner (raycaster)
│   └── HackerMon.h             # Juego Hackermon (estilo Pokémon)
├── img/
│   └── *.h                     # Imágenes del screensaver
└── README.md
```

---

## Capacidades

### Juegos

- **Maze Runner** — laberinto con motor de raycaster 3D tipo Doom, gemas coleccionables, minimapa, música y efectos de sonido
- **Dino Run** — runner infinito con dinosaurio, obstáculos, patos desde el aire y música de fondo (Imperial March)
- **Hackermon** — RPG de mundo abierto estilo Pokémon, con batallas por turnos, sistema de XP y múltiples mapas

#### Controles comunes para juegos

| Combo | Acción |
|---|---|
| DOWN + BACK (hold) | Salir del juego → menú de juegos |

#### Maze Runner

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

### Screensaver

- Imágenes animadas con efecto parallax, múltiples temas

### LEDs

- 12 LEDs WS2812B con temas de color por modo
- Brillo configurable, guardado en NVS

### Temas de UI

Cuatro temas de color seleccionables desde Settings: **Green** (default), **Red**, **Blue**, **White**.

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
| `dinohi` | High score del Dino Run |

---

## Compilar y flashear

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

## Changelog

### v2.1

- **Estructura**: archivos de juegos movidos a la carpeta `games/`
- **Fox Hunting**: color "cerca" del indicador RSSI fijado a verde neón (`0x07E0`) independiente del tema de UI
- **Maze / Dino / Hackermon**: combo de salida unificado a **DOWN + BACK** en los tres juegos
- **Maze**: footer del menú de pausa corregido para usar el componente estándar `uiFooter()`
- **Seguridad de memoria**: corregidos múltiples desbordamientos de buffer (`strcpy` → `strlcpy`, `strncpy` → `memcpy`, escritura fuera de bounds en nick)
- **HackerMon**: añadida validación de índice en `pkmAwardXP()` antes del bit-shift; bounds clamp en `pkmStartBattle()`
- **Maze raycaster**: clamp de `texY` para evitar lecturas fuera de la textura de pared; corrección de clipping de sprites de gemas; normalización periódica del vector de dirección para evitar deriva flotante
- **RF**: corregido over-read de `badgeNick` en `frfBuild()` (buffer de 8 bytes leído como 14)
- **Friends**: añadida validación de índice `friendsListIndex < friendsCount` antes de acceder a la lista
- **Dino**: eliminado campo `boosting` sin uso del struct `dinoSt`
