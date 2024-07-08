#ifndef _PLATFORM_MIDI_H_
#define _PLATFORM_MIDI_H_

struct platform_midi_driver;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platform_midi_driver* (*platform_midi_init_fn)(const char *, void*);
typedef void  (*platform_midi_deinit_fn)(struct platform_midi_driver*);
typedef int   (*platform_midi_read_fn)(struct platform_midi_driver*, unsigned char*, int);
typedef int   (*platform_midi_write_fn)(struct platform_midi_driver*, const unsigned char*, int);
typedef int   (*platform_midi_avail_fn)(struct platform_midi_driver*);

struct platform_midi_driver* platform_midi_init(const char *name);
void platform_midi_deinit(struct platform_midi_driver *driver);
int platform_midi_read(struct platform_midi_driver *driver, unsigned char *out, int size);
int platform_midi_avail(struct platform_midi_driver *driver);
int platform_midi_write(struct platform_midi_driver *driver, const unsigned char *buf, int size);

#if defined(__linux) || defined(__linux__) || defined(linux) || defined(__LINUX__)
#define PLATFORM_MIDI_ALSA_RAWMIDI 1
#define PLATFORM_MIDI_ALSA 1
#elif defined(__APPLE__)
#define PLATFORM_MIDI_COREMIDI 1
#else
#define PLATFORM_MIDI_WINMM 1
#endif

#ifndef PLATFORM_MIDI_EVENT_BUFFER_ITEMS
#define PLATFORM_MIDI_EVENT_BUFFER_ITEMS 32
#endif

#ifndef PLATFORM_MIDI_EVENT_BUFFER_SIZE
#define PLATFORM_MIDI_EVENT_BUFFER_SIZE 1024
#endif

#ifdef PLATFORM_MIDI_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct platform_midi_packet_info
{
    unsigned int offset;
    unsigned int length;
};

struct platform_midi_ringbuf
{
    unsigned char buffer[PLATFORM_MIDI_EVENT_BUFFER_SIZE];
    struct platform_midi_packet_info packets[PLATFORM_MIDI_EVENT_BUFFER_ITEMS];
    unsigned int read_pos;
    unsigned int write_pos;
    unsigned int buffer_offset;
    unsigned int buffer_end;
};

#define platform_midi_buffer_empty(buf) (buf->read_pos == buf->write_pos)

static void platform_midi_push_packet(struct platform_midi_ringbuf *buf, unsigned char *data, unsigned int length)
{
    if ((buf->write_pos + 1) % PLATFORM_MIDI_EVENT_BUFFER_ITEMS == buf->read_pos)
    {
        printf("Warn: MIDI packet buffer is full, dropping an event\n");
        // The buffer is full... gotta drop a packet
        buf->buffer_offset = (buf->buffer_offset + buf->packets[buf->read_pos].length) % PLATFORM_MIDI_EVENT_BUFFER_SIZE;
        buf->read_pos = (buf->read_pos + 1) % PLATFORM_MIDI_EVENT_BUFFER_ITEMS;
    }

    buf->packets[buf->write_pos].offset = buf->buffer_end;
    buf->packets[buf->write_pos].length = length;

    if (buf->buffer_end + length <= PLATFORM_MIDI_EVENT_BUFFER_SIZE)
    {
        memcpy(&buf->buffer[buf->buffer_end], data, length);
    }
    else
    {
        unsigned int startLen = PLATFORM_MIDI_EVENT_BUFFER_SIZE - buf->buffer_end;
        memcpy(&buf->buffer[buf->buffer_end], data, startLen);
        memcpy(buf->buffer, data + startLen, length - startLen);
    }

    buf->buffer_end = (buf->buffer_end + length) % PLATFORM_MIDI_EVENT_BUFFER_SIZE;
    buf->write_pos = (buf->write_pos + 1) % PLATFORM_MIDI_EVENT_BUFFER_ITEMS;
}

static int platform_midi_convert_from_ump(unsigned char *out, unsigned int maxlen, const unsigned int *umpWords, unsigned int wordCount)
{
    unsigned int written = 0;
    unsigned char type  = (umpWords[0] & 0xF0000000) >> 28;
    //unsigned char group = (umpWords[0] & 0x0F000000) >> 24;

    switch (type)
    {
        case 0x01:
        {
            // System real-time / system common
            unsigned char status = (umpWords[0] & 0x00FF0000) >> 20;
            unsigned int packetLen = 1;
            switch (status)
            {
                case 0xF1:
                case 0xF3:
                {
                    packetLen = 2;
                    break;
                }

                case 0xF2:
                {
                    packetLen = 3;
                    break;
                }

                default:
                break;
            }

            if (written + packetLen > maxlen)
            {
                return written;
            }
            out[written++] = status;

            if (packetLen > 1)
            {
                out[written++] = (umpWords[0] & 0x0000FF00) >> 8;

                if (packetLen > 2)
                {
                    out[written++] = (umpWords[0] & 0x000000FF);
                }
            }
            break;
        }

        case 0x02:
        {
            // Channel voice
            unsigned char status = (umpWords[0] & 0x00F00000) >> 20;
            unsigned int packetLen = 3;
            switch (status)
            {
                case 0xC:
                case 0xD:
                {
                    packetLen = 2;
                    break;
                }

                default:
                break;
            }

            if (written + packetLen > maxlen)
            {
                return written;
            }

            out[written++] = (umpWords[0] & 0x00FF0000) >> 16;
            out[written++] = (umpWords[0] & 0x0000FF00) >> 8;

            if (packetLen > 2)
            {
                out[written++] = umpWords[0] & 0x000000FF;
            }
            break;
        }

        case 0x03:
        {
            // Data (SysEx)
            // TODO
            break;
        }
    }

    return written;
}

static int platform_midi_convert_to_ump(unsigned int *out, unsigned int maxWords, const unsigned char *data, unsigned int dataLen)
{
    unsigned int *ump = out;
    unsigned int read = 0;

    while (ump < out + maxWords && read < dataLen)
    {
        // Always 1
        unsigned char group = 0;
        // default 2: Channel Voice
        unsigned char type = 2;
        if (data[read] == 0xF7)
        {
            type = 3;
        }
        else if ((data[read] & 0xF0) == 0xF0)
        {
            type = 1;
        }

        *ump = ((type & 0xF) << 28) | ((group & 0xF) << 24);
        int bits = 16;

        while (read < dataLen)
        {
            *ump |= data[read++] << bits;

            bits -= 8;
            if (bits == 0)
            {
                ump++;
            }
        }
    }

    return ump - out;
}

static int platform_midi_packet_count(struct platform_midi_ringbuf *buf)
{
    if (buf->read_pos == buf->write_pos)
    {
        return 0;
    }
    else if (buf->read_pos < buf->write_pos)
    {
        return buf->write_pos - buf->read_pos;
    }
    else
    {
        return PLATFORM_MIDI_EVENT_BUFFER_ITEMS - buf->read_pos - buf->write_pos;
    }
}

static int platform_midi_pop_packet(struct platform_midi_ringbuf *buf, unsigned char *out, unsigned int size)
{
    if (buf->read_pos == buf->write_pos)
    {
        return 0;
    }

    struct platform_midi_packet_info *packet = &buf->packets[buf->read_pos];
    int toCopy = (size < packet->length) ? size : packet->length;

    // TODO detect large events overrunning the buffer

    if (packet->offset + packet->length > PLATFORM_MIDI_EVENT_BUFFER_SIZE)
    {
        // event is split across the end and beginning of the buffer
        int endCopy = (packet->offset + toCopy <= PLATFORM_MIDI_EVENT_BUFFER_SIZE) ? toCopy : PLATFORM_MIDI_EVENT_BUFFER_SIZE - packet->offset;
        int startCopy = packet->length - endCopy;

        memcpy(out, &buf->buffer[packet->offset], endCopy);
        memcpy(out + endCopy, buf->buffer, startCopy);
    }
    else
    {
        memcpy(out, &buf->buffer[packet->offset], toCopy);
    }

    buf->buffer_offset = (buf->buffer_offset + packet->length) % PLATFORM_MIDI_EVENT_BUFFER_SIZE;
    buf->read_pos = (buf->read_pos + 1) % PLATFORM_MIDI_EVENT_BUFFER_ITEMS;

    return toCopy;
}

static int platform_midi_buffer_init(struct platform_midi_ringbuf *buf)
{
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->buffer_offset = 0;
    buf->buffer_end = 0;
    memset(buf->buffer, 0, PLATFORM_MIDI_EVENT_BUFFER_SIZE);

    return 1;
}

static void platform_midi_buffer_deinit(struct platform_midi_ringbuf *buf)
{
    platform_midi_buffer_init(buf);
}

struct platform_midi_driver
{
    platform_midi_deinit_fn deinitFn;
    platform_midi_avail_fn availFn;
    platform_midi_read_fn readFn;
    platform_midi_write_fn writeFn;
    void *data;
};
#endif

//#define PLATFORM_MIDI_DRIVER_NULL { 1, "NULL", platform_midi_init_null }
//#include "platform_midi_null.h"
#define PLATFORM_MIDI_DRIVER_NULL { 0, 0, 0 }

#ifdef PLATFORM_MIDI_ALSA
#define PLATFORM_MIDI_DRIVER_ALSA { 2, "ALSA-Sequencer", platform_midi_init_alsa }
#include "platform_midi_alsa.h"
#else
#define PLATFORM_MIDI_DRIVER_ALSA { 0, 0, 0 }
#endif

#ifdef PLATFORM_MIDI_ALSA_RAWMIDI
#define PLATFORM_MIDI_DRIVER_ALSA_RAWMIDI { 1, "ALSA-RawMIDI", platform_midi_init_alsa_rawmidi }
#include "platform_midi_alsa_rawmidi.h"
#else
#define PLATFORM_MIDI_DRIVER_ALSA_RAWMIDI { 0, 0, 0 }
#endif

#ifdef PLATFORM_MIDI_COREMIDI
#define PLATFORM_MIDI_DRIVER_COREMIDI { 2, "CoreMIDI", platform_midi_init_coremidi }
#include "platform_midi_coremidi.h"
#else
#define PLATFORM_MIDI_DRIVER_COREMIDI { 0, 0, 0 }
#endif

#ifdef PLATFORM_MIDI_WINMM
#define PLATFORM_MIDI_DRIVER_WINMM { 2, "WinMM", platform_midi_init_winmm }
#include "platform_midi_winmm.h"
#else
#define PLATFORM_MIDI_DRIVER_WINMM { 0, 0, 0 }
#endif

#ifdef PLATFORM_MIDI_JACK
#define PLATFORM_MIDI_DRIVER_JACK { 3, "JACK", platform_midi_init_jack }
#include "platform_midi_jack.h"
#else
#define PLATFORM_MIDI_DRIVER_JACK { 0, 0, 0 }
#endif

struct platform_midi_driver_def
{
    int priority;
    const char *name;
    platform_midi_init_fn initFn;
};

static const struct platform_midi_driver_def PLATFORM_MIDI_DRIVERS[] = {
    PLATFORM_MIDI_DRIVER_NULL,
    PLATFORM_MIDI_DRIVER_ALSA,
    PLATFORM_MIDI_DRIVER_ALSA_RAWMIDI,
    PLATFORM_MIDI_DRIVER_COREMIDI,
    PLATFORM_MIDI_DRIVER_WINMM,
    PLATFORM_MIDI_DRIVER_JACK,
};

struct platform_midi_driver* platform_midi_init(const char* name)
{
    const struct platform_midi_driver_def *driver = &PLATFORM_MIDI_DRIVERS[0];

    for (int i = 0; i < sizeof(PLATFORM_MIDI_DRIVERS) / sizeof(PLATFORM_MIDI_DRIVERS[0]); i++)
    {
        if (PLATFORM_MIDI_DRIVERS[i].priority > driver->priority && PLATFORM_MIDI_DRIVERS[i].initFn)
        {
            driver = &PLATFORM_MIDI_DRIVERS[i];
        }
    }

    if (!driver->initFn)
    {
        printf("ERR: No suitable MIDI backends found.\n");
        return NULL;
    }

    return driver->initFn(name, NULL);
}

void platform_midi_deinit(struct platform_midi_driver* driver)
{
    if (driver && driver->deinitFn)
    {
        driver->deinitFn(driver);
    }
}

int platform_midi_read(struct platform_midi_driver* driver, unsigned char * out, int size)
{
    return driver->readFn(driver, out, size);
}

int platform_midi_avail(struct platform_midi_driver* driver)
{
    return driver->availFn(driver);
}

int platform_midi_write(struct platform_midi_driver* driver, const unsigned char* buf, int size)
{
    return driver->writeFn(driver, buf, size);
}

#ifdef __cplusplus
};
#endif

#endif
