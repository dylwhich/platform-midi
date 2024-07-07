#ifndef _PLATFORM_MIDI_WINDOWS_H_
#define _PLATFORM_MIDI_WINDOWS_H_

struct platform_midi_driver *platform_midi_init_winmm(const char *name, void *data);
void platform_midi_deinit_winmm(struct platform_midi_driver *driver);
int platform_midi_read_winmm(struct platform_midi_driver *driver, unsigned char *out, int size);
int platform_midi_avail_winmm(struct platform_midi_driver *driver);
int platform_midi_write_winmm(struct platform_midi_driver *driver, const unsigned char *buf, int size);

#ifdef PLATFORM_MIDI_IMPLEMENTATION

#include <windows.h>
#include <WinDef.h>
#include <IntSafe.h>
#include <BaseTsd.h>
#include <mmeapi.h>

struct platform_midi_winmm_driver
{
    platform_midi_deinit_fn deinitFn;
    platform_midi_avail_fn availFn;
    platform_midi_read_fn readFn;
    platform_midi_write_fn writeFn;
    void *data;

    struct platform_midi_ringbuf buffer;

    // Anywhere that accepts LPHMIDIIN just wants a pointer to this
    HMIDIIN phmi;
}

void CALLBACK platform_midi_winmm_callback(HMIDIIN midiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    struct platform_midi_winmm_driver *driver = (struct platform_midi_winmm_driver*)dwInstance;

    // All the MIDI data is in dwParam1
    // TODO: Should dwParam1 be dereferenced?
    unsigned char status = dwParam1 & 0xFF;
    unsigned char dataByte1 = (dwParam1 & 0xFF00) >> 8;
    unsigned char dataByte2 = (dwParam1 & 0xFF0000) >> 16;

    unsigned int packetLen = 1;

    // This API expands all running status bytes, so don't worry about that
    switch ((status & 0xF0) >> 4)
    {
        case 0xF:
        {
            if (status == 0xF1 || status == 0xF3)
            {
                packetLen = 2;
            }
            else if (status == 0xF2)
            {
                packetLen = 3;
            }
            else
            {
                packetLen = 1;
            }
            break;
        }

        case 0xC:
        case 0xD:
        {
            packetLen = 2;
            break;
        }

        default:
        {
            packetLen = 3;
            break;
        }
    }

    unsigned char out[3];
    out[0] = status;
    if (packetLen > 1)
    {
        out[1] = dataByte1;

        if (packetLen > 2)
        {
            out[2] = dataByte2;
        }
    }

    platform_midi_push_packet(&driver->buffer, out, packetLen);
}


struct platform_midi_driver *platform_midi_init_winmm(const char* name, void *data)
{
    void *alloc = malloc(sizeof(struct platform_midi_winmm_driver));

    if (!alloc)
    {
        printf("Failed to allocate driver struct\n");
        return NULL;
    }

    struct platform_midi_winmm_driver *winmm_driver = (struct platform_midi_winmm_driver*)alloc;
    winmm_driver->deinitFn = platform_midi_deinit_winmm;
    winmm_driver->availFn = platform_midi_avail_winmm;
    winmm_driver->readFn = platform_midi_read_winmm;
    winmm_driver->writeFn = platform_midi_write_winmm;
    winmm_driver->data = data;

    // Pass pointer to phmi, as this function sets it to a new handle
    MMRESULT result = midiInOpen(&winmm_driver->phmi, 0, (DWORD_PTR)platform_midi_winmm_callback, (DWORD_PTR)driver, CALLBACK_FUNCTION);
    printf("midiInOpen() == %d\n", result);
    printf("midiInStart() == %d\n", midiInStart(winmm_driver->phmi));

    return (struct platform_midi_driver*)winmm_driver;
}

void platform_midi_deinit_winmm(struct platform_midi_driver *driver)
{
    struct platform_midi_winmm_driver *winmm_driver = (struct platform_midi_winmm_driver*)driver;
    printf("midiInStop() == %d\n", midiInStop(winmm_driver->phmi));
    MMRESULT result = midiInClose(driver->phmi);
    printf("platform_midi_deinit_winmm() result: %d\n", result);
    free(winmm_driver);
}

int platform_midi_read_winmm(struct platform_midi_driver *driver, unsigned char * out, int size)
{
    struct platform_midi_winmm_driver *winmm_driver = (struct platform_midi_winmm_driver*)driver;
    return platform_midi_pop_packet(&winmm_driver->buffer, out, size);
}

int platform_midi_avail_winmm(struct platform_midi_driver *driver)
{
    struct platform_midi_winmm_driver *winmm_driver = (struct platform_midi_winmm_driver*)driver;
    return platform_midi_packet_count(&winmm_driver->buffer);
}

int platform_midi_write_winmm(struct platform_midi_driver *driver, const unsigned char* buf, int size)
{
    struct platform_midi_winmm_driver *winmm_driver = (struct platform_midi_winmm_driver*)driver;
    printf("platform_midi_write_winmm() not implemented\n");
    return 0;
}

#endif

#endif
