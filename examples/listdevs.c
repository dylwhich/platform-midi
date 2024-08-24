#define PLATFORM_MIDI_IMPLEMENTATION
#include "platform_midi.h"
#include <stdio.h>
#include <stddef.h>

/*
 * listdevs.c
 *
 * Lists all known MIDI devices and their capabilities exits
 *
 */

int main(int argc, char** argv)
{
    struct platform_midi_driver *driver = NULL;

    if (argc > 1)
    {

    }

    if (!(driver = platform_midi_init("listdevs")))
    {
        printf("Initialization failed!\n");
        return 1;
    }

    platform_midi_print_devices(driver);

    platform_midi_deinit(driver);
    return 0;
}
