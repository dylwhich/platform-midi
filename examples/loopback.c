#define PLATFORM_MIDI_IMPLEMENTATION
#include "platform_midi.h"
#include <stdio.h>
#include <stddef.h>

#if defined(WINDOWS) || defined(__WINDOWS__) || defined(_WINDOWS) \
                     || defined(_WIN32)      || defined(_WIN64) \
                     || defined(WIN32)       || defined(WIN64) \
                     || defined(__WIN32__)   || defined(__CYGWIN__) \
                     || defined(__MINGW32__) || defined(__MINGW64__) \
                     || defined(__TOS_WIN__)
#include <windows.h>
#define sleep(n) Sleep((n) * 1000)
#else
#include <unistd.h>
#endif

/*
 * loopback.c
 *
 * Sends a MIDI message to itself and attempts to retrieve it
 *
 */

int main(int argc, char** argv)
{
    unsigned char packet[16];
    int read = 0;
    int limit = 0;
    int packets = 0;

    struct platform_midi_driver *driver = platform_midi_init("mididump");

    if (!driver)
    {
        printf("ERROR! Could not initialize platform_midi\n");
        return 1;
    }

    for (packets = 0; packets < 10; packets++)
    {
        packet[0] = (packets % 2) ? 0x80 : 0x90;
        packet[1] = 0x32;
        packet[2] = 0x7F;
        platform_midi_write(driver, packet, 3);
        printf(">> Wrote packet of length %d\n", 3);

        sleep(1);

        read = platform_midi_read(driver, packet, sizeof(packet));

        if (read > 0)
        {
            printf("<< Received packet of length %d\n", read);
        }
        else
        {
            printf("-- No packet received\n");
        }
    }

    platform_midi_deinit(driver);
    return 0;
}
