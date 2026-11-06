# HackConRD 2026 Badge for Flipper Zero

[Español](README.md) | **English**

A port of the [HackConRD 2026 conference badge](https://github.com/nullvestige/Badge_HackConRD_2026) to the Flipper Zero, for everyone who could not be at the event.

The badge's radio is a CC1101, the same chip the Flipper has, so this is more than a look-alike: a Flipper speaks the badge's RF protocols and can add real badges as friends, and the other way around.

## What's in it

| Badge feature | On the Flipper |
|---|---|
| **Doom Maze, Dino Run, HackerMon, Star Invaders** | All four, with the original rules, maps, enemies, scoring and music. Redrawn for the 128x64 screen. |
| **CTF story** (4 flags across chapters I to IV) | Complete, same logic and flags. |
| **Go Hunting** (fox hunt by RSSI) | Real RF, plus virtual bearers in Solo Mode. |
| **Friends over RF** | Same 32-byte frames and handshake as the badge. Interoperable. |
| **THE KEY / THE BOX** | Same codes and checks, and the BOX frame is still transmitted. |
| **Learning RF lab** (spectrum scan, injection, frames, capture, replay) | All five lessons, same 9-byte frames. The guide QR code is included. |
| **Lock screen** with nick, role and ID | 21 images, converted to 1-bit. |
| **Settings** | Everything the badge has, plus Solo Mode and Radio Info. |
| **CTFs** (Flipper only) | Plays the event's missing hardware and people over the air, so a real badge can finish its challenges. |
| **12 WS2812B LEDs** | Same themes and effects, shown on the Flipper's single RGB LED. Fox hunting color follows RSSI. |
| **Buzzer** | Flipper speaker. Honors the system's Stealth Mode. |

## Solo Mode

At the event, the quest depended on a crowd of badges, people carrying transmitters, and a second badge for the replay lesson. Solo Mode (on by default) stands in for them, locally and never on the air:

- **Friends:** simulated badges show up in scans and accept requests.
- **Go Hunting:** a Virtual Bearers option. Walk an invisible grid with the D-pad, following the signal. Each bearer gives you a piece of the truth.
- **Learning:** a simulated peer sends frames you can capture and replay.

Real radio traffic keeps working alongside it. Turn it off in `Settings > Solo Mode` when playing with real badges, so the lists only show real devices.

## CTFs

For people who have a real badge but no event around it. Where Solo Mode lets the Flipper play alone, the `CTFs` menu makes it the other side of the challenge: everything goes over the air to the badge, and the badge needs no firmware change. It works with Solo Mode on or off.

| Role | Stands in for | What it does |
|---|---|---|
| **Bearer** | People carrying transmitters | Sends a steady carrier for the badge's `Go Hunting`. Hide the Flipper and hunt it. Once found, hold OK to get a digit pair. There are three rounds, one pair each. |
| **Crowd** | A crowd of badges | Appears as 16 badges on 433.00 MHz, CAESARS among them, and accepts every request. Enough to earn FLAG 1 and FLAG 2 and unlock THE KEY. |
| **THE KEY** | Nothing, it runs on the badge | Keeps the pairs the Bearer revealed and says what is left to do. |
| **THE BOX** | The sealed box at the venue | Listens on 433.00 MHz for the code the badge sends, runs the same hash check, and opens when it is right. It shows who opened it. |
| **Lab Partner** | A second badge in the RF lab | Sends frames for lessons 2 to 5, accepts the badge's replay of them, and can replay the badge's own frames back at it. |

Things worth knowing when playing:

- **Bearer frequency:** it has to match the badge's `Settings > Frequency`. The default on both is 433.90 MHz.
- **Bearer power:** the badge's signal bar was tuned for 1-2 m through a backpack. At about 2 m a badge read around -60 dBm at MED and -40 dBm at MAX, which is already past the top of its bar, so MED is the default. MIN and LOW read around -70 to -80 dBm and suit a small space.
- **Bearer time limit:** it stops transmitting after 30 minutes. Be considerate, a carrier blocks others on that frequency while it runs.
- **Who hides it:** the reveal runs on honor, and the game is better if somebody else hides the Flipper.
- **The badge sleeps:** it locks after a minute without a key press, which turns its radio off during a hunt. Tap a key while walking.
- **A full friends list:** a badge holds 32 friends. At that point it cannot add more, so delete some first.
- **Linking Lab Partner:** it has to find the badge's channel first. In lesson 1 on the badge, hold DOWN for TX mode and press UP until the Flipper shows `LOCK`. The link lasts as long as the badge stays out of its spectrum sweep.
- **Replaying the badge with Lab Partner:** the badge has to be on lesson 4, its victim mode, when you press OK on the Flipper. On any other lesson it ignores the replay and the Flipper shows `NO ANSWER`.
- **THE BOX is optional for the flag:** the badge checks the code itself and never waits for an answer. FLAG 4 appears on the badge either way. Nothing is known about what the real box did when opened, so the opening here is this port's own design.

Friends and THE BOX are fixed at 433.00 MHz in the badge's firmware. Its Frequency setting only moves the hunt. So the Crowd needs a Flipper whose firmware allows transmitting on 433.00, and it says so on screen if not. The Bearer, THE BOX and Lab Partner do not have that limit: the Bearer can move to any frequency both devices allow, THE BOX only listens, and the lab runs inside the usual 433 MHz band.

## Controls

The badge has 4 buttons, the Flipper has a D-pad, so Flipper conventions apply where they differ.

| Where | Control |
|---|---|
| Menus | Up/Down move, OK select, Back return |
| Main menu | Back exits the app, **hold Back** opens the lock screen |
| Code and nick editors | Up/Down change, Left/Right move, **hold OK** submit |
| Dino Run | Up or OK jump, Down duck, hold Back exit |
| Doom Maze | Up/Down walk, Left/Right turn, OK toggle minimap, Back pause |
| HackerMon | D-pad walk, hold Back exit. In battle: D-pad pick a move, OK attack, Back flee (costs 5 HP) |
| Star Invaders | Left/Right move, OK fire, hold Back exit |
| CTFs: Bearer | Up/Down frequency, Left/Right power, OK transmit. On air: hold OK when found, Back stop |
| CTFs: THE KEY | Hold OK to forget the pairs and play again |
| CTFs: THE BOX | OK seals it again after it opens |
| CTFs: Lab Partner | OK replay the badge's last frame, Down search for the badge again |

## Install

Copy the `.fap` matching your firmware to `SD Card/apps/Games/` with qFlipper, then open it from `Apps > Games`:

- `release/hackconrd_badge_momentum.fap`: Momentum `mntm-012` (API 87.1)
- `release/hackconrd_badge_official.fap`: official firmware `1.5.1` (API 88.2)

Both files are also on the [Releases page](https://github.com/JonathanERC/Badge_HackConRD_2026/releases/latest).

For any other firmware, build from source (next section).

## Build

```bash
pipx install ufbt
ufbt update --channel=release                # official firmware SDK
ufbt launch                                  # build, install and run over USB
```

For Momentum: `ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release`

`tools/convert_assets.py` regenerates `assets/`, `icon.png` and `src/qr_data.h` from the original badge repository (needs `pillow` and `qrcode`).

## RF notes

| Link | Frequency | Format |
|---|---|---|
| Friends, THE BOX | 433.00 MHz | GFSK ~4.8 kbps, fixed 32-byte packets, CRC |
| Learning lab | 433.72 to 434.10 MHz sweep | GFSK ~4.8 kbps, fixed 9-byte packets, CRC |
| Fox hunt | 433.90 MHz, adjustable | Badge side: live RSSI, RX only. CTFs Bearer: unmodulated carrier |

**Regional limits are respected.** Transmitting always goes through the firmware's regional check; the app never bypasses it. Most Flipper regions start the 433 MHz band at 433.05 MHz, so 433.00 is refused there, and the Friends link moves to 433.05 MHz instead. `Settings > Radio Info` shows which channel your device uses. Follow your local radio regulations.

What that means for Friends:

| Your Flipper's channel | Real badges | Flippers on 433.00 | Flippers on 433.05 |
|---|---|---|---|
| 433.00 | Works (tested) | Should work | No contact |
| 433.05 | No contact (tested) | No contact | Should work |

Solo Mode doesn't use the radio, so the whole CTF can be finished on any Flipper.

## Differences from the badge

- **RF lab channel search:** the badge stays on whichever frequency the sweep last touched when leaving lesson 1, so two devices can easily end up out of reach of each other. Until it hears a real frame, the Flipper hops across the lab band to find the other device, then stays on that channel. The frames screen shows `SEARCH` or `LOCK`.
- **Fox hunting doesn't time out:** the badge drops to its lock screen after a minute without a key press, which cuts a hunt short while walking.
- **UI themes:** the screen is 1-bit, so WHITE is the light theme, and MATRIX, RED and BLUE are dark themes that differ in LED accent color.
- **Friends list:** OK deletes, Back leaves. The badge used Back for both, short and held.

## Test status

Tested on a Flipper Zero running Momentum `mntm-012`, against a real badge:

- **Friends:** requests and accepts in both directions at 433.00 MHz. At 433.05 MHz: no contact either way.
- **RF lab:** frames from the badge received, captured, and replayed back to it, with `ACCESS GRANTED` on both devices.
- **Solo:** all four games, the full CTF with its 4 flags, and all five lab lessons.
- **CTFs, Bearer:** the bar in the badge's `Go Hunting` moves with distance and with all four power levels. All three pairs reveal correctly.
- **CTFs, Crowd:** the badge added members and reached FLAG 2.
- **CTFs, THE KEY:** the Bearer's pairs solved it on the badge, for FLAG 3.
- **CTFs, THE BOX:** a wrong code and the right one both arrived, and the box opened with the badge's nick and ID. FLAG 4 appeared on the badge.
- **CTFs, Lab Partner:** used for lessons 2 to 5, with replays accepted in both directions.

Also tested and confirmed by a person, without the AI:

- **CTFs, Crowd:** from a badge without friends, 10 adds earn FLAG 1 on the way, and all 16 members show up in a single scan.
- **Fox hunting:** against a real transmitter.
- **Buzzer and LED:** the output of both.

Not tested yet:

- The Bearer's 30-minute stop, and its LED staying off while on air.
- The `TX BLOCKED` and `TX REFUSED` screens, on a firmware that refuses those frequencies.
- The official firmware build. It compiles, but has not been run on a device.
- Flipper to Flipper, on either channel.

## Artwork

The lock screens come from the badge repository. Some depict third-party characters and logos, and one shows a real person (`hacker_saver`). Check their rights before publishing this app in a catalog. To drop an image, remove it from `LOCKS` in `tools/convert_assets.py` and from `SAVER_NAMES` in `src/lockscreen.c`, then update `SAVER_COUNT` in `src/features.h`.

## AI notice

Almost all of this port was made with AI assistance, using Claude (Anthropic) through Claude Code:

- **Code and documentation:** the AI wrote nearly all of the code, tools and documentation, and analyzed the original firmware.
- **Direction:** a person chose the features and set the design rules.
- **Joint testing:** the first list under Test status ran on real hardware. A person handled the real badge and reported what it did. The AI ran scripts that pressed the Flipper's buttons and read its screen, and the Solo Mode tests were done that way.
- **Human review and testing:** in addition, that same person reviewed the code and tested every feature on their own, except what Test status lists as not tested yet.

This notice only covers this port, not the original firmware, games or artwork, which belong to their authors. The code may still contain mistakes, so review it before relying on it.

## Credits

- Original badge firmware: Junior Restituyo.
- Pin guide: Emeraldo Ramos (RAMTEC).
- Flipper Zero port: Jonathan Esteban Rondon (RONS), with Claude (Anthropic) as AI assistant.
