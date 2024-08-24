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

    struct platform_midi_client client;
    struct platform_midi_port port;

    // Use -1 to specify the first client
    client.id = -1;

    // Loop over all the clients
    while (0 == platform_midi_next_client(driver, &client))
    {
        printf("%s: %d port%s\n", client.name, client.port_count, (client.port_count == 1) ? "" : "s");

        // Use -1 to specify the first port on this client
        port.id = -1;
        while (0 == platform_midi_next_port(driver, client.id, &port))
        {
            char cap_str[4] = {'-', '-', '-', '\0'};
            if (port.caps & PLATFORM_MIDI_PORT_SOURCE)
            {
                cap_str[0] = 'R';
            }

            if (port.caps & PLATFORM_MIDI_PORT_DEST)
            {
                cap_str[1] = 'W';
            }

            if (port.caps & PLATFORM_MIDI_PORT_THRU)
            {
                cap_str[2] = 'T';
            }

            printf(" - %d:%d [%s] '%s'\n", client.id, port.id, cap_str, port.name);
        }
        printf("\n");
    }

    platform_midi_deinit(driver);
    return 0;
}
