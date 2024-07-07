#define PLATFORM_MIDI_IMPLEMENTATION
#include "platform_midi.h"
#include <stddef.h>
#include <stdio.h>

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
 * send.c
 *
 * Sends arbitrary MIDI messages in hex format
 *
 */

int main(int argc, char** argv)
{
    unsigned char data[1024];
    int datalen = 0;
    int totalread = 0;
    int read = 0;
    int result;
    int i;

    if (argc <= 1)
    {
        printf("Usage: %s \"<hex-data ...>\"\n", argv[0]);
        return 0;
    }

    while (1 == sscanf(&argv[1][totalread], "%2hhx%n", &data[datalen++], &read))
    {
        totalread += read;
        if (datalen >= sizeof(data))
        {
            printf("Reached end of buffer, input may be truncated at 1024 bytes\n");
            break;
        }
    }
    
    struct platform_midi_driver *driver = platform_midi_init("midisend");

    for (i = 0; i < 10; i++)
    {
        result = platform_midi_write(driver, data, datalen);

        if (result > 0)
        {
            printf("Sent data\n");
        }
        else
        {
            printf("Error sending: %d\n", result);
        }

        sleep(1);
    }

    platform_midi_deinit(driver);
    driver = NULL;
    return (result > 0);
}
