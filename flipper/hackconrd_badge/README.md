# Badge de HackConRD 2026 para Flipper Zero

**Español** | [English](README.en.md)

Adaptación del [badge de la conferencia HackConRD 2026](https://github.com/nullvestige/Badge_HackConRD_2026) al Flipper Zero, para quienes no pudieron asistir al evento.

El módulo de radio del badge es un CC1101, el mismo chip que tiene el Flipper, así que esto es más que una imitación: un Flipper habla los protocolos RF del badge y puede agregar badges reales como friends, y viceversa.

## Qué incluye

| Función del badge | En el Flipper |
|---|---|
| **Doom Maze, Dino Run, HackerMon, Star Invaders** | Los cuatro, con sus reglas, mapas, enemigos, puntuación y música originales. Redibujados para la pantalla de 128x64. |
| **Historia del CTF** (4 flags a lo largo de los chapters I a IV) | Completa, con la misma lógica y las mismas flags. |
| **Go Hunting** (fox hunting por RSSI) | RF real, más Virtual Bearers en Solo Mode. |
| **Friends por RF** | Los mismos frames de 32 bytes y el mismo handshake que el badge. Interoperable. |
| **THE KEY / THE BOX** | Los mismos códigos y comprobaciones, y el frame de THE BOX se sigue transmitiendo. |
| **RF lab de Learning** (spectrum scan, packet injection, frames, capture, replay) | Las cinco lessons, con los mismos frames de 9 bytes. Incluye el código QR de la guía. |
| **Lock screen** con nick, role e ID | 21 imágenes, convertidas a 1 bit. |
| **Settings** | Todo lo que tiene el badge, más Solo Mode y Radio Info. |
| **CTFs** (solo en Flipper) | Reemplaza por aire al hardware y a las personas del evento que faltan, para que un badge real pueda completar sus retos. |
| **12 LEDs WS2812B** | Los mismos themes y efectos, mostrados en el único LED RGB del Flipper. En fox hunting, el color sigue al RSSI. |
| **Buzzer** | Altavoz del Flipper. Respeta el Stealth Mode del sistema. |

## Solo Mode

En el evento, la quest dependía de un crowd de badges, de personas que llevaban transmisores y de un segundo badge para la lesson de replay. Solo Mode (activado por defecto) los reemplaza, de forma local y nunca por aire:

- **Friends:** aparecen badges simulados en los scans y aceptan requests.
- **Go Hunting:** incluye la opción Virtual Bearers. Recorre una cuadrícula invisible con el D-pad, siguiendo la señal. Cada bearer te da una parte de la verdad.
- **Learning:** un segundo badge simulado envía frames que puedes capturar y reenviar con replay.

El tráfico de radio real sigue funcionando al mismo tiempo. Desactiva Solo Mode en `Settings > Solo Mode` cuando juegues con badges reales, para que las listas solo muestren dispositivos reales.

## CTFs

Para quienes tienen un badge real, pero no un evento alrededor. Mientras Solo Mode permite que el Flipper juegue por su cuenta, el menú `CTFs` lo convierte en la otra parte del reto: todo va por aire hacia el badge, y el badge no necesita ningún cambio de firmware. Funciona con Solo Mode activado o desactivado.

| Rol | Reemplaza a | Qué hace |
|---|---|---|
| **Bearer** | Las personas que llevaban transmisores | Envía un carrier continuo para el `Go Hunting` del badge. Esconde el Flipper y búscalo. Una vez encontrado, mantén presionado OK para obtener un par de dígitos. Son tres rondas, con un par en cada una. |
| **Crowd** | Un crowd de badges | Aparece como 16 badges en 433.00 MHz, CAESARS entre ellos, y acepta todos los requests. Alcanza para obtener FLAG 1 y FLAG 2 y desbloquear THE KEY. |
| **THE KEY** | Nada, se ejecuta en el badge | Guarda los pares que reveló Bearer e indica qué falta por hacer. |
| **THE BOX** | THE BOX sellada del evento | Escucha en 433.00 MHz el código que envía el badge, ejecuta la misma comprobación de hash y se abre cuando es correcto. Muestra quién la abrió. |
| **Lab Partner** | Un segundo badge en el RF lab | Envía frames para las lessons 2 a 5, acepta el replay que el badge hace de ellos y puede devolverle al badge sus propios frames con replay. |

Datos útiles para jugar:

- **Frecuencia de Bearer:** tiene que coincidir con `Settings > Frequency` del badge. El valor predeterminado en ambos es 433.90 MHz.
- **Power de Bearer:** la barra de señal del badge se ajustó para 1-2 m a través de una mochila. A unos 2 m, un badge leyó alrededor de -60 dBm en MED y -40 dBm en MAX, lo que ya supera el tope de su barra, por eso MED es el valor predeterminado. MIN y LOW leen alrededor de -70 a -80 dBm y sirven para un espacio pequeño.
- **Límite de tiempo de Bearer:** deja de transmitir a los 30 minutos. Ten consideración: mientras está activo, el carrier bloquea a otras personas en esa frecuencia.
- **Quién lo esconde:** la revelación funciona por honor, y el juego es mejor si otra persona esconde el Flipper.
- **El badge se duerme:** se bloquea tras un minuto sin presionar ningún botón, lo que apaga su radio durante el hunting. Presiona un botón de vez en cuando mientras caminas.
- **Friends List llena:** un badge guarda hasta 32 friends. Al llegar a ese punto no puede agregar más, así que primero borra algunos.
- **Enlazar Lab Partner:** primero tiene que encontrar el channel del badge. En la lesson 1 del badge, mantén presionado DOWN para entrar en TX mode y presiona UP hasta que el Flipper muestre `LOCK`. El enlace dura mientras el badge no vuelva a su spectrum scan.
- **Hacerle replay al badge con Lab Partner:** el badge tiene que estar en la lesson 4, su victim mode, cuando presiones OK en el Flipper. En cualquier otra lesson ignora el replay y el Flipper muestra `NO ANSWER`.
- **THE BOX es opcional para la flag:** el badge comprueba el código por sí mismo y nunca espera respuesta. FLAG 4 aparece en el badge de cualquier forma. No se sabe qué hacía THE BOX real al abrirse, así que la apertura aquí es un diseño propio de esta adaptación.

Friends y THE BOX están fijos en 433.00 MHz en el firmware del badge. Su ajuste Frequency solo mueve el hunting. Por eso Crowd necesita un Flipper cuyo firmware permita transmitir en 433.00, y lo indica en pantalla si no es así. Bearer, THE BOX y Lab Partner no tienen ese límite: Bearer puede moverse a cualquier frecuencia que permitan ambos dispositivos, THE BOX solo escucha, y el RF lab funciona dentro de la banda habitual de 433 MHz.

## Controles

El badge tiene 4 botones y el Flipper tiene un D-pad, así que donde difieren se usan las convenciones del Flipper.

| Dónde | Control |
|---|---|
| Menús | Up/Down mover, OK seleccionar, Back volver |
| Menú principal | Back sale de la app, **mantener Back** abre la lock screen |
| Editores de código y nick | Up/Down cambiar, Left/Right mover, **mantener OK** enviar |
| Dino Run | Up u OK saltar, Down agacharse, mantener Back salir |
| Doom Maze | Up/Down caminar, Left/Right girar, OK mostrar u ocultar el minimapa, Back pausa |
| HackerMon | D-pad caminar, mantener Back salir. En batalla: D-pad elegir movimiento, OK atacar, Back huir (cuesta 5 HP) |
| Star Invaders | Left/Right mover, OK disparar, mantener Back salir |
| CTFs: Bearer | Up/Down frecuencia, Left/Right power, OK transmitir. Al aire: mantener OK al encontrarlo, Back detener |
| CTFs: THE KEY | Mantener OK para olvidar los pares y jugar de nuevo |
| CTFs: THE BOX | OK la vuelve a sellar después de abrirse |
| CTFs: Lab Partner | OK hace replay del último frame del badge, Down vuelve a buscar el badge |

## Instalación

Copia el `.fap` que corresponda a tu firmware a `SD Card/apps/Games/` con qFlipper y luego ábrelo desde `Apps > Games`:

- `release/hackconrd_badge_momentum.fap`: Momentum `mntm-012` (API 87.1)
- `release/hackconrd_badge_official.fap`: firmware oficial `1.5.1` (API 88.2)

Ambos archivos también están en la [página de Releases](https://github.com/JonathanERC/Badge_HackConRD_2026/releases/latest).

Para cualquier otro firmware, compila desde el código fuente (siguiente sección).

## Compilación

```bash
pipx install ufbt
ufbt update --channel=release                # SDK del firmware oficial
ufbt launch                                  # compila, instala y ejecuta por USB
```

Para Momentum: `ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release`

`tools/convert_assets.py` regenera `assets/`, `icon.png` y `src/qr_data.h` a partir del repositorio original del badge (necesita `pillow` y `qrcode`).

## Notas de RF

| Enlace | Frecuencia | Formato |
|---|---|---|
| Friends, THE BOX | 433.00 MHz | GFSK ~4.8 kbps, packets fijos de 32 bytes, CRC |
| RF lab de Learning | Sweep de 433.72 a 434.10 MHz | GFSK ~4.8 kbps, packets fijos de 9 bytes, CRC |
| Fox hunt | 433.90 MHz, ajustable | Lado del badge: RSSI en vivo, solo RX. CTFs Bearer: carrier sin modular |

**Se respetan los límites regionales.** La transmisión siempre pasa por la comprobación regional del firmware; la app nunca la evita. La mayoría de las regiones del Flipper empiezan la banda de 433 MHz en 433.05 MHz, así que allí se rechaza 433.00 y el enlace de Friends pasa a 433.05 MHz. `Settings > Radio Info` muestra qué channel usa tu dispositivo. Cumple las regulaciones de radio de tu país.

Lo que esto significa para Friends:

| Channel de tu Flipper | Badges reales | Flippers en 433.00 | Flippers en 433.05 |
|---|---|---|---|
| 433.00 | Funciona (probado) | Debería funcionar | Sin contacto |
| 433.05 | Sin contacto (probado) | Sin contacto | Debería funcionar |

Solo Mode no usa RF, así que todo el CTF se puede terminar en cualquier Flipper.

## Diferencias con el badge

- **Búsqueda de channel en el RF lab:** el badge se queda en la última frecuencia que tocó el sweep al salir de la lesson 1, así que es fácil que dos dispositivos queden fuera de alcance entre sí. Hasta que oye un frame real, el Flipper salta por la banda del RF lab para encontrar el otro dispositivo y luego se queda en ese channel. La pantalla de frames muestra `SEARCH` o `LOCK`.
- **Fox hunting no tiene tiempo límite:** el badge vuelve a su lock screen tras un minuto sin presionar ningún botón, lo que interrumpe el hunting mientras caminas.
- **UI themes:** la pantalla es de 1 bit, así que WHITE es el theme claro, y MATRIX, RED y BLUE son themes oscuros que se diferencian por el color de acento del LED.
- **Friends List:** OK borra, Back sale. El badge usaba Back para ambas cosas, con una pulsación corta y una larga.

## Estado de las pruebas

Probado en un Flipper Zero con Momentum `mntm-012`, contra un badge real:

- **Friends:** requests y accepts en ambas direcciones a 433.00 MHz. A 433.05 MHz: sin contacto en ninguna dirección.
- **RF lab:** frames del badge recibidos, capturados y reenviados de vuelta a él con replay, con `ACCESS GRANTED` en ambos dispositivos.
- **Solo:** los cuatro juegos, el CTF completo con sus 4 flags y las cinco lessons del RF lab.
- **CTFs, Bearer:** la barra del `Go Hunting` del badge se mueve con la distancia y con los cuatro niveles de power. Los tres pares se revelan correctamente.
- **CTFs, Crowd:** el badge agregó miembros y llegó a FLAG 2.
- **CTFs, THE KEY:** los pares de Bearer lo resolvieron en el badge, y así se obtuvo FLAG 3.
- **CTFs, THE BOX:** llegaron tanto un código incorrecto como el correcto, y THE BOX se abrió con el nick y el ID del badge. FLAG 4 apareció en el badge.
- **CTFs, Lab Partner:** usado en las lessons 2 a 5, con replays aceptados en ambas direcciones.

También probado y confirmado por una persona, sin la IA:

- **CTFs, Crowd:** desde un badge sin friends, con 10 friends agregados se obtiene FLAG 1 en el camino, y los 16 miembros aparecen en un solo scan.
- **Fox hunting:** contra un transmisor real.
- **Buzzer y LED:** la salida de ambos.

Aún sin probar:

- La parada de Bearer a los 30 minutos, y que su LED se mantenga apagado mientras transmite.
- Las pantallas `TX BLOCKED` y `TX REFUSED`, en un firmware que rechace esas frecuencias.
- La compilación para el firmware oficial. Compila, pero no se ha ejecutado en un dispositivo.
- Flipper contra Flipper, en cualquiera de los dos channels.

## Imágenes

Las lock screens vienen del repositorio del badge. Algunas muestran personajes y logos de terceros, y una muestra a una persona real (`hacker_saver`). Verifica sus derechos antes de publicar esta app en un catálogo. Para quitar una imagen, elimínala de `LOCKS` en `tools/convert_assets.py` y de `SAVER_NAMES` en `src/lockscreen.c`, y luego actualiza `SAVER_COUNT` en `src/features.h`.

## Aviso sobre IA

Casi toda esta adaptación se hizo con asistencia de IA, usando Claude (Anthropic) a través de Claude Code:

- **Código y documentación:** la IA escribió casi todo el código, las herramientas y la documentación, y analizó el firmware original.
- **Dirección:** una persona eligió las funciones y definió las reglas de diseño.
- **Pruebas en conjunto:** la primera lista de Estado de las pruebas se ejecutó en hardware real. Una persona manejó el badge real y reportó lo que hacía. La IA ejecutó scripts que presionaban los botones del Flipper y leían su pantalla, y las pruebas de Solo Mode se hicieron de esa forma.
- **Revisión y pruebas humanas:** además, esa misma persona revisó el código y probó cada función por su cuenta, salvo lo que Estado de las pruebas indica como aún sin probar.

Este aviso solo cubre esta adaptación, no el firmware, los juegos ni las imágenes originales, que pertenecen a sus autores. Aun así, el código puede contener errores, por lo que conviene revisarlo antes de basarte en él.

## Créditos

- Firmware original del badge: Junior Restituyo.
- Guía de pins: Emeraldo Ramos (RAMTEC).
- Adaptación a Flipper Zero: Jonathan Esteban Rondon (RONS), con Claude (Anthropic) como asistente de IA.
