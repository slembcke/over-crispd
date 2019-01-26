#include <stdlib.h>
#include <string.h>

#include <nes.h>
#include <joystick.h>

#include "pixler/pixler.h"
#include "shared.h"
#include "gfx/gfx.h"

u8 joy0, joy1;

#define CLR_BG 0x1D

static const u8 PALETTE[] = {
	CLR_BG, 0x06, 0x19, 0x01,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	
	CLR_BG, 0x06, 0x19, 0x01,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
};

static void wait_noinput(void){
	while(joy_read(0) || joy_read(1)) px_wait_nmi();
}

static void poll_input(void){
	joy0 = joy_read(0);
	joy1 = joy_read(1);
}

static void px_ppu_sync_off(void){
	px_mask &= ~PX_MASK_RENDER_ENABLE;
	px_wait_nmi();
}

static void px_ppu_sync_on(void){
	px_mask |= PX_MASK_RENDER_ENABLE;
	px_wait_nmi();
}

static GameState main_menu(void);
static GameState game_loop(void);
static void pause(void);

static GameState main_menu(void){
	px_inc(PX_INC1);
	px_ppu_sync_off(); {
		decompress_lz4_to_vram(NT_ADDR(0, 0, 0), gfx_menu_lz4);
	} px_ppu_sync_on();
	
	wait_noinput();
	
	// Randomize the seed based on start time.
	while(true){
		poll_input();
		if(JOY_START(joy0 | joy1)) return game_loop();
		
		px_spr_end();
		px_wait_nmi();
	}
	
	return game_loop();
}

#define PLAYER_SPEED 0x0180

typedef struct {
	u16 x, y;
	s16 vx, vy;
	bool flip;
} Player;

static Player PLAYERS[2];

static const Player PLAYERS_INIT[] = {
	{128 << 8, 128 << 8, 0, 0},
	{150 << 8, 128 << 8, 0, 0},
};

static void update_player(register Player *_player, u8 joy){
	static Player player;
	memcpy(&player, _player, sizeof(player));
	
	player.vx = 0;
	player.vy = 0;
	
	if(JOY_LEFT( joy)) player.vx = -PLAYER_SPEED, player.flip = false;
	if(JOY_RIGHT(joy)) player.vx = +PLAYER_SPEED, player.flip = true;
	if(JOY_UP(   joy)) player.vy = -PLAYER_SPEED;
	if(JOY_DOWN( joy)) player.vy = +PLAYER_SPEED;
	
	player.x += player.vx;
	player.y += player.vy;
	
	memcpy(_player, &player, sizeof(player));
}

static void draw_player(register Player *player){
	ix = player->x >> 8;
	iy = player->y >> 8;
	
	if(player->flip){
		px_spr(ix - 8, iy - 8, 0x40, '<' + 0x80);
		px_spr(ix + 0, iy - 8, 0x40, '<' + 0x80);
		px_spr(ix - 9, iy + 0, 0x40, '0' + (ix & 0x3));
		px_spr(ix + 1, iy + 0, 0x40, '0' + (ix & 0x3));
	} else {
		px_spr(ix - 8, iy - 8, 0x00, '<' + 0x80);
		px_spr(ix + 0, iy - 8, 0x00, '<' + 0x80);
		px_spr(ix - 9, iy + 0, 0x00, '0' + (ix & 0x3));
		px_spr(ix + 1, iy + 0, 0x00, '0' + (ix & 0x3));
	}
}

static GameState game_loop(void){
	px_inc(PX_INC1);
	px_ppu_sync_off(); {
		decompress_lz4_to_vram(NT_ADDR(0, 0, 0), gfx_level1_lz4);
		px_spr_clear();
	} px_ppu_sync_on();
	
	PLAYERS[0] = PLAYERS_INIT[0];
	PLAYERS[1] = PLAYERS_INIT[1];
	
	wait_noinput();
	while(true){
		
		poll_input();
		
		if(JOY_START(joy0 | joy1)) pause();
		
		update_player(PLAYERS + 0, joy0);
		update_player(PLAYERS + 1, joy1);
		
		// z-sort and draw.
		if((PLAYERS[0].y >> 8) > (PLAYERS[1].y >> 8)){
			draw_player(PLAYERS + 0);
			draw_player(PLAYERS + 1);
		} else {
			draw_player(PLAYERS + 1);
			draw_player(PLAYERS + 0);
		}
		px_spr_end();
		px_wait_nmi();
	}
	
	return main_menu();
}

static void pause(void){
	wait_noinput();
	while(!JOY_START(joy_read(0))){}
	wait_noinput();
}

void main(void){
	px_bank_select(0);
	
	// Install the cc65 static joystick driver.
	joy_install(joy_static_stddrv);
	
	// Set an initial random seed that's not just zero.
	// The main menu increments this constantly until the player starts the game.
	rand_seed = 0x0D8E;
	
	// Set the palette.
	waitvsync();
	px_addr(PAL_ADDR);
	px_blit(32, PALETTE);
	
	// Load BG tiles..
	px_bg_table(0);
	decompress_lz4_to_vram(CHR_ADDR(0, 0x00), gfx_chr0_lz4chr);
	
	// Load sprites.
	px_spr_table(1);
	decompress_lz4_to_vram(CHR_ADDR(1, 0x00), gfx_chr0_lz4chr);
	
	music_init(MUSIC);
	sound_init(SOUNDS);
	
	game_loop();
	main_menu();
}
