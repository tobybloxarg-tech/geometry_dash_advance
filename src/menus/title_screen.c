#include <tonc.h>
#include "main.h"
#include "level_select.h"
#include "tonc_tte.h"
#include "pusab.h"
#include "posprintf.h"
#include <maxmod.h>
#include "soundbank.bin.h"
#include "soundbank.h"
#include "physics_defines.h"
#include <string.h>
#include <ctype.h>
#include "color.h"
#include "icon_kit.h"

EWRAM_DATA u32 title_screen_color_index = 0;
EWRAM_DATA u16 title_screen_color_transition_backup[6];
EWRAM_DATA u8 title_screen_player_hold;
EWRAM_DATA u32 title_screen_scroll_x;

void title_screen_players();
void reset_title_screen_player();
void draw_button_glyphs_title_screen();

/* BG0 and BG1 title maps use tiles at most 489; the original copies
 * populate 0-127 and 256-511. Custom labels live at 768-803 (VRAM 0x6000),
 * ahead of the three screenblocks at 0xD000, 0xD800, and 0xE000.
 * Access as character block 1 + 256, not tile_mem[0][768]: Tonc's
 * character-block subarray only has 512 TILEs, even though VRAM is linear.
 * BG0 uses CBB 0, so its screen entries still address them as 768-803.
 * Palette bank 4 is unused by all three title backgrounds and is not touched
 * by menu_set_bg_color or the player palette updates. */
#define CREATE_BUTTON_PAL 4

static void draw_title_label(const char *label, unsigned text_tiles, unsigned tile_base, unsigned col) {
    unsigned LEFT_TILE = text_tiles, RIGHT_TILE = text_tiles+1, TOP_TILE = text_tiles+2, BOTTOM_TILE = text_tiles+3, WIDTH_TILES = text_tiles+2;
    TILE *tiles = &tile_mem[1][tile_base-512];
    unsigned i, row, pixel;

    /* Pusab stores one 8x8 4bpp tile per ASCII character. Its 1/2/3
     * pixels are white/black/gray; index 4 is our opaque dark fill. */
    for (i = 0; i < text_tiles; i++) {
        for (row = 0; row < 8; row++) {
            u32 source = pusabGlyphs[(unsigned char)label[i] * 8 + row];
            u32 result = 0;
            for (pixel = 0; pixel < 8; pixel++) {
                u32 color = (source >> (pixel * 4)) & 15;
                result |= (color ? color : 4) << (pixel * 4);
            }
            tiles[i].data[row] = result;
        }
    }
    for (row = 0; row < 8; row++) {
        tiles[LEFT_TILE].data[row] = 0x44444445;
        tiles[RIGHT_TILE].data[row] = 0x54444444;
        tiles[TOP_TILE].data[row] = row == 0 ? 0x55555555 : 0x44444444;
        tiles[BOTTOM_TILE].data[row] = row == 7 ? 0x55555555 : 0x44444444;
    }
    palette_buffer[CREATE_BUTTON_PAL * 16 + 1] = CLR_WHITE;
    palette_buffer[CREATE_BUTTON_PAL * 16 + 2] = 0x0000;
    palette_buffer[CREATE_BUTTON_PAL * 16 + 3] = 0x5294;
    palette_buffer[CREATE_BUTTON_PAL * 16 + 4] = 0x2000;
    palette_buffer[CREATE_BUTTON_PAL * 16 + 5] = 0x03FF;

    /* Top row labels, priority 0 above BG1/BG2. */
    for (i = 0; i < WIDTH_TILES; i++) {
        se_mem[26][col + i] = (CREATE_BUTTON_PAL << 12) | (tile_base + TOP_TILE);
        se_mem[26][2 * 32 + col + i] = (CREATE_BUTTON_PAL << 12) | (tile_base + BOTTOM_TILE);
    }
    se_mem[26][32 + col] = (CREATE_BUTTON_PAL << 12) | (tile_base + LEFT_TILE);
    for (i = 0; i < text_tiles; i++)
        se_mem[26][32 + col + 1 + i] = (CREATE_BUTTON_PAL << 12) | (tile_base + i);
    se_mem[26][32 + col + WIDTH_TILES - 1] = (CREATE_BUTTON_PAL << 12) | (tile_base + RIGHT_TILE);
}

static void draw_create_button(void) {
    draw_title_label("B: CREAR", 8, 768, 12);
    draw_title_label("UP: FAN 2", 9, 780, 1);
    draw_title_label("DOWN:OP", 7, 793, 22);
}

extern u16 __key_curr;

INLINE void do_button_draw() {
    nextSpr = 0;
    
    draw_button_glyphs_title_screen();
    
    obj_copy(oam_mem, shadow_oam, 128);
    obj_aff_copy(obj_aff_mem, obj_aff_buffer, 32);
}
u32 draw_sprite_number(const u32 x, const u32 y, const u32 value, const u32 vram_index, const u16* number_metasprite, const u32 priority);

void title_screen_loop() {
    REG_DISPCNT = DCNT_MODE0 | DCNT_OBJ | DCNT_OBJ_1D | DCNT_BG0 | DCNT_BG1 | DCNT_BG2;

    REG_BG0CNT  = BG_CBB(0) | BG_SBB(26) | BG_REG_32x32 | BG_PRIO(0);
    REG_BG0HOFS = 8;
    REG_BG0VOFS = 0;

    REG_BG1CNT  = BG_CBB(0) | BG_SBB(27) | BG_REG_32x32 | BG_PRIO(1);
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;

    REG_BG2CNT  = BG_CBB(0) | BG_SBB(28) | BG_REG_32x32 | BG_PRIO(2);
    REG_BG2HOFS = 0;
    REG_BG2VOFS = 96;

    memset32(palette_buffer, 0, 256);
    memcpy16(palette_buffer, menu_palette, sizeof(menu_palette) / sizeof(COLOR));
    palette_buffer[0x24] = palette_buffer[0];

    memcpy32(&tile_mem[0][0], square_background_chr, sizeof(square_background_chr) / 8);
    memcpy32(&tile_mem[0][256], title_screen_chr, sizeof(title_screen_chr) / 8);
    memcpy32(&tile_mem_obj[0][992], level_text_chr, sizeof(TILE) * 32 / 4);
    memcpy32(&se_mem[26][0], title_screen_l0_tilemap, sizeof(title_screen_l0_tilemap) / sizeof(u32));
    memcpy32(&se_mem[27][0], title_screen_l1_tilemap, sizeof(title_screen_l1_tilemap) / sizeof(u32));
    memcpy32(&se_mem[28][0], square_background_tilemap, sizeof(square_background_tilemap) / sizeof(u32));
    draw_create_button();

    // Button glyph chr
    memcpy16(&palette_buffer[0x1f0], button_glyph_pal, sizeof(button_glyph_pal) / sizeof(COLOR));
    memcpy32(&tile_mem_obj[0][512], &title_screen_chr[0xf0], 24 * sizeof(TILE8) / 4);
    
    // Restore bg color
    memcpy16(&col_trigger_buffer[0], title_screen_color_transition_backup, 6);
    do_menu_color_transition();

    scroll_x = 0;
    scroll_y = BOTTOM_SCROLL_LIMIT + TO_FIXED(14);
    title_screen_scroll_x = 0;

    memset32(shadow_oam, ATTR0_HIDE, 256);
    memset16(rotation_buffer, 0x0000, NUM_ROT_SLOTS);
    memset16(rotation_flags_buffer, 0x0000, NUM_ROT_SLOTS);
    
    do_button_draw();
    
    fade_in();

    memset32(&player_1, 0, sizeof(player_1) / sizeof(u32));
    memset32(level_buffer, 0x0000, sizeof(level_buffer) / sizeof(u32));
    memset32(object_buffer, 0x0000, sizeof(object_buffer) / 4);
    

    // Set default player colors
    set_player_colors(palette_buffer, palette_kit_colors[save_data.p1_col_selected], palette_kit_colors[save_data.p2_col_selected], palette_kit_colors[save_data.glow_col_selected]);
    palette_buffer[0x10e] = CLR_WHITE;

    reset_title_screen_player();

    put_ground();

    mirror_scaling = float2fx(1.0);
    
    while (1) {
        key_poll();
    
        REG_BG1HOFS = title_screen_scroll_x >> SUBPIXEL_BITS;
        REG_BG2HOFS = title_screen_scroll_x >> (3+SUBPIXEL_BITS);
        
        obj_copy(oam_mem, shadow_oam, 128);
        obj_aff_copy(obj_aff_mem, obj_aff_buffer, 32);

        if (key_hit(KEY_UP)) { game_state = STATE_FAN_MENU; break; }
        if (key_hit(KEY_DOWN)) { game_state = STATE_OPTIONS; break; }

        // B opens the prototype editor; the other title actions remain unchanged.
        if (key_hit(KEY_B)) {
            game_state = STATE_EDITOR;
            break;
        }

        // Level select
        if (key_hit(KEY_A) | key_hit(KEY_START)) {
            custom_levels = FALSE;
            game_state = STATE_LEVEL_SELECT;
            break;
        }

        // Icon kit
        if (key_hit(KEY_L)) {
            game_state = STATE_ICON_KIT;
            break;
        }

        // Custom level menu
        if (key_hit(KEY_R)) {
            custom_levels = TRUE;
            game_state = STATE_LEVEL_SELECT;
            break;
        }

        // Sound test
        if (key_hit(KEY_SELECT)) {
            game_state = STATE_SOUND_TEST;
            break;
        }

        // Do scroll
        title_screen_scroll_x += SPEED_1x;

        // Wrap around
        if (title_screen_scroll_x >= 0x80000000) title_screen_scroll_x -= 0x80000000;

        do_menu_color_transition();
        
        // Copy palette from buffer
        memcpy32(pal_bg_mem, palette_buffer, 256);

        // If the current transition ended, start a new one to the next color
        if (!col_trigger_buffer[0][COL_TRIG_BUFF_ACTIVE]) {
            col_trigger_buffer[0][COL_TRIG_BUFF_ACTIVE] = TRUE;
            col_trigger_buffer[0][COL_TRIG_BUFF_OLD_COLOR] = palette_buffer[BG_COLOR];
            col_trigger_buffer[0][COL_TRIG_BUFF_NEW_COLOR] = menu_bg_colors[title_screen_color_index % (sizeof(menu_bg_colors) / sizeof(COLOR))];
            col_trigger_buffer[0][COL_TRIG_BUFF_TOTAL_FRAMES] = 240;
            col_trigger_buffer[0][COL_TRIG_BUFF_CURRENT_FRAMES] = 0;
            title_screen_color_index++;
        }

        memset32(shadow_oam, ATTR0_HIDE, 256);
        memset16(rotation_buffer, 0x0000, NUM_ROT_SLOTS);
        memset16(rotation_flags_buffer, 0x0000, NUM_ROT_SLOTS);

        nextSpr = 0;

        draw_button_glyphs_title_screen();
        
        // Handle title screen players
        title_screen_players();
        
        sort_oam_by_prio();
        
        // Wait for VSYNC
        VBlankIntrWait();
    }
    
    memcpy16(title_screen_color_transition_backup, &col_trigger_buffer[0], 6);
    fade_out();
}

void draw_button_glyphs_title_screen() {
    oam_metaspr(112, 100, menuButton, FALSE, FALSE, 512, 15, PRIORITY_DONT_DISABLE_0 | 0, 0, TRUE, FALSE); // A
    oam_metaspr(112, 140, menuButton, FALSE, FALSE, 528, 15, PRIORITY_DONT_DISABLE_0 | 0, 0, TRUE, FALSE); // SELECT
    oam_metaspr( 56,  94, menuButton, FALSE, FALSE, 524, 15, PRIORITY_DONT_DISABLE_0 | 0, 0, TRUE, FALSE); // L
    oam_metaspr(168,  94, menuButton, FALSE, FALSE, 520, 15, PRIORITY_DONT_DISABLE_0 | 0, 0, TRUE, FALSE); // R
}

void reset_title_screen_player() {
    gravity_multiplier = TO_FIXED(1);
    player_1.player_x = TO_FIXED(-16);
    player_1.player_y = ((GROUND_HEIGHT - 1) << (4 + SUBPIXEL_BITS)) + (0x2 << SUBPIXEL_BITS);
    player_1.player_y_speed = 0;

    memset16(trail_enabled, 0x0000, sizeof(trail_enabled) / sizeof(u16));
    
    // 1/4 chance for player to be mini
    if (!(qran() & 0b11)) player_1.player_size = SIZE_MINI;
    else player_1.player_size = SIZE_BIG;
    
    u32 gamemode = qran() % GAMEMODE_COUNT;
    // Get random gamemode
    player_1.gamemode = gamemode;
    
    u32 random_icon = 0;
    switch (gamemode) {
        case GAMEMODE_CUBE:
            random_icon = qran() % ICON_COUNT_CUBES; 
            break;
        case GAMEMODE_SHIP:
            random_icon = qran() % ICON_COUNT_SHIPS; 
            break;
        case GAMEMODE_BALL:
            random_icon = qran() % ICON_COUNT_BALLS; 
            break;
        case GAMEMODE_UFO:
            random_icon = qran() % ICON_COUNT_UFOS; 
            break;
        case GAMEMODE_WAVE:
            random_icon = qran() % ICON_COUNT_WAVES; 
            break;
    }
    
    upload_ply_chr(gamemode, random_icon, ID_PLAYER_1, qran() & 0b1);
    
    // Choose random colors
    u32 p1 = qran() % NUM_COLORS;
    u32 p2 = qran() % NUM_COLORS;
    set_player_colors(palette_buffer, palette_kit_colors[p1], palette_kit_colors[p2], palette_kit_colors[qran() & 0b1 ? p1 : p2]);
    
    // Reset sprites
    obj_copy(oam_mem, shadow_oam, 128);
    obj_aff_copy(obj_aff_mem, obj_aff_buffer, 32);
    memset32(shadow_oam, ATTR0_HIDE, 256);
    
    nextSpr = 0;
    draw_button_glyphs_title_screen();
    sort_oam_by_prio();

    // Get random speed
    speed_id = qran() % SPEED_COUNT;

    wave_trail_pointer[ID_PLAYER_1] = 0;
}

void title_screen_players() {
    /* The demo player uses Tonc's global key state for synthetic input.
     * Preserve the physical key history so the next key_poll() can detect
     * real button edges correctly, including buttons held across frames. */
    u16 real_key_curr = __key_curr;
    u16 real_key_prev = __key_prev;
    curr_player_id = ID_PLAYER_1;
    curr_player = player_1;
    
    curr_player.player_x_speed = speed_constants[speed_id];
    
    curr_player.relative_player_x = FROM_FIXED(curr_player.player_x) - 32;
    curr_player.relative_player_y = FROM_FIXED(curr_player.player_y) - FROM_FIXED(scroll_y) - 14;

    // Disable input
    __key_curr = 0;
    __key_prev = 0;

    // Set movement depending on gamemode
    switch (curr_player.gamemode) {
        case GAMEMODE_CUBE:
            // Press A 1/16 of time
            if (!(qran() & 0b1111)) __key_curr = KEY_A;
            break;
        case GAMEMODE_UFO:
            // Press A 1/32 of time
            if (!(qran() & 0b11111)) __key_curr = KEY_A;
            break;
        case GAMEMODE_WAVE:
            // Switch holding A 1/16 of time
            if (!(qran() & 0b1111)) title_screen_player_hold ^= 1;

            if (title_screen_player_hold) {
                __key_curr = KEY_A;
            }
            break;
        case GAMEMODE_SHIP:
            // Switch holding A 1/16 of time
            if (!(qran() & 0b1111)) title_screen_player_hold ^= 1;

            if (title_screen_player_hold) {
                __key_curr = KEY_A;
            }
            break;
    }

    player_main();
    draw_player();

    __key_curr = real_key_curr;
    __key_prev = real_key_prev;

    // Cap y position
    if (curr_player.player_y < TO_FIXED(0x728)) {
        curr_player.player_y = TO_FIXED(0x728);
        curr_player.player_y_speed = 0;
    }

    player_1 = curr_player;
    
    // Reset player if offscreen
    if (player_1.relative_player_x > 256) {
        reset_title_screen_player();
    }
}