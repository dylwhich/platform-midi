#ifndef _PLATFORM_MIDI_ALSA_H_
#define _PLATFORM_MIDI_ALSA_H_

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

struct platform_midi_driver *platform_midi_init_alsa(const char *name, void *data);
void platform_midi_deinit_alsa(struct platform_midi_driver *driver);
int platform_midi_read_alsa(struct platform_midi_driver *driver, unsigned char *out, int size);
int platform_midi_avail_alsa(struct platform_midi_driver *driver);
int platform_midi_write_alsa(struct platform_midi_driver *driver, const unsigned char *buf, int size);

#ifdef PLATFORM_MIDI_IMPLEMENTATION

struct platform_midi_alsa_driver
{
    platform_midi_deinit_fn deinitFn;
    platform_midi_avail_fn availFn;
    platform_midi_read_fn readFn;
    platform_midi_write_fn writeFn;
    void *data;

    snd_seq_t *seq_handle;
    snd_midi_event_t *event_parser;
    int in_port;
    int out_port;
    unsigned char* sysexBuf;
    int sysexBuflen;
    int sysexWriteOff;
    int sysexReadOff;
    int sysexPending;
};

struct platform_midi_driver *platform_midi_init_alsa(const char* name, void *data)
{
    snd_seq_t *seq_handle;
    snd_midi_event_t *event_parser;
    int in_port = 0;
    int out_port = 0;

    if (0 != snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK))
    {
        // Error!
        printf("Failed to initialize ALSA driver\n");
        return 0;
    }

    printf("Sequncer initialized\n");

    if (0 != snd_seq_set_client_name(seq_handle, name))
    {
        printf("Failed to set client name\n");
    }

    printf("Client name set to %s\n", name);

    in_port = snd_seq_create_simple_port(seq_handle, "listen:in",
                      SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                      SND_SEQ_PORT_TYPE_APPLICATION);

    out_port = snd_seq_create_simple_port(seq_handle, "output",
                                          SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_WRITE,
                                          SND_SEQ_PORT_TYPE_APPLICATION|SND_SEQ_PORT_TYPE_PORT|SND_SEQ_PORT_TYPE_SOFTWARE);

    if (0 != snd_midi_event_new(64, &event_parser))
    {
        printf("Failed to create MIDI parser\n");
    }

    void* alloc = malloc(sizeof(struct platform_midi_alsa_driver));

    if (!alloc)
    {
        printf("Failed to allocate driver struct\n");
        return 0;
    }

    struct platform_midi_alsa_driver *alsa_driver = (struct platform_midi_alsa_driver*)alloc;
    alsa_driver->deinitFn = platform_midi_deinit_alsa;
    alsa_driver->availFn = platform_midi_avail_alsa;
    alsa_driver->readFn = platform_midi_read_alsa;
    alsa_driver->writeFn = platform_midi_write_alsa;
    alsa_driver->data = data;

    alsa_driver->seq_handle = seq_handle;
    alsa_driver->event_parser = event_parser;
    alsa_driver->in_port = in_port;
    alsa_driver->out_port = out_port;
    alsa_driver->sysexBuf = NULL;
    alsa_driver->sysexBuflen = 0;
    alsa_driver->sysexWriteOff = 0;
    alsa_driver->sysexPending = 0;

    printf("Done initializing MIDI!\n");
    return (struct platform_midi_driver*)alsa_driver;
}

void platform_midi_deinit_alsa(struct platform_midi_driver* driver)
{
    struct platform_midi_alsa_driver *alsa_driver = (struct platform_midi_alsa_driver*)driver;

    snd_midi_event_free(alsa_driver->event_parser);
    snd_seq_delete_port(alsa_driver->seq_handle, alsa_driver->in_port);
    snd_seq_close(alsa_driver->seq_handle);
    if (alsa_driver->sysexBuf)
    {
        alsa_driver->sysexBuflen = 0;
        alsa_driver->sysexWriteOff = 0;
        free(alsa_driver->sysexBuf);
        alsa_driver->sysexBuf = NULL;
    }
    free(alsa_driver);
}

int platform_midi_read_alsa(struct platform_midi_driver* driver, unsigned char * out, int size)
{
    struct platform_midi_alsa_driver *alsa_driver = (struct platform_midi_alsa_driver*)driver;
    snd_seq_event_t *ev = NULL;

    if (alsa_driver->sysexPending)
    {
        if (alsa_driver->sysexWriteOff - alsa_driver->sysexReadOff <= size)
        {
            // write the whole thing out and then we're done!
            memcpy(out, alsa_driver->sysexBuf + alsa_driver->sysexReadOff, alsa_driver->sysexWriteOff - alsa_driver->sysexReadOff);
            alsa_driver->sysexPending = 0;
            return alsa_driver->sysexWriteOff - alsa_driver->sysexReadOff;
        }
        else
        {
            // Partial write, fill the whole buffer
            memcpy(out, alsa_driver->sysexBuf + alsa_driver->sysexReadOff, size);
            alsa_driver->sysexReadOff += size;
            return size;
        }
    }
    else
    {
        int result = snd_seq_event_input(alsa_driver->seq_handle, &ev);
        if (result == -EAGAIN)
        {
            return 0;
        }

        if (ev->type == SND_SEQ_EVENT_SYSEX && ev->data.ext.len > size)
        {
            // We have a SysEx event, which may not fit in the buffer!
            // (otherwise max is 12 and we assume the caller knows that...)
            if (!alsa_driver->sysexBuf)
            {
                unsigned int v = ev->data.ext.len;
                v--;
                v |= v >> 1;
                v |= v >> 2;
                v |= v >> 4;
                v |= v >> 8;
                v |= v >> 16;
                v++;

                alsa_driver->sysexBuf = malloc(v);
                alsa_driver->sysexBuflen = v;
            }
            else if (alsa_driver->sysexWriteOff + ev->data.ext.len > alsa_driver->sysexBuflen)
            {
                unsigned int v = ev->data.ext.len + alsa_driver->sysexWriteOff;
                v--;
                v |= v >> 1;
                v |= v >> 2;
                v |= v >> 4;
                v |= v >> 8;
                v |= v >> 16;
                v++;
                alsa_driver->sysexBuf = realloc(alsa_driver->sysexBuf, v);
                alsa_driver->sysexBuflen = v;
            }

            // Should be room in the buffer now!
            alsa_driver->sysexWriteOff = 0;
            alsa_driver->sysexReadOff = 0;
            long convertResult = snd_midi_event_decode(alsa_driver->event_parser, alsa_driver->sysexBuf, alsa_driver->sysexBuflen, ev);
            if (convertResult < 0)
            {
                if (convertResult == -ENOMEM)
                {
                    printf("Err: event doesn't fit in %d bytes (ev.data.ext.len == %u\n", alsa_driver->sysexBuflen, ev->data.ext.len);
                }
                else if (convertResult == -ENOENT)
                {
                    printf("Err: noent???\n");
                }
                printf("Error: couldn't convert ALSA SysEx event to MIDI: %ld\n", convertResult);
                return -1;
            }

            alsa_driver->sysexPending = 1;
            alsa_driver->sysexWriteOff += convertResult;

            memcpy(out, alsa_driver->sysexBuf, size);
            alsa_driver->sysexReadOff += size;
            return size;
        }
        else if (ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED || ev->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED)
        {
            // Ignore, and we can't convert it to a MIDI event
            return 0;
        }
        else
        {
            long convertResult = snd_midi_event_decode(alsa_driver->event_parser, out, size, ev);
            if (convertResult < 0)
            {
                printf("Err: couldn't convert ALSA event to MIDI: %ld, type=%d\n", convertResult, (int)ev->type);
                return -1;
            }
            else
            {
                return convertResult;
            }
        }
    }
}

int platform_midi_avail_alsa(struct platform_midi_driver* driver)
{
    struct platform_midi_alsa_driver *alsa_driver = (struct platform_midi_alsa_driver*)driver;
    if (alsa_driver->sysexPending)
    {

    }
    return snd_seq_event_input_pending(alsa_driver->seq_handle, 1);
}

int platform_midi_write_alsa(struct platform_midi_driver* driver, const unsigned char* buf, int size)
{
    struct platform_midi_alsa_driver *alsa_driver = (struct platform_midi_alsa_driver*)driver;

    snd_seq_event_t ev;
    int total = 0;

    do
    {
        int result = snd_midi_event_encode(alsa_driver->event_parser, buf + total, size - total, &ev);

        if (result < 0)
        {
            return result;
        }

        total += result;
    } while (total < size);

    if (0 >= snd_seq_event_output(alsa_driver->seq_handle, &ev))
    {
        printf("Error sending event\n");
        return -1;
    }

    return total;
}
#endif

#endif
