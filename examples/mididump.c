#define PLATFORM_MIDI_IMPLEMENTATION
#include "platform_midi.h"
#include <stdio.h>
#include <stddef.h>

/*
 * mididump.c
 *
 * Listens for and prints out MIDI messages
 *
 */

unsigned char get_running_status(unsigned char status, unsigned char *running_status)
{
    if (0 != (status & 0xF0))
    {
        if (status > 0xF7)
        {
            // Realtime message, does not affect running status
            return status;
        }
        else if (0xF0 <= status && status <= 0xF7)
        {
            // System message, resets running status
            *running_status = 0;
            return status;
        }
        else
        {
            // (0x80 <= status && status <= 0xEF)
            // Channel voice message, sets running status
            *running_status = status;
            return status;
        }
    }
    else
    {
        // status <= 0x7F (data value byte)
        if (0 == *running_status)
        {
            // Running status wasn't set! This is an error
            return 0;
        }

        // use the previous status
        return *running_status;
    }
}

static const char *note_names[] = {
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B",
};

void write_note_name(char *out, unsigned char note)
{
    const char *note_name = note_names[note % 12];
    int octave = (note / 12) - 2;

    sprintf(out, "%2s%d%s", note_name, octave, ((octave < 0) ? "" : " "));
}

void print_midi_packet(unsigned char *packet, unsigned int size)
{
    static unsigned char running_status = 0;
    unsigned char status;
    unsigned char *data;
    char note_name[16];
    const char *status_name;

    if (!size)
    {
        printf("<Empty Packet>\n");
        return;
    }

    // We may need to handle running status!
    status = get_running_status(packet[0], &running_status);
    if (!status)
    {
        // Error
        printf("Invalid status byte: %02hhx\n", packet[0]);
        return;
    }
    if (status != packet[0])
    {
        // Running status was used, so the first byte is data
        data = packet;
    }
    else
    {
        // Running status was not used, so the second byte is data
        data = packet + 1;
    }

#define RSC ((status == packet[0]) ? ' ' : '>')
#define CH (status & 0x0F)
    switch (status & 0xF0)
    {
        case 0x80: // Note Off
        case 0x90: // Note On
        case 0xA0: // After-Touch
        {
            write_note_name(note_name, data[0]);

            if (0x80 == (status & 0xF0))
            {
                status_name = "Note OFF";
            }
            else if (0x90 == (status & 0xF0))
            {
                status_name = "Note ON ";
            }
            else if( 0xA0 == (status  &0xF0))
            {
                status_name = "AfterTouch";
            }

            printf("%c%02hhx %02hhx %02hhx  %s Chan %2hhd  Note %s  Velocity %03u\n", RSC, status, data[0], data[1], status_name, (status & 0x0F) + 1, note_name, data[1]);
            break;
        }

        case 0xB0: // Controller
        {
            printf("%c%02hhx %02hhx %02hhx  Controller Chan %2hhd  %hhu = %hhu\n", RSC, status, data[0], data[1], (status & 0x0F) + 1, data[0], data[1]);
            break;
        }

        case 0xC0: // Program Select
        {
            printf("%c%02hhx %02hhx     Program Select Chan %2hhd  Program %hhu\n", RSC, status, data[0], (status & 0x0F) + 1);
            break;
        }

        case 0xD0: // Channel Pressure
        {
            printf("%c%02hhx %02hhx     Channel Pressure Chan %2hhd  Pressure %03hhu\n", RSC, status, data[0], (status & 0x0F) + 1, data[0]);
            break;
        }

        case 0xE0: // Pitch wheel
        {
            printf("%c%02hhx %02hhx %02hhx  Pitch Wheel Chan %2hhd  Pitch %+03dc\n", RSC, status, data[0], data[1], (status & 0x0F) + 1, (-0x2000 + (((data[1] & 0x7F) << 7) | (data[0] & 0x7F))) * 100 / 0x1FFF);
            break;
        }

        case 0xF0: // System
        {
            switch (status & 0x0F)
            {
                case 0x0: // SysEx Start
                {
                    printf(" %02hhx        SysEx Start\n", status);
                    break;
                }

                case 0x1: // MTC Quarter Frame
                {
                    printf(" %02hhx %02hhx     Quarter Frame %03hhu\n", status, data[0], data[0]);
                    break;
                }

                case 0x2: // Song Position Pointer
                {
                    printf(" %02hhx %02hhx %02hhx  Song Position Pointer Beat %03d\n", status, data[0], data[1], ((data[1] & 0x7F) << 7) | (data[0] & 0x7F));
                    break;
                }

                case 0x3: // Song Select
                {
                    printf(" %02hhx %02hhx     Song Select Song %03u\n", status, data[0], data[0]);
                    break;
                }

                case 0x4: // RESERVED
                {
                    printf(" %02hhx        Reserved\n", status);
                    break;
                }

                case 0x5: // RESERVED
                {
                    printf(" %02hhx        Reserved\n", status);
                    break;
                }

                case 0x6: // Tune Request
                {
                    printf(" %02hhx        Tune Request\n", status);
                    break;
                }

                case 0x7: // SysEx End
                {
                    printf(" %02hhx        SysEx End\n", status);
                    break;
                }

                case 0x8: // Clock
                {
                    printf(" %02hhx        Clock\n", status);
                    break;
                }

                case 0x9: // Tick
                {
                    printf(" %02hhx        Tick\n", status);
                    break;
                }

                case 0xA: // Start
                {
                    printf(" %02hhx        Start\n", status);
                    break;
                }

                case 0xB: // Continue
                {
                    printf(" %02hhx        Continue\n", status);
                    break;
                }

                case 0xC: // Stop
                {
                    printf(" %02hhx        Stop\n", status);
                    break;
                }

                case 0xD: // RESERVED
                {
                    printf(" %02hhx        Reserved\n", status);
                    break;
                }

                case 0xE: // Active Sense
                {
                    printf(" %02hhx        Active Sense\n", status);
                    break;
                }

                case 0xF: // Reset
                {
                    printf(" %02hhx        Reset\n", status);
                    break;
                }
            }
            break;
        }
    }
}

int main()
{
    unsigned char packet[16];
    int read = 0;
    PLATFORM_MIDI_INIT("mididump");

    while (1)
    {
        while (PLATFORM_MIDI_AVAIL())
        {
            read = PLATFORM_MIDI_READ(packet, sizeof(packet));
            if (read > 0)
            {
                print_midi_packet(packet, read);
            }
        else if (read < 0)
        {
        printf("Error reading: %d\n", read);
        break;
        }
        }
    }

    PLATFORM_MIDI_DEINIT();
    return 0;
}
