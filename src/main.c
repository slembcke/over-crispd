#include <stdlib.h>
#include <string.h>

#include <nes.h>
#include <joystick.h>

#include "pixler/pixler.h"
#include "shared.h"
#include "gfx/gfx.h"

u8 joy0, joy1;

#define BLANK 0x60

#define CLR_BG 0x36

static const u8 PALETTE[] = {
	CLR_BG, 0x26, 0x16, 0x06,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	CLR_BG, CLR_BG, CLR_BG, CLR_BG,
	
	CLR_BG, 0x26, 0x16, 0x06,
	CLR_BG, 0x24, 0x14, 0x04,
	CLR_BG, 0x28, 0x17, CLR_BG,
	CLR_BG, 0x20, 0x2D, 0x1D,
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

static void debug_hex(u16 value){
	px_buffer_data(4, NT_ADDR(0, 1, 1));
	PX.buffer[0] = _hextab[(value >> 0xC) & 0xF];
	PX.buffer[1] = _hextab[(value >> 0x8) & 0xF];
	PX.buffer[2] = _hextab[(value >> 0x4) & 0xF];
	PX.buffer[3] = _hextab[(value >> 0x0) & 0xF];
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
	
	while(true){
		poll_input();
		if(JOY_START(joy0 | joy1)) return game_loop();
		
		px_spr_end();
		px_wait_nmi();
	}
	
	return game_loop();
}

#define GENE_00 0
#define GENE_L 0x10
#define GENE_R 0x20
#define GENE_WHOLE (GENE_L | GENE_R)
#define GENE_HELD 0x40

#define GENE_SHIFT 2
#define GENE_MASK 0x3
#define GENE_LMASK (GENE_L | 0xC)
#define GENE_RMASK (GENE_R | 0x3)

#define GENE_A0 (0x00 | GENE_L)
#define GENE_B0 (0x04 | GENE_L)
#define GENE_C0 (0x08 | GENE_L)
#define GENE_D0 (0x0C | GENE_L)

#define GENE_0A (0x00 | GENE_R)
#define GENE_0B (0x01 | GENE_R)
#define GENE_0C (0x02 | GENE_R)
#define GENE_0D (0x03 | GENE_R)

#define GENE_MAX 16

static u8 GENE_COUNT = 8;
static u8 GENE_X[GENE_MAX] = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32};
static u8 GENE_Y[GENE_MAX] = {0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0};
static u8 GENE_VALUE[GENE_MAX] = {
	GENE_A0 | GENE_0A,
	GENE_B0 | GENE_0B,
	GENE_C0 | GENE_0C,
	GENE_D0 | GENE_0D,
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
	GENE_A0 | GENE_0B,
	GENE_C0 | GENE_0D,
};

static void gene_remove(u8 i){
	--GENE_COUNT;
	GENE_X[i] = GENE_X[GENE_COUNT];
	GENE_Y[i] = GENE_Y[GENE_COUNT];
	GENE_VALUE[i] = GENE_VALUE[GENE_COUNT];
}

static void gene_draw(void){
	for(idx = 0; idx < GENE_COUNT; ++idx){
		ix = GENE_X[idx];
		iy = GENE_Y[idx];
		tmp = GENE_VALUE[idx];
		
		if(tmp & GENE_L){
			iz = (tmp >> GENE_SHIFT) & GENE_MASK;
			px_spr(ix - 8, iy - 0x10, 0x00 | iz, 0x20 + iz);
			px_spr(ix - 8, iy - 0x08, 0x80 | iz, 0x20 + iz);
		}
		
		if(tmp & GENE_R){
			iz = (tmp >> 0) & GENE_MASK;
			px_spr(ix + 0, iy - 0x10, 0x40 | iz, 0x20 + iz);
			px_spr(ix + 0, iy - 0x08, 0xC0 | iz, 0x20 + iz);
		}
	}
}

static s8 gene_at(u8 x, u8 y){
	for(idx = 0; idx < GENE_COUNT; ++idx){
		if(GENE_X[idx] == x && GENE_Y[idx] == y) return idx;
	}
	
	return -1;
}

#define PLAYER_SPEED 0x0180
#define GRAB_OFFSET(_player_) (_player_.flip ? 14 : -14)

typedef struct {
	u16 x, y;
	s16 vx, vy;
	
	u8 joy, prev_joy;
	bool flip;
	
	u8 palette;
	s8 gene_held;
} Player;

static Player PLAYER[2];

static const Player PLAYER_INIT[] = {
	{128 << 8, 128 << 8, 0, 0, 0x00, 0x00, false, 0x00, -1},
	{150 << 8, 128 << 8, 0, 0, 0x00, 0x00, false, 0x01, -1},
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
	
	if(JOY_BTN_B((player.joy ^ player.prev_joy) & player.joy)){
		if(player.gene_held >= 0){
			// Drop the gene.
			GENE_X[player.gene_held] = (GENE_X[player.gene_held] & 0xF0) + 0x08;
			GENE_Y[player.gene_held] = (GENE_Y[player.gene_held] + 0x08) & 0xF0;
			GENE_VALUE[player.gene_held] &= ~GENE_HELD;
			player.gene_held = -1;
			sound_play(SOUND_DROP);
		} else {
			// Search for a gene to pick up.
			for(idx = 0; idx < GENE_COUNT; ++idx){
				if(
					(GENE_VALUE[idx] & GENE_HELD) == 0 &&
					abs((s8)GENE_X[idx] - (s8)(player.x >> 8) - GRAB_OFFSET(player)) < 12 &&
					abs((s8)GENE_Y[idx] - (s8)(player.y >> 8)) < 12
				){
					player.gene_held = idx;
					GENE_VALUE[player.gene_held] |= GENE_HELD;
					sound_play(SOUND_PICKUP);
					break;
				}
			}
		}
	}
	
	if(JOY_BTN_A((player.joy ^ player.prev_joy) & player.joy)){
		{
			s8 i0 = gene_at(0x68, 0x50);
			s8 i1 = gene_at(0x78, 0x50);
			s8 i2 = gene_at(0x88, 0x50);
			
			if(i0 >=0 && i1 < 0 && i2 >= 0 && (GENE_VALUE[i0] & GENE_WHOLE) == GENE_L && (GENE_VALUE[i2] & GENE_WHOLE) == GENE_R){
				// Join the gene.
				GENE_VALUE[i0] |= GENE_VALUE[i2] & GENE_RMASK;
				GENE_X[i0] += 16;
				gene_remove(i2);
			}
		}
		{
			s8 i0 = gene_at(0x78, 0xB0);
			s8 i1 = gene_at(0x88, 0xB0);
			s8 i2 = gene_at(0x98, 0xB0);
			
			if(i0 < 0 && i1 >= 0 && i2 < 0 && (GENE_VALUE[i1] & GENE_WHOLE) == GENE_WHOLE){
				// Split the gene.
				GENE_X[GENE_COUNT] = GENE_X[i1] + 16;
				GENE_Y[GENE_COUNT] = GENE_Y[i1];
				GENE_VALUE[GENE_COUNT] = GENE_VALUE[i1] & ~GENE_LMASK;
				++GENE_COUNT;
				
				GENE_X[i1] -= 16;
				GENE_VALUE[i1] &= ~GENE_RMASK;
			}
		}
		{
			s8 i = gene_at(0xB8, 0x60);
			
			if(i >= 0){
				static const u8 BIT_SWAP_TABLE[] = {
					0x00, 0x05, 0x0A, 0x0F, 0x05, 0x00, 0x0F, 0x0A, 0x0A, 0x0F, 0x00, 0x05, 0x0F, 0x0A, 0x05, 0x00,
					0x30, 0x35, 0x3A, 0x3F, 0x35, 0x30, 0x3F, 0x3A, 0x3A, 0x3F, 0x30, 0x35, 0x3F, 0x3A, 0x35, 0x30,
					0x30, 0x35, 0x3A, 0x3F, 0x35, 0x30, 0x3F, 0x3A, 0x3A, 0x3F, 0x30, 0x35, 0x3F, 0x3A, 0x35, 0x30,
					0x00, 0x05, 0x0A, 0x0F, 0x05, 0x00, 0x0F, 0x0A, 0x0A, 0x0F, 0x00, 0x05, 0x0F, 0x0A, 0x05, 0x00
				};
				
				GENE_VALUE[i] ^= BIT_SWAP_TABLE[GENE_VALUE[i] & 0x3F];
			}
		}
	}
	
	if(JOY_SELECT(player.joy)){
		// debug_hex((GENE_X[0] << 0) | (GENE_Y[0] & 0));
		debug_hex(GENE_X[0] << 8);
	}
	
	if(player.gene_held >= 0){
		// GENE_VALUE[player.gene_held] = 0;
		GENE_X[player.gene_held] = (player.x >> 8) + GRAB_OFFSET(player);
		GENE_Y[player.gene_held] = (player.y >> 8);
	}
	
	player.prev_joy = joy;
	memcpy(_player, &player, sizeof(player));
}

static void player_draw(register Player *player){
	ix = player->x >> 8;
	iy = player->y >> 8;
	idx = (ix/2 + (iy/4)) & 0x3;
	idx *= 2;
	
	if(player->flip){
		iz = 0x40 | player->palette;
		px_spr(ix - 8, iy - 0x10, iz, 0x01 + (px_ticks & 0x7));
		px_spr(ix + 0, iy - 0x10, iz, 0x00);
		px_spr(ix - 8, iy - 0x08, iz, 0x11 + idx);
		px_spr(ix + 0, iy - 0x08, iz, 0x10 + idx);
	} else {
		iz = 0x00 | player->palette;
		px_spr(ix - 8, iy - 0x10, iz, 0x00);
		px_spr(ix + 0, iy - 0x10, iz, 0x01 + (px_ticks & 0x7));
		px_spr(ix - 8, iy - 0x08, iz, 0x10 + idx);
		px_spr(ix + 0, iy - 0x08, iz, 0x11 + idx);
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
		
		gene_draw();
		
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
	decompress_lz4_to_vram(CHR_ADDR(0, 0x00), gfx_tiles_lz4chr);
	
	// Load sprites.
	px_spr_table(1);
	decompress_lz4_to_vram(CHR_ADDR(1, 0x00), gfx_tiles_lz4chr);
	
	music_init(MUSIC);
	sound_init(SOUNDS);
	
	game_loop();
	main_menu();
}
