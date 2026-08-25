/*
 * Desktop entry: init SDL, then run the Lynx core (app_main_lynx).
 */

#include <cstdio>
#include <cstdlib>

extern "C" {
#include "host_compat.h"
#include "host_platform.h"
}

#ifndef HOST_SCALE
#define HOST_SCALE 2
#endif

extern "C" void app_main_lynx(uint8_t load_state, uint8_t start_paused, int8_t save_slot);

int main(int argc, char **argv)
{
    const char *title = "Atari Lynx (host)";
    const char *rom = getenv("HOST_ROM");

    if (argc > 1 && argv[1] && argv[1][0])
        rom = argv[1];

    if (host_platform_init(title, HOST_SCALE) != 0)
        return 1;

    gw_core_bridge_init();
    if (rom)
        host_set_rom_path(rom);

    std::printf("host: Esc or close window to quit\n");
    std::printf("host: Arrows=D-pad  Z=B  X=A  Enter=Start  Shift=Select  A/S=Y/X\n");
    std::printf("host: F1=save state  F2=load state  (./host_saves/)\n");
    if (rom)
        std::printf("host: ROM %s\n", rom);
    else
        std::printf("host: no ROM — pass path as argv[1] or set HOST_ROM\n");

    app_main_lynx(0, 0, -1);

    host_platform_shutdown();
    return 0;
}
