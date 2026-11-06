// Entry points of every badge module, dispatched from the app core.
#pragma once

#include "badge.h"
#include "radio.h"

// Lock screen
#define SAVER_COUNT 21
extern const char* const SAVER_NAMES[SAVER_COUNT];
void lock_load_art(uint8_t index);
void lock_draw(Canvas* c);
void lock_draw_art_preview(Canvas* c, int x, int y);
void boot_enter(void);
void boot_tick(const ButtonsEdge* b);
void boot_draw(Canvas* c);

// Menus
void menu_tick(const ButtonsEdge* b);
void menu_draw(Canvas* c);
void games_menu_tick(const ButtonsEdge* b);
void games_menu_draw(Canvas* c);
void hunt_menu_tick(const ButtonsEdge* b);
void hunt_menu_draw(Canvas* c);

// Fox hunt
void fox_enter(bool skip_intro);
void fox_exit(void);
void fox_tick(const ButtonsEdge* b);
void fox_draw(Canvas* c);
void fox_sim_enter(bool skip_intro);
void fox_sim_tick(const ButtonsEdge* b);
void fox_sim_draw(Canvas* c);

// Friends. Frame layout is documented in friends.c.
#define FRF_MAGIC     0xF6
#define FRF_BROADCAST 0xFFFF
enum { FRF_HELLO = 1, FRF_REQ = 2, FRF_ACK = 3, FRF_NACK = 4 };

void friends_on_enter(ScreenMode mode);
void friends_background_tick(void);
void friends_tick(ScreenMode mode, const ButtonsEdge* b);
void friends_draw(ScreenMode mode, Canvas* c);
void friends_scan_seen(uint16_t id, uint8_t bro, int8_t rssi, const char* nick);
void friends_virtual_ack(uint16_t from);
bool friends_is_friend(uint16_t id);

// Quest
void quest_after_friend_added(uint16_t sid, uint8_t bro, bool added, bool was_armed, bool* story_started);
uint8_t bro_encode(uint8_t role);

// THE KEY / THE BOX
void key_enter(bool skip_intro);
void key_tick(const ButtonsEdge* b);
void key_draw(Canvas* c);
char key_pair_char(uint8_t i);
void box_enter(bool skip_intro);
void box_tick(const ButtonsEdge* b);
void box_draw(Canvas* c);
bool box_code_is_correct(const char* code);

// Learning RF lab
void learning_enter(bool skip_intro);
void learning_tick(const ButtonsEdge* b);
void learning_draw(Canvas* c);

// CTFs: the Flipper stands in for the event's hardware and people, over the air, so that a real
// badge can finish its challenges. Solo Mode is the opposite: the Flipper plays alone.
void ctf_menu_tick(const ButtonsEdge* b);
void ctf_menu_draw(Canvas* c);
void ctf_bearer_enter(bool skip_intro);
void ctf_bearer_exit(void);
void ctf_bearer_tick(const ButtonsEdge* b);
void ctf_bearer_draw(Canvas* c);
void ctf_key_tick(const ButtonsEdge* b);
void ctf_key_draw(Canvas* c);
void ctf_crowd_enter(bool skip_intro);
void ctf_crowd_tick(const ButtonsEdge* b);
void ctf_crowd_draw(Canvas* c);
void ctf_box_enter(bool skip_intro);
void ctf_box_tick(const ButtonsEdge* b);
void ctf_box_draw(Canvas* c);
void lab_partner_enter(bool skip_intro);
void lab_partner_tick(const ButtonsEdge* b);
void lab_partner_draw(Canvas* c);

// Settings and first boot
void settings_enter(void);
void settings_tick(const ButtonsEdge* b);
void settings_draw(Canvas* c);
void first_boot_enter(void);
void first_boot_tick(const ButtonsEdge* b);
void first_boot_draw(Canvas* c);

// Solo mode: stand-ins for the people and hardware of the live event.
bool solo_enabled(void);
void solo_scan_reset(void);
void solo_scan_tick(void);
bool solo_is_virtual(uint16_t id);
void solo_request_start(uint16_t id);
void solo_request_tick(void);
void solo_lab_tick(bool active);
void solo_lab_replay(uint8_t target);
uint8_t solo_lab_peer(void);

// Games. enter(resume): resume is true when returning from the lock screen.
void dino_enter(bool resume);
void dino_exit(void);
void dino_tick(const ButtonsEdge* b);
void dino_draw(Canvas* c);

void maze_enter(bool resume);
void maze_exit(void);
void maze_tick(const ButtonsEdge* b);
void maze_draw(Canvas* c);

void invaders_enter(bool resume);
void invaders_exit(void);
void invaders_tick(const ButtonsEdge* b);
void invaders_draw(Canvas* c);

void hackermon_enter(bool resume);
void hackermon_exit(void);
void hackermon_tick(const ButtonsEdge* b);
void hackermon_draw(Canvas* c);
