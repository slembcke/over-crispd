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
		if(JOY_START(joy0)){
		}
		
		px_spr_end();
		px_wait_nmi();
	}
	
	return game_loop();
}

static GameState game_loop(void){
	px_inc(PX_INC1);
	px_ppu_sync_off(); {
		decompress_lz4_to_vram(NT_ADDR(0, 0, 0), gfx_level1_lz4);
		px_spr_clear();
	} px_ppu_sync_on();
	
	wait_noinput();
	
	while(true){
		DEBUG_PROFILE_START();
		
		joy0 = joy_read(0);
		joy1 = joy_read(1);
		
		if(JOY_START(joy0 | joy1)) pause();
		
		// Do stuff here.
		
		px_spr_end();
		DEBUG_PROFILE_END();
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
	
	main_menu();
}
