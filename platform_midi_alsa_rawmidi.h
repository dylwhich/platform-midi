#ifndef _PLATFORM_MIDI_ALSA_RAWMIDI_H_
#define _PLATFORM_MIDI_ALSA_RAWMIDI_H_

#include <alsa/asoundlib.h>
#include <stdio.h>

int platform_midi_init_alsa_rawmidi(const char* name);
void platform_midi_deinit_alsa_rawmidi(void);
int platform_midi_read_alsa_rawmidi(unsigned char * out, int size);
int platform_midi_avail_alsa_rawmidi(void);
int platform_midi_write_alsa_rawmidi(const unsigned char* buf, int size);

#define PLATFORM_MIDI_INIT(name) platform_midi_init_alsa_rawmidi(name)
#define PLATFORM_MIDI_DEINIT() platform_midi_deinit_alsa_rawmidi()
#define PLATFORM_MIDI_READ(out, size) platform_midi_read_alsa_rawmidi(out, size)
#define PLATFORM_MIDI_AVAIL() platform_midi_avail_alsa_rawmidi()
#define PLATFORM_MIDI_WRITE(buf, size) platform_midi_write_alsa_rawmidi(buf, size)

#ifdef PLATFORM_MIDI_IMPLEMENTATION

snd_rawmidi_t *raw_in_port = 0;
snd_rawmidi_t *raw_out_port = 0;
int rawmidi_init = 0;

int platform_midi_init_alsa_rawmidi(const char* name)
{
    int result = snd_rawmidi_open(&raw_in_port, &raw_out_port, "virtual", SND_RAWMIDI_NONBLOCK);
    if (0 != result)
    {
	const char *error_desc = snd_strerror(result);
        // Error!
        printf("Failed to initialize ALSA RawMIDI driver: %d (%s)\n", result, error_desc ? error_desc : "?");
        return 0;
    }

    printf("RawMIDI initialized\n");

    rawmidi_init = 1;
    return 1;
}

void platform_midi_deinit_alsa_rawmidi(void)
{
    snd_rawmidi_drain(raw_in_port);
    snd_rawmidi_close(raw_in_port);
    snd_rawmidi_drain(raw_out_port);
    snd_rawmidi_close(raw_out_port);

    raw_in_port = 0;
    raw_out_port = 0;
    
    rawmidi_init = 0;
}

int platform_midi_read_alsa_rawmidi(unsigned char * out, int size)
{
    ssize_t result = snd_rawmidi_read(raw_in_port, (void*)out, (size_t)size);
    if (result < 1)
    {
        printf("Error reading data\n");
        return -1;
    }

    return (int)result;
}

int platform_midi_avail_alsa_rawmidi(void)
{
    char status_data[snd_rawmidi_status_sizeof()];
    snd_rawmidi_status_t *status = (snd_rawmidi_status_t*)status_data;
    if (0 == snd_rawmidi_status(raw_in_port, status))
    {
        return snd_rawmidi_status_get_avail(status);
    }

    return -1;
}

int platform_midi_write_alsa_rawmidi(const unsigned char* buf, int size)
{
    ssize_t result = snd_rawmidi_write(raw_out_port, (const void*)buf, (size_t)size);
    
    if (result < 0)
    {
        printf("Error sending data\n");
        return -1;
    }

    return (int)result;
}
#endif

#endif
