# Badge HackConRD 2026

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
| Botones | 4 botones — ARRIBA, ABAJO, VOLVER, OK |

---

## Capacidades

### Juegos

- **Maze Runner** — juego de laberinto con motor de raycaster 3D, música no bloqueante y efectos de sonido
- **Dino Run** — corre con el dinosaurio, esquiva obstáculos, aves desde el inicio y música de fondo (Imperial March)
- **GTA** — juego estilo GTA top-down
- **Pokémon** — juego estilo Pokémon
- **Race** — juego de carreras

### Sistema RF (433 MHz)

- **Fox Hunting** — caza del zorro por radiofrecuencia: mide RSSI en tiempo real para localizar una baliza oculta
- **Sistema de amigos (Friends RF)** — intercambio peer-to-peer de perfiles de usuario por RF; guarda historial de encuentros en memoria no volátil

### Módulos de aprendizaje

- Contenido educativo interactivo navegable desde el menú principal

### Perfil de usuario

- Nick de 7 caracteres
- Rol de badge (BadgeRole)
- Persistencia en NVS (memoria flash no volátil) bajo el namespace `"hackcon"`

### Screensaver

- GIF animado con efecto parallax

### LEDs

- 12 LEDs WS2812B con temas de color por modo
- Brillo configurable, guardado en NVS

### Configuración persistente (NVS)

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

---

## Compilar y flashear

Proyecto **Arduino/PlatformIO** para `esp32:esp32:esp32c3`.

```sh
# Compilar
arduino-cli compile --fqbn esp32:esp32:esp32c3 "custom badge"

# Flashear
arduino-cli upload -p COM<N> --fqbn esp32:esp32:esp32c3 "custom badge"

# Monitor serial
arduino-cli monitor -p COM<N> --config baudrate=115200
```

---

## Estructura del código

| Archivo | Contenido |
|---|---|
| `badge.ino` | Firmware principal (~5700 líneas): setup, loop, UI, radio, NVS, navegación |
| `game_maze.h` | Juego Maze Runner con motor raycaster |
| `game_dino.h` | Juego Dino Run |
| `game_gta.h` | Juego estilo GTA |
| `game_pokemon.h` | Juego estilo Pokémon |
| `game_race.h` | Juego de carreras |
| `saver.h` / `saver_gif.h` | Screensaver animado |
| `logo.h` | Datos del logo de arranque |
