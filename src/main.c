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

#define GENE_00 0
#define GENE_L 0x80
#define GENE_R 0x08

#define GENE_A0 (0x00 | GENE_L)
#define GENE_B0 (0x10 | GENE_L)
#define GENE_C0 (0x20 | GENE_L)
#define GENE_D0 (0x30 | GENE_L)

#define GENE_0A (0x00 | GENE_R)
#define GENE_0B (0x01 | GENE_R)
#define GENE_0C (0x02 | GENE_R)
#define GENE_0D (0x03 | GENE_R)

#define GENE_COUNT 10
static u8 GENE_X[GENE_COUNT] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
static u8 GENE_Y[GENE_COUNT] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0};
static u8 GENE_VALUE[GENE_COUNT] = {
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
};

static void genes_draw(void){
	for(idx = 0; idx < GENE_COUNT; ++idx){
		ix = GENE_X[idx];
		iy = GENE_Y[idx];
		tmp = GENE_VALUE[idx];
		
		if(tmp & GENE_L){
			iz = 'A' + 0x80 + ((tmp >> 4) & 0x3);
			px_spr(ix - 8, iy - 0x10, 0x00, iz);
			px_spr(ix - 8, iy - 0x08, 0x00, iz);
		}
		
		if(tmp & GENE_R){
			iz = 'A' + 0x80 + ((tmp >> 0) & 0x3);
			px_spr(ix + 0, iy - 0x10, 0x00, iz);
			px_spr(ix + 0, iy - 0x08, 0x00, iz);
		}
	}
}

#define PLAYER_SPEED 0x0180
#define GRAB_OFFSET

typedef struct {
	u16 x, y;
	s16 vx, vy;
	
	u8 joy, prev_joy;
	bool flip;
	
	s8 gene_held;
} Player;

static Player PLAYER[2];

static const Player PLAYER_INIT[] = {
	{128 << 8, 128 << 8, 0, 0, 0x00, 0x00, false, -1},
	{150 << 8, 128 << 8, 0, 0, 0x00, 0x00, false, -1},
};

static void player_update(register Player *_player, u8 joy){
	static Player player;
	memcpy(&player, _player, sizeof(player));
	
	player.prev_joy = player.joy;
	player.joy = joy;
	
	player.vx = 0;
	player.vy = 0;
	
	if(JOY_LEFT( player.joy)) player.vx = -PLAYER_SPEED, player.flip = false;
	if(JOY_RIGHT(player.joy)) player.vx = +PLAYER_SPEED, player.flip = true;
	if(JOY_UP(   player.joy)) player.vy = -PLAYER_SPEED;
	if(JOY_DOWN( player.joy)) player.vy = +PLAYER_SPEED;
	
	player.x += player.vx;
	player.y += player.vy;
	
	if(JOY_BTN_B(player.joy ^ player.prev_joy) && JOY_BTN_B(player.joy)){
		if(player.gene_held >= 0){
			// Drop the gene.
			player.gene_held = -1;
			sound_play(SOUND_DROP);
		} else {
			// Search for a gene to pick up.
			for(idx = 0; idx < GENE_COUNT; ++idx){
				// if(abs(GENE_X[idx] - (player.x >> 8) - (player.flip ? 18 : -18)) < 12 && abs(GENE_Y[idx] - (player.y >> 8)) < 12){
				if(abs((s8)GENE_X[idx] - (s8)(player.x >> 8) - (player.flip ? 18 : -18)) < 12 && abs((s8)GENE_Y[idx] - (s8)(player.y >> 8)) < 12){
					player.gene_held = idx;
					sound_play(SOUND_PICKUP);
					break;
				}
			}
		}
	}
	
	if(player.gene_held >= 0){
		// GENE_VALUE[player.gene_held] = 0;
		GENE_X[player.gene_held] = (player.x >> 8) + (player.flip ? 18 : -18);
		GENE_Y[player.gene_held] = (player.y >> 8);
	}
	
	player.prev_joy = joy;
	memcpy(_player, &player, sizeof(player));
}

static void player_draw(register Player *player){
	ix = player->x >> 8;
	iy = player->y >> 8;
	idx = (ix/2 + (iy/4)) & 0x3;
	
	if(player->flip){
		px_spr(ix - 8, iy - 0x10, 0x40, '<' + 0x80);
		px_spr(ix + 0, iy - 0x10, 0x40, '<' + 0x80);
		px_spr(ix - 9, iy - 0x08, 0x40, '0' + idx);
		px_spr(ix + 1, iy - 0x08, 0x40, '0' + idx);
	} else {
		px_spr(ix - 8, iy - 0x10, 0x00, '<' + 0x80);
		px_spr(ix + 0, iy - 0x10, 0x00, '<' + 0x80);
		px_spr(ix - 9, iy - 0x08, 0x00, '0' + idx);
		px_spr(ix + 1, iy - 0x08, 0x00, '0' + idx);
	}
}

static GameState game_loop(void){
	px_inc(PX_INC1);
	px_ppu_sync_off(); {
		decompress_lz4_to_vram(NT_ADDR(0, 0, 0), gfx_level1_lz4);
		px_spr_clear();
	} px_ppu_sync_on();
	
	PLAYER[0] = PLAYER_INIT[0];
	PLAYER[1] = PLAYER_INIT[1];
	
	wait_noinput();
	while(true){
		
		poll_input();
		
		if(JOY_START(joy0 | joy1)) pause();
		
		player_update(PLAYER + 0, joy0);
		player_update(PLAYER + 1, joy1);
		
		// z-sort and draw.
		if((PLAYER[0].y >> 8) > (PLAYER[1].y >> 8)){
			player_draw(PLAYER + 0);
			player_draw(PLAYER + 1);
		} else {
			player_draw(PLAYER + 1);
			player_draw(PLAYER + 0);
		}
		
		genes_draw();
		
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
