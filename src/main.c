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
	CLR_BG, 0x24, 0x14, 0x04,
	CLR_BG, 0x28, 0x17, CLR_BG,
	CLR_BG, 0x20, 0x2D, 0x1D,
	
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
	px_buffer_inc(PX_INC1);
	px_buffer_data(4, NT_ADDR(0, 1, 1));
	PX.buffer[0] = _hextab[(value >> 0xC) & 0xF];
	PX.buffer[1] = _hextab[(value >> 0x8) & 0xF];
	PX.buffer[2] = _hextab[(value >> 0x4) & 0xF];
	PX.buffer[3] = _hextab[(value >> 0x0) & 0xF];
}

static GameState main_menu(void);
static GameState game_loop(u8 sequence);
static void pause(void);

static GameState main_menu(void){
	px_inc(PX_INC1);
	px_ppu_sync_off(); {
		decompress_lz4_to_vram(NT_ADDR(0, 0, 0), gfx_menu_lz4);
	} px_ppu_sync_on();
	
	wait_noinput();
	
	while(true){
		poll_input();
		if(JOY_START(joy0 | joy1)) break;
		
		++idx;
		
		px_spr_end();
		px_wait_nmi();
	}
	
	return game_loop(idx & 0xF);
}

#define BUTTON_BIT 0x04
#define NON_WALKABLE_BIT 0x08
#define STORAGE_BIT 0x10
#define FULL_BIT 0x20

#define MPTY 0x00
#define SBUT (BUTTON_BIT | 0x00)
#define DBUT (BUTTON_BIT | 0x01)
#define RBUT (BUTTON_BIT | 0x02)
#define GBUT (BUTTON_BIT | 0x03)
#define WALL (NON_WALKABLE_BIT)
#define STOR (NON_WALKABLE_BIT | STORAGE_BIT)
#define FULL (NON_WALKABLE_BIT | STORAGE_BIT | FULL_BIT)

// Uff, running out of time. Gotta just cram this in.
static u8 MAP[] = {
//  0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
	WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, // 0
	WALL, FULL, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, STOR, WALL, WALL, STOR, WALL, // 1
	WALL, FULL, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, WALL, WALL, STOR, WALL, // 2
	WALL, FULL, MPTY, RBUT, MPTY, WALL, WALL, WALL, WALL, WALL, MPTY, MPTY, WALL, WALL, STOR, WALL, // 3
	WALL, FULL, MPTY, MPTY, MPTY, WALL, STOR, MPTY, MPTY, WALL, SBUT, MPTY, WALL, WALL, STOR, WALL, // 4
	WALL, FULL, MPTY, MPTY, MPTY, WALL, WALL, MPTY, MPTY, WALL, WALL, WALL, WALL, WALL, STOR, WALL, // 5
	WALL, FULL, MPTY, MPTY, MPTY, WALL, STOR, MPTY, MPTY, MPTY, MPTY, MPTY, WALL, WALL, STOR, WALL, // 6
	WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, WALL, WALL, STOR, WALL, // 7
	WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, MPTY, STOR, WALL, // 8
	WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, STOR, WALL, // 9
	WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, STOR, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, STOR, WALL, // A
	WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, STOR, WALL, // B
	WALL, WALL, MPTY, MPTY, MPTY, WALL, WALL, WALL, WALL, MPTY, MPTY, MPTY, MPTY, MPTY, STOR, WALL, // C
	WALL, WALL, MPTY, MPTY, MPTY, WALL, DBUT, MPTY, MPTY, MPTY, MPTY, GBUT, MPTY, MPTY, STOR, WALL, // D
	WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, WALL, // E
};

#define MAP_BLOCK_AT(x, y) ((y & 0xF0)| (x >> 4))

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

#define GENE_AA (GENE_A0 | GENE_0A)
#define GENE_AB (GENE_A0 | GENE_0B)
#define GENE_AC (GENE_A0 | GENE_0C)
#define GENE_AD (GENE_A0 | GENE_0D)
#define GENE_BA (GENE_B0 | GENE_0A)
#define GENE_BB (GENE_B0 | GENE_0B)
#define GENE_BC (GENE_B0 | GENE_0C)
#define GENE_BD (GENE_B0 | GENE_0D)
#define GENE_CA (GENE_C0 | GENE_0A)
#define GENE_CB (GENE_C0 | GENE_0B)
#define GENE_CC (GENE_C0 | GENE_0C)
#define GENE_CD (GENE_C0 | GENE_0D)
#define GENE_DA (GENE_D0 | GENE_0A)
#define GENE_DB (GENE_D0 | GENE_0B)
#define GENE_DC (GENE_D0 | GENE_0C)
#define GENE_DD (GENE_D0 | GENE_0D)

#define GENE_MAX 12

static u8 GENE_SETS[][6] = {
	{GENE_AD, GENE_AD, GENE_CC, GENE_CB, GENE_AD, GENE_BB},
	{GENE_AA, GENE_CD, GENE_DB, GENE_CB, GENE_BD, GENE_CA},
	{GENE_AD, GENE_AC, GENE_CA, GENE_BB, GENE_CB, GENE_DD},
	{GENE_BC, GENE_BD, GENE_AC, GENE_BD, GENE_AC, GENE_DA},
	{GENE_DB, GENE_AC, GENE_CA, GENE_AB, GENE_BD, GENE_CD},
	{GENE_CB, GENE_CB, GENE_AA, GENE_DB, GENE_CD, GENE_DA},
	{GENE_CD, GENE_AD, GENE_CB, GENE_BD, GENE_AA, GENE_CB},
	{GENE_DA, GENE_AC, GENE_BB, GENE_BC, GENE_AD, GENE_DC},
	{GENE_DB, GENE_AC, GENE_BA, GENE_CD, GENE_CD, GENE_AB},
	{GENE_DC, GENE_BA, GENE_BA, GENE_CD, GENE_AD, GENE_CB},
	{GENE_CA, GENE_DA, GENE_DB, GENE_BC, GENE_CB, GENE_DA},
	{GENE_BC, GENE_AA, GENE_BD, GENE_DA, GENE_DC, GENE_BC},
	{GENE_BD, GENE_BC, GENE_AB, GENE_AC, GENE_DD, GENE_CA},
	{GENE_AD, GENE_BC, GENE_AC, GENE_AB, GENE_DC, GENE_BD},
	{GENE_AD, GENE_CC, GENE_BA, GENE_AB, GENE_CD, GENE_DB},
	{GENE_CA, GENE_CA, GENE_DB, GENE_DB, GENE_DC, GENE_AB},
};

static u8 EXPECTED_SETS[][6] = {
	{GENE_CD, GENE_BC, GENE_CD, GENE_DA, GENE_AB, GENE_AB},
	{GENE_DC, GENE_DA, GENE_CC, GENE_BB, GENE_DA, GENE_AB},
	{GENE_BA, GENE_BA, GENE_DA, GENE_CC, GENE_DB, GENE_DC},
	{GENE_DC, GENE_AD, GENE_AB, GENE_AD, GENE_CC, GENE_BB},
	{GENE_BA, GENE_BA, GENE_CB, GENE_DC, GENE_DC, GENE_AD},
	{GENE_AB, GENE_DD, GENE_AC, GENE_AC, GENE_DC, GENE_BB},
	{GENE_BA, GENE_DD, GENE_AC, GENE_BC, GENE_DA, GENE_BC},
	{GENE_CA, GENE_CD, GENE_CD, GENE_DB, GENE_AB, GENE_AB},
	{GENE_CA, GENE_DC, GENE_AA, GENE_CB, GENE_BB, GENE_DD},
	{GENE_AC, GENE_AC, GENE_CA, GENE_DD, GENE_BB, GENE_DB},
	{GENE_BB, GENE_DC, GENE_BA, GENE_CD, GENE_AA, GENE_CD},
	{GENE_DB, GENE_AB, GENE_CD, GENE_AB, GENE_CD, GENE_CA},
	{GENE_DC, GENE_AD, GENE_BB, GENE_BA, GENE_DA, GENE_CC},
	{GENE_CA, GENE_DB, GENE_CA, GENE_CB, GENE_DB, GENE_DA},
	{GENE_AC, GENE_BD, GENE_AA, GENE_BC, GENE_DC, GENE_BD},
	{GENE_DA, GENE_DD, GENE_AC, GENE_BB, GENE_BA, GENE_CC},
};

static u8 GENE_COUNT = 6;
static u8 GENE_X[GENE_MAX] = {24, 24, 24, 24, 24, 24};
static u8 GENE_Y[GENE_MAX] = {0x20, 0x30, 0x40, 0x50, 0x60, 0x70};
static u8 GENE_VALUE[GENE_MAX];
static const u8 EXPECTED_GENES[6];

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
			if((tmp & GENE_R) == 0) iz += 4;
			px_spr(ix - 8, iy - 0x10, 0x00 | iz, 0x20 + iz);
			px_spr(ix - 8, iy - 0x08, 0x80 | iz, 0x20 + iz);
		}
		
		if(tmp & GENE_R){
			iz = (tmp >> 0) & GENE_MASK;
			if((tmp & GENE_L) == 0) iz += 4;
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

static void gene_splice(void){
	s8 i0 = gene_at(0x68, 0x50);
	s8 i1 = gene_at(0x68, 0x60);
	s8 i2 = gene_at(0x68, 0x70);
	
	if(i0 >=0 && i1 < 0 && i2 >= 0 && ((GENE_VALUE[i0] ^ GENE_VALUE[i2]) & GENE_WHOLE) == GENE_WHOLE){
		// Join the gene.
		GENE_VALUE[i0] |= GENE_VALUE[i2] & (GENE_LMASK | GENE_RMASK);
		GENE_Y[i0] += 16;
		gene_remove(i2);
		
		MAP[0x46] &= ~FULL_BIT;
		MAP[0x56] |=  FULL_BIT;
		MAP[0x66] &= ~FULL_BIT;
	}
}

static void gene_dice(void){
	s8 i0 = gene_at(0x78, 0xA0);
	s8 i1 = gene_at(0x78, 0xB0);
	s8 i2 = gene_at(0x78, 0xC0);
	
	if(i0 < 0 && i1 >= 0 && i2 < 0 && (GENE_VALUE[i1] & GENE_WHOLE) == GENE_WHOLE){
		// Split the gene.
		GENE_X[GENE_COUNT] = GENE_X[i1];
		GENE_Y[GENE_COUNT] = GENE_Y[i1] + 16;
		GENE_VALUE[GENE_COUNT] = GENE_VALUE[i1] & ~GENE_LMASK;
		++GENE_COUNT;
		
		GENE_Y[i1] -= 16;
		GENE_VALUE[i1] &= ~GENE_RMASK;
		
		MAP[0x97] |=  FULL_BIT;
		MAP[0xA7] &= ~FULL_BIT;
		MAP[0xB7] |=  FULL_BIT;
	}
}

static void gene_rotate(void){
	s8 i = gene_at(0xB8, 0x20);
	
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

static void blit_conveyor(u16 addr, u8 base);

static void gene_check(){
	s8 i;
	u8 match_count = 0;
	
	i = gene_at(0xE8, 0x90);
	if(i >= 0 && GENE_VALUE[idx] == EXPECTED_GENES[0]) match_count += 1;
	
	i = gene_at(0xE8, 0xA0);
	if(i >= 0 && GENE_VALUE[idx] == EXPECTED_GENES[1]) match_count += 1;
	
	i = gene_at(0xE8, 0xB0);
	if(i >= 0 && GENE_VALUE[idx] == EXPECTED_GENES[2]) match_count += 1;
	
	i = gene_at(0xE8, 0xC0);
	if(i >= 0 && GENE_VALUE[idx] == EXPECTED_GENES[3]) match_count += 1;
	
	i = gene_at(0xE8, 0xD0);
	if(i >= 0 && GENE_VALUE[idx] == EXPECTED_GENES[4]) match_count += 1;
	
	i = gene_at(0xE8, 0xE0);
	if(i >= 0 && GENE_VALUE[idx] == EXPECTED_GENES[5]) match_count += 1;
	
	if(match_count == 6){
		u8 i;
		
		// Play a silly animation.
		for(i = 0; i < 7*16; ++i){
			blit_conveyor(NT_ADDR(0, 28, 2), 0xE0);
			blit_conveyor(NT_ADDR(0, 29, 2), 0xE1);
			
			for(ix = 0; ix < GENE_COUNT; ++ix) GENE_Y[ix] -= 1;
			
			gene_draw();
			
			px_spr_end();
			px_wait_nmi();
		}
		
		sound_play(SOUND_MATCH);
		px_wait_frames(240);
		
		// Restart the game.
		exit(0);
	}
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
	
	u8 button;
} Player;

static Player PLAYER[2];

static const Player PLAYER_INIT[] = {
	{128 << 8, 128 << 8, 0, 0, 0x00, 0x00, false, 0x00, -1, 0},
	{150 << 8, 128 << 8, 0, 0, 0x00, 0x00, false, 0x01, -1, 0},
};

static void player_update(register Player *_player, u8 joy){
	static s8 edge;
	
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
	
	ix = (player.x >> 8);
	iy = (player.y >> 8) - 4;
	
	// Collide with the block below.
	iz = MAP_BLOCK_AT(ix, iy);
	idx = (MAP + 16)[iz] & NON_WALKABLE_BIT;
	edge = 6 - (~iy & 0x0F);
	if(idx && edge >= 0){
		player.y -= (edge << 8);
	}
	
	// Collide with the block above.
	iz = MAP_BLOCK_AT(ix, iy);
	idx = (MAP - 16)[iz] & NON_WALKABLE_BIT;
	edge = (~iy & 0x0F) - 10;
	if(idx && edge >= 0){
		player.y += (edge << 8);
	}
	
	// Collide with the block to the right
	iz = MAP_BLOCK_AT(ix, iy);
	idx = (MAP + 1)[iz] & NON_WALKABLE_BIT;
	edge = 6 - (~ix & 0x0F);
	if(idx && edge >= 0){
		player.x -= (edge << 8);
	}
	
	// Collide with the block to the right
	iz = MAP_BLOCK_AT(ix, iy);
	idx = (MAP - 1)[iz] & NON_WALKABLE_BIT;
	edge = (~ix & 0x0F) - 8;
	if(idx && edge >= 0){
		player.x += (edge << 8);
	}
	
	if(JOY_BTN_B((player.joy ^ player.prev_joy) & player.joy)){
		if(player.gene_held >= 0){
			ix = (player.x >> 8) + GRAB_OFFSET(player);
			iy = (player.y >> 8) - 8;
			iz = MAP_BLOCK_AT(ix, iy);
			idx = MAP[iz];
			if((idx & STORAGE_BIT) && !(idx & FULL_BIT)){
				// Drop the gene.
				GENE_X[player.gene_held] = (GENE_X[player.gene_held] & 0xF0) + 0x08;
				GENE_Y[player.gene_held] = (GENE_Y[player.gene_held] + 0x08) & 0xF0;
				GENE_VALUE[player.gene_held] &= ~GENE_HELD;
				
				// Mark the slot as full.
				MAP[iz] |= FULL_BIT;
				
				player.gene_held = -1;
				sound_play(SOUND_DROP);
			}
		} else {
			// Search for a gene to pick up.
			// Holy crap this is a trash fire, lol.
			for(idx = 0; idx < GENE_COUNT; ++idx){
				ix = (player.x >> 8) + GRAB_OFFSET(player);
				iy = (player.y >> 8) - 8;
				if(
					(((GENE_X[idx] - 8) ^ ix) & 0xF0) == 0 &&
					(((GENE_Y[idx] - 8) ^ iy) & 0xF0) == 0 &&
					(GENE_VALUE[idx] & GENE_HELD) == 0
				){
					player.gene_held = idx;
					sound_play(SOUND_PICKUP);
					
					// Mark the gene as held.
					GENE_VALUE[player.gene_held] |= GENE_HELD;
					
					// Mark the map as open.
					iz = MAP_BLOCK_AT(ix, iy);
					MAP[iz] &= ~FULL_BIT;
					
					break;
				}
			}
		}
	}
	
	if(player.gene_held >= 0){
		// GENE_VALUE[player.gene_held] = 0;
		GENE_X[player.gene_held] = (player.x >> 8) + GRAB_OFFSET(player);
		GENE_Y[player.gene_held] = (player.y >> 8);
	}
	
	ix = (player.x >> 8);
	iy = (player.y >> 8);
	iz = MAP_BLOCK_AT(ix, iy);
	idx = MAP[iz];
	if(idx & BUTTON_BIT){
		u8 button = idx;
		if(button == SBUT && player.button == 0xFF) gene_splice();
		if(button == DBUT && player.button == 0xFF) gene_dice();
		if(button == RBUT && player.button == 0xFF) gene_rotate();
		if(button == GBUT && player.button == 0xFF) gene_check();
		player.button = button;
	} else {
		player.button = 0xFF;
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

static void blit_conveyor(u16 addr, u8 base){
	ix = base + 0x00 + (px_ticks & 0xE);
	iy = ix + 0x10;
	
	px_buffer_inc(PX_INC32);
	px_buffer_data(26, addr);
	PX.buffer[0x00] = ix;
	PX.buffer[0x01] = iy;
	PX.buffer[0x02] = ix;
	PX.buffer[0x03] = iy;
	PX.buffer[0x04] = ix;
	PX.buffer[0x05] = iy;
	PX.buffer[0x06] = ix;
	PX.buffer[0x07] = iy;
	PX.buffer[0x08] = ix;
	PX.buffer[0x09] = iy;
	PX.buffer[0x0A] = ix;
	PX.buffer[0x0B] = iy;
	PX.buffer[0x0C] = ix;
	PX.buffer[0x0D] = iy;
	PX.buffer[0x0E] = ix;
	PX.buffer[0x0F] = iy;
	PX.buffer[0x10] = ix;
	PX.buffer[0x11] = iy;
	PX.buffer[0x12] = ix;
	PX.buffer[0x13] = iy;
	PX.buffer[0x14] = ix;
	PX.buffer[0x15] = iy;
	PX.buffer[0x16] = ix;
	PX.buffer[0x17] = iy;
	PX.buffer[0x18] = ix;
	PX.buffer[0x19] = iy;
}

static const u8 BLOCK_CHR[] = {0x2F, 0x2E, 0x2E, 0x2E};
static const u8 BLOCK_PAL[] = {0x00, 0x55, 0xAA, 0xFF};
static const u8 BLOCK_MASK[] = {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00};

static GameState game_loop(u8 sequence){
	memcpy(GENE_VALUE, GENE_SETS[sequence], 6);
	memcpy(EXPECTED_GENES, EXPECTED_SETS[sequence], 6);
	
	px_inc(PX_INC1);
	px_ppu_sync_off(); {
		static u8 attrib_mem[8];
		
		decompress_lz4_to_vram(NT_ADDR(0, 0, 0), gfx_level1_lz4);
		
		// Need to copy the expected gene sequence into PPU memory.
		// Cripses this code is rushed... Don't look!
		for(idx = 0; idx < sizeof(EXPECTED_GENES); ++idx){
			u8 block = EXPECTED_GENES[idx];
			u8 block_l = BLOCK_CHR[(block >> 2) & 0x3];
			u8 block_r = BLOCK_CHR[(block >> 0) & 0x3];
			u16 addr = NT_ADDR(0, 26, 2 + 2*idx);
			
			px_addr(addr + 0x00);
			PPU.vram.data = block_l;
			PPU.vram.data = block_l;
			px_addr(addr + 0x20);
			PPU.vram.data = block_l;
			PPU.vram.data = block_l;
			
			px_addr(addr + 0x04);
			PPU.vram.data = block_r;
			PPU.vram.data = block_r;
			px_addr(addr + 0x24);
			PPU.vram.data = block_r;
			PPU.vram.data = block_r;
		}
		
		// I don't want to know what the assembly for this looks like...
		attrib_mem[0] = (EXPECTED_GENES[0] & 0xC) << 4;
		attrib_mem[1] = (EXPECTED_GENES[0] & 0x3) << 6;
		attrib_mem[2] = (EXPECTED_GENES[1] & 0xC) << 0 | (EXPECTED_GENES[2] & 0xC) << 4;
		attrib_mem[3] = (EXPECTED_GENES[1] & 0x3) << 2 | (EXPECTED_GENES[2] & 0x3) << 6;
		attrib_mem[4] = (EXPECTED_GENES[3] & 0xC) << 0 | (EXPECTED_GENES[4] & 0xC) << 4;
		attrib_mem[5] = (EXPECTED_GENES[3] & 0x3) << 2 | (EXPECTED_GENES[4] & 0x3) << 6;
		attrib_mem[6] = (EXPECTED_GENES[5] & 0xC) << 0;
		attrib_mem[7] = (EXPECTED_GENES[5] & 0x3) << 2;
		
		px_addr(AT_ADDR(0) + 0x06);
		PPU.vram.data = attrib_mem[0];
		PPU.vram.data = attrib_mem[1];
		px_addr(AT_ADDR(0) + 0x0E);
		PPU.vram.data = attrib_mem[2];
		PPU.vram.data = attrib_mem[3];
		px_addr(AT_ADDR(0) + 0x16);
		PPU.vram.data = attrib_mem[4];
		PPU.vram.data = attrib_mem[5];
		px_addr(AT_ADDR(0) + 0x1E);
		PPU.vram.data = attrib_mem[6];
		PPU.vram.data = attrib_mem[7];
		
		px_spr_clear();
	} px_ppu_sync_on();
	
	PLAYER[0] = PLAYER_INIT[0];
	PLAYER[1] = PLAYER_INIT[1];
	
	wait_noinput();
	while(true){
		// px_profile_start();
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
		// px_profile_end();
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
	
	main_menu();
}
