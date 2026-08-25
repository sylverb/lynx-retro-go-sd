/* Atari Lynx standalone core — Handy (handy-go) via gw_firmware_abi_t.
 * Include gw_core_bridge.h last so ACTIVE_FILE / ram_start / common_emu_state
 * rewrite to live ABI pointers. */
extern "C"
{
#include <odroid_system.h>
#include <string.h>
#include <stdio.h>

#include "gw_lcd.h"
#include "gw_malloc.h"
#include "common.h"
#include "rom_manager.h"
#include "odroid_overlay.h"
#include "appid.h"

#ifdef HOST_BUILD
#include "host_compat.h"
#else
#include "gw_core_bridge.h"
#endif
}

#include "heap.hpp"
#include <handy.h>
#define LYNX_FPS                60
#define AUDIO_LYNX_SAMPLE_RATE  HANDY_AUDIO_SAMPLE_FREQ /* 32000 */

static CSystem *lynx = NULL;
static uint16_t lynx_framebuffer[HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT];
static SWORD    lynx_audio_buffer[HANDY_AUDIO_BUFFER_LENGTH];

static void blit();

/* Save/Load through firmware fn-ptrs can see a null `lynx` on device; defer
 * the real ContextSave/Load into the main loop next to UpdateFrame. */
static volatile bool s_pending_save = false;
static volatile bool s_pending_load = false;
static char          s_pending_path[300];

static void queue_state_op(volatile bool *flag, const char *savePathName)
{
    strncpy(s_pending_path, savePathName, sizeof s_pending_path - 1);
    s_pending_path[sizeof s_pending_path - 1] = '\0';
    *flag = true;
}

static bool LoadState(const char *savePathName)
{
    queue_state_op(&s_pending_load, savePathName);
    return true;
}

static bool SaveState(const char *savePathName)
{
    queue_state_op(&s_pending_save, savePathName);
    return true;
}

static void process_pending_state_ops()
{
    if (s_pending_save)
    {
        s_pending_save = false;
        FILE *fp = fopen(s_pending_path, "wb");
        if (fp == NULL) { printf("[lynx save] fopen failed\n"); }
        else
        {
            bool ok = lynx->ContextSave(fp);
            fclose(fp);
            printf("[lynx save] done ok=%d\n", (int)ok);
        }
    }
    if (s_pending_load)
    {
        s_pending_load = false;
        FILE *fp = fopen(s_pending_path, "rb");
        if (fp == NULL) { lynx->Reset(); printf("[lynx load] fopen failed\n"); }
        else
        {
            bool ok = lynx->ContextLoad(fp);
            fclose(fp);
            if (!ok) lynx->Reset();
            printf("[lynx load] done ok=%d\n", (int)ok);
        }
    }
}

static void *Screenshot()
{
    lcd_wait_for_vblank();
    lcd_clear_active_buffer();
    blit();
    return lcd_get_active_buffer();
}

/* Headroom for bank1 (≤64K), CSystem/Mikey/Susie objects, and heap slack
 * after an optional full ROM copy into RAM_EMU. */
#define LYNX_RAM_COPY_HEADROOM (192 * 1024)

static size_t getromdata(unsigned char **data)
{
    uint32_t size = ACTIVE_FILE->size;

    /* Prefer a RAM copy when there is room: 65C02 cart peeks are much
     * faster from AXI SRAM than QSPI XIP, and this is the proven path for
     * ≤256K carts. */
    if (heap_get_largest_free_size() >= (size_t)size + LYNX_RAM_COPY_HEADROOM)
    {
        unsigned char *ram = new unsigned char[size];
        if (ram != NULL) {
            *data = ram;
            odroid_overlay_cache_file_in_ram(ACTIVE_FILE->path, ram);
            printf("[lynx] SD card->RAM\n");
            return size;
        }
        printf("[lynx] RAM allocation failed, falling back to flash\n");
    }

    *data = (unsigned char *)odroid_overlay_cache_file_in_flash(ACTIVE_FILE->path, &size, false);
    printf("[lynx] SD Card->Flash cache\n");
    return size;
}

static void blit()
{
    const uint16_t *src = lynx_framebuffer;
    uint16_t *out = (uint16_t *)lcd_get_active_buffer();
    const int y_offset = (GW_LCD_HEIGHT - HANDY_SCREEN_HEIGHT * 2) / 2; // 18

    for (int sy = 0; sy < HANDY_SCREEN_HEIGHT; sy++)
    {
        uint16_t *row0 = out + (y_offset + sy * 2) * GW_LCD_WIDTH;
        uint16_t *row1 = row0 + GW_LCD_WIDTH;
        const uint16_t *in = src + sy * HANDY_SCREEN_WIDTH;
        for (int sx = 0; sx < HANDY_SCREEN_WIDTH; sx++)
        {
            uint16_t c = in[sx];
            row0[sx * 2]     = c;
            row0[sx * 2 + 1] = c;
            row1[sx * 2]     = c;
            row1[sx * 2 + 1] = c;
        }
    }
}

static void sound_store()
{
    if (common_emu_sound_loop_is_muted())
    {
        gAudioBufferPointer = 0;
        return;
    }

    int32_t factor = common_emu_sound_get_volume();
    int16_t *out = audio_get_active_buffer();
    uint16_t len = audio_get_buffer_length();
    int gen = (int)(gAudioBufferPointer / 2);

    for (uint16_t i = 0; i < len; i++)
    {
        int idx = (gen > 0) ? ((i < (uint16_t)gen) ? i : gen - 1) : 0;
        int32_t s = (int32_t)lynx_audio_buffer[idx * 2] + (int32_t)lynx_audio_buffer[idx * 2 + 1];
        out[i] = (int16_t)((s * factor) >> 9);
    }

    gAudioBufferPointer = 0;
}

static void map_buttons(odroid_gamepad_state_t *joystick)
{
    ULONG buttons = 0;
    if (joystick->values[ODROID_INPUT_UP])     buttons |= BUTTON_UP;
    if (joystick->values[ODROID_INPUT_DOWN])   buttons |= BUTTON_DOWN;
    if (joystick->values[ODROID_INPUT_LEFT])   buttons |= BUTTON_LEFT;
    if (joystick->values[ODROID_INPUT_RIGHT])  buttons |= BUTTON_RIGHT;
    if (joystick->values[ODROID_INPUT_A])      buttons |= BUTTON_A;
    if (joystick->values[ODROID_INPUT_B])      buttons |= BUTTON_B;
    if (joystick->values[ODROID_INPUT_START])  buttons |= BUTTON_PAUSE;
    if (joystick->values[ODROID_INPUT_SELECT]) buttons |= BUTTON_OPT1;
    if (joystick->values[ODROID_INPUT_X])      buttons |= BUTTON_OPT2;
    lynx->SetButtonData(buttons);
}

extern "C" void app_main_lynx(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
{
    odroid_gamepad_state_t joystick;
    odroid_dialog_choice_t options[] = {ODROID_DIALOG_CHOICE_LAST};
    uint32_t rom_length = 0;
    uint8_t *rom_ptr = NULL;

    common_emu_state.pause_after_frames = start_paused ? 2 : 0;

    rom_length = getromdata(&rom_ptr);
    printf("[lynx] getromdata: rom_ptr=%p rom_length=%lu\n",
           (void *)rom_ptr, (unsigned long)rom_length);
    if (rom_ptr == NULL)
    {
        printf("Lynx: Failed to load ROM in flash/ram.\n");
        return;
    }

    printf("[lynx] new CSystem...\n");
    lynx = new CSystem((const UBYTE *)rom_ptr, (ULONG)rom_length,
                       MIKIE_PIXEL_FORMAT_16BPP_565, AUDIO_LYNX_SAMPLE_RATE);

    if (lynx == NULL || lynx->mFileType == HANDY_FILETYPE_ILLEGAL)
    {
        printf("Lynx: ROM loading failed.\n");
        return;
    }

    gPrimaryFrameBuffer = (UBYTE *)lynx_framebuffer;
    gAudioBuffer = lynx_audio_buffer;
    gAudioEnabled = 1;
    printf("[lynx] CSystem ok, fb=%p (build %s %s)\n",
           (void *)gPrimaryFrameBuffer, __DATE__, __TIME__);

    uint32_t samplesPerFrame = AUDIO_LYNX_SAMPLE_RATE / LYNX_FPS;

    common_emu_state.frame_time_10us = (uint16_t)(100000 / LYNX_FPS + 0.5f);
    lcd_set_refresh_rate(LYNX_FPS);

    odroid_system_init(APPID_CORE, AUDIO_LYNX_SAMPLE_RATE);
    odroid_system_emu_init(&LoadState, &SaveState, &Screenshot, NULL, NULL, NULL, NULL);

    bool pending_resume = load_state;
    if (!load_state)
        lcd_clear_buffers();

    audio_start_playing(samplesPerFrame);

    while (1)
    {
        wdog_refresh();
        common_emu_frame_loop();
        odroid_input_read_gamepad(&joystick);
        common_emu_input_loop(&joystick, options, &blit);
        common_emu_input_loop_handle_turbo(&joystick);

        map_buttons(&joystick);

        if (pending_resume)
        {
            pending_resume = false;
            odroid_system_emu_load_state(save_slot);
        }

        process_pending_state_ops();

        lynx->UpdateFrame(true);

        blit();
        common_ingame_overlay();
        lcd_swap();
        sound_store();

        common_emu_sound_sync(false);
    }
}
