lz4_header_bytes = 8

.macro inclz4 symbol, file
	.export symbol
	symbol:
		.incbin file, lz4_header_bytes
		.word 0 ; terminator
.endmacro

; .segment "PRG0"
.rodata

inclz4 _gfx_tiles_lz4chr, "tiles.lz4chr"

inclz4 _gfx_menu_lz4, "menu.lz4"
inclz4 _gfx_credits_lz4, "credits.lz4"
inclz4 _gfx_level1_lz4, "level1.lz4"
inclz4 _gfx_game_over_lz4, "game_over.lz4"
