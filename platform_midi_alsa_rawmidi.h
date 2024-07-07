#ifndef _PLATFORM_MIDI_ALSA_RAWMIDI_H_
#define _PLATFORM_MIDI_ALSA_RAWMIDI_H_

#include <alsa/asoundlib.h>
#include <stdio.h>

struct platform_midi_driver *platform_midi_init_alsa_rawmidi(const char *name, void *data);
void platform_midi_deinit_alsa_rawmidi(struct platform_midi_driver *driver);
int platform_midi_read_alsa_rawmidi(struct platform_midi_driver *driver, unsigned char *out, int size);
int platform_midi_avail_alsa_rawmidi(struct platform_midi_driver *driver);
int platform_midi_write_alsa_rawmidi(struct platform_midi_driver *driver, const unsigned char *buf, int size);

#ifdef PLATFORM_MIDI_IMPLEMENTATION

struct platform_midi_alsa_rawmidi_driver
{
    platform_midi_deinit_fn deinitFn;
    platform_midi_avail_fn availFn;
    platform_midi_read_fn readFn;
    platform_midi_write_fn writeFn;
    void *data;

    snd_rawmidi_t *raw_in_port;
    snd_rawmidi_t *raw_out_port;
    int rawmidi_init;
};

struct platform_midi_driver *platform_midi_init_alsa_rawmidi(const char* name, void *data)
{
    void *alloc = malloc(sizeof(struct platform_midi_alsa_rawmidi_driver));

    if (!alloc)
    {
        printf("Failed to allocate driver struct\n");
        return NULL;
    }

    struct platform_midi_alsa_rawmidi_driver *rawmidi_driver = (struct platform_midi_alsa_rawmidi_driver*)alloc;

    int result = snd_rawmidi_open(&rawmidi_driver->raw_in_port, &rawmidi_driver->raw_out_port, "virtual", SND_RAWMIDI_NONBLOCK);
    if (0 != result)
    {
        const char *error_desc = snd_strerror(result);
        // Error!
        printf("Failed to initialize ALSA RawMIDI driver: %d (%s)\n", result, error_desc ? error_desc : "?");
        return 0;
    }

    printf("RawMIDI initialized\n");

    return (struct platform_midi_driver*)rawmidi_driver;
}

void platform_midi_deinit_alsa_rawmidi(struct platform_midi_driver *driver)
{
    struct platform_midi_alsa_rawmidi_driver *rawmidi_driver = (struct platform_midi_alsa_rawmidi_driver*)driver;
    snd_rawmidi_drain(rawmidi_driver->raw_in_port);
    snd_rawmidi_close(rawmidi_driver->raw_in_port);
    snd_rawmidi_drain(rawmidi_driver->raw_out_port);
    snd_rawmidi_close(rawmidi_driver->raw_out_port);

    free(rawmidi_driver);
}

int platform_midi_read_alsa_rawmidi(struct platform_midi_driver *driver, unsigned char *out, int size)
{
    struct platform_midi_alsa_rawmidi_driver *rawmidi_driver = (struct platform_midi_alsa_rawmidi_driver*)driver;
    ssize_t result = snd_rawmidi_read(rawmidi_driver->raw_in_port, (void*)out, (size_t)size);
    if (result < 1)
    {
        printf("Error reading data\n");
        return -1;
    }

    return (int)result;
}

int platform_midi_avail_alsa_rawmidi(struct platform_midi_driver *driver)
{
    struct platform_midi_alsa_rawmidi_driver *rawmidi_driver = (struct platform_midi_alsa_rawmidi_driver*)driver;
    char status_data[snd_rawmidi_status_sizeof()];
    snd_rawmidi_status_t *status = (snd_rawmidi_status_t*)status_data;
    if (0 == snd_rawmidi_status(rawmidi_driver->raw_in_port, status))
    {
        return snd_rawmidi_status_get_avail(status);
    }

    return -1;
}

int platform_midi_write_alsa_rawmidi(struct platform_midi_driver *driver, const unsigned char* buf, int size)
{
    struct platform_midi_alsa_rawmidi_driver *rawmidi_driver = (struct platform_midi_alsa_rawmidi_driver*)driver;
    ssize_t result = snd_rawmidi_write(rawmidi_driver->raw_out_port, (const void*)buf, (size_t)size);

    if (result < 0)
    {
        printf("Error sending data\n");
        return -1;
    }

    return (int)result;
}
#endif

#endif
