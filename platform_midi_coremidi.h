#ifndef _PLATFORM_MIDI_COREMIDI_H_
#define _PLATFORM_MIDI_COREMIDI_H_

struct platform_midi_driver *platform_midi_init_coremidi(const char *name, void *data);
void platform_midi_deinit_coremidi(struct platform_midi_driver *driver);
int platform_midi_read_coremidi(struct platform_midi_driver *driver, unsigned char *out, int size);
int platform_midi_avail_coremidi(struct platform_midi_driver *driver);
int platform_midi_write_coremidi(struct platform_midi_driver *driver, const unsigned char *buf, int size);

#ifdef PLATFORM_MIDI_IMPLEMENTATION

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <MacTypes.h>
#include <CoreFoundation/CFString.h>
#include <CoreMIDI/CoreMIDI.h>

struct platform_midi_coremidi_driver
{
    platform_midi_deinit_fn deinitFn;
    platform_midi_avail_fn availFn;
    platform_midi_read_fn readFn;
    platform_midi_write_fn writeFn;
    void *data;

    struct platform_midi_ringbuf buffer;
    MIDIClientRef coremidi_client;
    MIDIPortRef coremidi_in_port;
    MIDIPortRef coremidi_out_port;
};

void platform_midi_receive_callback(const MIDIEventList* events, void* refcon)
{
    struct platform_midi_coremidi_driver *driver = (struct platform_midi_coremidi_driver*)refcon;
    printf("Recieved %u MIDI packets from CoreAudio\n", events->numPackets);

    for (unsigned int i = 0; i < events->numPackets; i++)
    {
        unsigned char data[16];
        int written = platform_midi_convert_ump(data, sizeof(data), events->packet[i].words, events->packet[i].wordCount);
        platform_midi_push_packet(&driver->buffer, data, written);
    }
}

struct platform_midi_driver *platform_midi_init_coremidi(const char* name, void *data)
{
    void *alloc = malloc(sizeof(struct platform_midi_coremidi_driver));

    if (!alloc)
    {
        printf("Failed to allocate driver struct\n");
        return NULL;
    }

    struct platform_midi_coremidi_driver *driver = (struct platform_midi_coremidi_driver*)alloc;
    driver->deinitFn = platform_midi_deinit_coremidi;
    driver->availFn = platform_midi_avail_coremidi;
    driver->readFn = platform_midi_read_coremidi;
    driver->writeFn = platform_midi_write_coremidi;
    driver->data = data;

    /* name: The client name */
    /* notifyProc: an optional callback for system changes */
    /* notifyRefCon: a nullable refCon for notifyRefCon*/
    CFStringRef nameCf = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    // Pass the driver as a void* for the callback
    OSStatus result = MIDIClientCreate(nameCf, NULL, driver, &driver->coremidi_client);

    if (0 != result)
    {
        printf("Failed to initialize CoreMIDI driver\n");
        free(alloc);
        return 0;
    }

    platform_midi_buffer_init(&driver->buffer);

    void (^receiveCbBlock)(const MIDIEventList* events, void* refcon) = ^void(const MIDIEventList* events, void *refcon) {
        platform_midi_receive_callback(events, refcon);
    };

    result = MIDIInputPortCreateWithProtocol(
        driver->coremidi_client,
        CFStringCreateWithCStringNoCopy(0, "listen:in", kCFStringEncodingUTF8, kCFAllocatorNull),
        kMIDIProtocol_1_0,
        &driver->coremidi_in_port,
        receiveCbBlock
    );

    if (0 != result)
    {
        printf("Failed to create CoreMIDI input port\n");

        MIDIClientDispose(driver->coremidi_client);
        return 0;
    }

    return (struct platform_midi_driver*)driver;
}

void platform_midi_deinit_coremidi(struct platform_midi_driver *driver)
{
    struct platform_midi_coremidi_driver *coremidi_driver = (struct platform_midi_coremidi_driver*)driver;
    OSStatus result = MIDIPortDispose(coremidi_driver->coremidi_in_port);

    if (0 != result)
    {
        printf("failed to destroy CoreMIDI input port\n");
    }

    result = MIDIClientDispose(coremidi_driver->coremidi_client);

    if (0 != result)
    {
        printf("Failed to deinitialize CoreMIDI driver\n");
    }

    free(coremidi_driver);
}

int platform_midi_read_coremidi(struct platform_midi_driver *driver, unsigned char * out, int size)
{
    struct platform_midi_coremidi_driver *coremidi_driver = (struct platform_midi_coremidi_driver*)driver;
    return platform_midi_pop_packet(&coremidi_driver->buffer, out, size);
}

int platform_midi_avail_coremidi(struct platform_midi_driver *driver)
{
    struct platform_midi_coremidi_driver *coremidi_driver = (struct platform_midi_coremidi_driver*)driver;
    return platform_midi_packet_count(&coremidi_driver->buffer);
}

int platform_midi_write_coremidi(struct platform_midi_driver *driver, const unsigned char* buf, int size)
{
    struct platform_midi_coremidi_driver *coremidi_driver = (struct platform_midi_coremidi_driver*)driver;
    printf("platform_midi_write_coremidi() not implemented\n");
    return 0;
}

#endif

#endif
