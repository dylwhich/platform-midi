#ifndef _PLATFORM_MIDI_COREMIDI_H_
#define _PLATFORM_MIDI_COREMIDI_H_

struct platform_midi_driver *platform_midi_init_coremidi(const char *name, void *data);
void platform_midi_deinit_coremidi(struct platform_midi_driver *driver);
int platform_midi_read_coremidi(struct platform_midi_driver *driver, unsigned char *out, int size);
int platform_midi_avail_coremidi(struct platform_midi_driver *driver);
int platform_midi_write_coremidi(struct platform_midi_driver *driver, const unsigned char *buf, int size);

#define PLATFORM_MIDI_IMPLEMENTATION
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
    // This is from our perspective, but the terminology for CoreMIDI is from the other app's persective
    // So, in_endpoint represents a MIDI Destination
    MIDIEndpointRef in_endpoint;
    // And out_endpoint represents a MIDI Source
    MIDIEndpointRef out_endpoint;
};

void platform_midi_receive_callback(const MIDIEventList* events, void* refcon, struct platform_midi_coremidi_driver* driver)
{
    for (unsigned int i = 0; i < events->numPackets; i++)
    {
        unsigned char data[16];
        int written = platform_midi_convert_from_ump(data, sizeof(data), events->packet[i].words, events->packet[i].wordCount);
        platform_midi_push_packet(&driver->buffer, data, written);
    }
}

void platform_midi_notify_callback(const MIDINotification* message, void* refcon)
{
    struct platform_midi_coremidi_driver* driver = (struct platform_midi_coremidi_driver*)refcon;

    switch (message->messageID)
    {
        case kMIDIMsgSetupChanged:
        // Some aspect of the current MIDI setup changed.
        break;

        case kMIDIMsgObjectAdded:
        {
            // The system added a device, entity, or endpoint.
            const MIDIObjectAddRemoveNotification* addMessage = (MIDIObjectAddRemoveNotification*)message;

            if (addMessage->childType & kMIDIObjectType_Source)
            {
                OSStatus result = MIDIPortConnectSource(driver->coremidi_in_port, addMessage->child, NULL);
                if (0 != result)
                {
                    printf("Error connecting to newly found MIDI source\n");
                }
            }

            break;
        }

        case kMIDIMsgObjectRemoved:
        // The system removed a device, entity, or endpoint.
        // TODO: Do we need to manually disconnect from a removed source? Probably not
        break;

        case kMIDIMsgPropertyChanged:
        // An object’s property value changed.
        break;

        case kMIDIMsgThruConnectionsChanged:
        // The system created or disposed of a persistent MIDI Thru connection.
        break;

        case kMIDIMsgSerialPortOwnerChanged:
        // The system changed a serial port owner.
        break;

        case kMIDIMsgIOError:
        // A driver I/O error occurred.
        break;
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

    driver->in_endpoint = 0;
    driver->out_endpoint = 0;
    driver->coremidi_in_port = 0;
    driver->coremidi_out_port = 0;

    /* name: The client name */
    /* notifyProc: an optional callback for system changes */
    /* notifyRefCon: a nullable refCon for notifyRefCon*/
    CFStringRef nameCf = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    // Pass the driver as a void* for the callback
    OSStatus result = MIDIClientCreate(nameCf, NULL, driver, &driver->coremidi_client);

    if (0 != result)
    {
        printf("Failed to initialize CoreMIDI driver\n");
        goto fail;
    }

    platform_midi_buffer_init(&driver->buffer);

    void (^receiveCbBlock)(const MIDIEventList* events, void* refcon) = ^void(const MIDIEventList* events, void *refcon) {
        platform_midi_receive_callback(events, refcon, driver);
    };

    result = MIDIDestinationCreateWithProtocol(
        driver->coremidi_client,
        CFSTR("input"),
        kMIDIProtocol_1_0,
        &driver->in_endpoint,
        receiveCbBlock
    );

    if (0 != result)
    {
        printf("Failed to create CoreMIDI input port\n");
        goto fail;
    }

    result = MIDIInputPortCreateWithProtocol(
        driver->coremidi_client,
        CFSTR("input"),
        kMIDIProtocol_1_0,
        &driver->coremidi_in_port,
        receiveCbBlock
    );

    result = MIDIOutputPortCreate(
        driver->coremidi_client,
        CFSTR("output"),
        &driver->coremidi_out_port
    );

    if (0 != result)
    {
        printf("Failed to create CoreMIDI output port\n");
        goto fail;
    }

    result = MIDISourceCreateWithProtocol(
        driver->coremidi_client,
        nameCf,
        kMIDIProtocol_1_0,
        &driver->out_endpoint
    );

    if (0 != result)
    {
        printf("Failed to create CoreMIDI source\n");
        goto fail;
    }

    // Now, connect up all the sources to the input port
    int sourceCount = MIDIGetNumberOfSources();
    for (int i = 0; i < sourceCount; i++)
    {
        MIDIEndpointRef ref = MIDIGetSource(i);
        result = MIDIPortConnectSource(driver->coremidi_in_port, ref, NULL);
        if (0 != result)
        {
            printf("Failed to connect input port to MIDI source #%d\n", i);
        }
    }

    // And connect up all the outputs
    // TODO I think we have to manually send to all the destinations
    /*int destCount = MIDIGetNumberOfDestinations();
    for (int i = 0; i < destcount; i++)
    {
        MIDIEndpointRef ref = MIDIGetDestination(i);
        result = MIDIPortConnectDestination(driver->coremidi_out_port, ref, NULL);
    }*/

    return (struct platform_midi_driver*)driver;

fail:
    if (driver)
    {
        if (driver->out_endpoint) MIDIEndpointDispose(driver->out_endpoint);
        if (driver->coremidi_out_port) MIDIPortDispose(driver->coremidi_out_port);
        if (driver->in_endpoint) MIDIEndpointDispose(driver->in_endpoint);
        if (driver->coremidi_in_port) MIDIPortDispose(driver->coremidi_in_port);
        if (driver->coremidi_client) MIDIClientDispose(driver->coremidi_client);

        free(driver);
    }

    return NULL;
}

void platform_midi_deinit_coremidi(struct platform_midi_driver *driver)
{
    struct platform_midi_coremidi_driver *coremidi_driver = (struct platform_midi_coremidi_driver*)driver;

    OSStatus result = MIDIEndpointDispose(coremidi_driver->out_endpoint);
    if (0 != result)
    {
        printf("failed to destroy CoreMIDI out endpoint\n");
    }

    result = MIDIPortDispose(coremidi_driver->coremidi_out_port);
    if (0 != result)
    {
        printf("failed to destroy CoreMIDI output port\n");
    }

    result = MIDIEndpointDispose(coremidi_driver->in_endpoint);
    if (0 != result)
    {
        printf("failed to destroy CoreMIDI in endpoint\n");
    }

    result = MIDIPortDispose(coremidi_driver->coremidi_in_port);
    if (0 != result)
    {
        printf("failed to destroyCoreMIDI in port\n");
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

    // Convert buffer to event list...
    // Then send it
    // MIDISend() is deprecated of course
    MIDIEventList list;
    MIDIEventPacket *packet = MIDIEventListInit(&list, kMIDIProtocol_1_0);
    packet->wordCount = platform_midi_convert_to_ump(packet->words, 64, buf, size);

    OSStatus result = MIDISendEventList(coremidi_driver->coremidi_out_port, coremidi_driver->out_endpoint, &list);

    if (0 != result)
    {
        return 0;
    }

    return size;
}

#endif

#endif
