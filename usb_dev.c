#include "usb_dev.h"

static const struct usb_device_descriptor dev = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,    /* was 0x0110 in Table B-1 example descriptor */
    .bDeviceClass = 0,   /* device defined at interface level */
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x3137,  /* Prototype product vendor ID */
    .idProduct = 0xC0DE, /* dd if=/dev/random bs=2 count=1 | hexdump */
    .bcdDevice = 0x0100,
    .iManufacturer = 1,  /* index to string desc */
    .iProduct = 2,       /* index to string desc */
    .iSerialNumber = 3,  /* index to string desc */
    .bNumConfigurations = 1,
};

/*
 * Midi specific endpoint descriptors.
 */
struct usb_midi_endpoint_descriptor2 {
	struct usb_midi_endpoint_descriptor_head head;
	struct usb_midi_endpoint_descriptor_body jack[2];
} __attribute__((packed));


static const struct usb_midi_endpoint_descriptor midi_bulk_endp_out = {
    /* Table B-12: MIDI Adapter Class-specific Bulk OUT Endpoint
     * Descriptor
     */
    .head = {
        .bLength = sizeof(struct usb_midi_endpoint_descriptor),
        .bDescriptorType = USB_AUDIO_DT_CS_ENDPOINT,
        .bDescriptorSubType = USB_MIDI_SUBTYPE_MS_GENERAL,
        .bNumEmbMIDIJack = 1,
    },
        .jack[0] = { .baAssocJackID = 0x01 }, 
};

static const struct usb_midi_endpoint_descriptor midi_bulk_endp_in = {
    /* Table B-14: MIDI Adapter Class-specific Bulk IN Endpoint
     * Descriptor
     */
    .head = {
        .bLength = sizeof(struct usb_midi_endpoint_descriptor),
        .bDescriptorType = USB_AUDIO_DT_CS_ENDPOINT,
        .bDescriptorSubType = USB_MIDI_SUBTYPE_MS_GENERAL,
        .bNumEmbMIDIJack = 1,
    },
        .jack[0] = { .baAssocJackID = 0x04 }, 
//        .jack[1] = { .baAssocJackID = 0x06 }, 
};

/*
 * Standard endpoint descriptors
 */
static const struct usb_endpoint_descriptor bulk_endp[] = {
    { //from computer to me
        // Table B-11: MIDI Adapter Standard Bulk OUT Endpoint Descriptor 
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_MIDI_I,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 0x40,
        .bInterval = 0x00,

        .extra = &midi_bulk_endp_out,
        .extralen = sizeof(midi_bulk_endp_out)
    }, { //from me to computer
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_MIDI_O,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 0x40,
        .bInterval = 0x00,

        .extra = &midi_bulk_endp_in,
        .extralen = sizeof(midi_bulk_endp_in)
    }
};

/*
 * Table B-4: MIDI Adapter Class-specific AC Interface Descriptor
 */
static const struct {
    struct usb_audio_header_descriptor_head header_head;
    struct usb_audio_header_descriptor_body header_body;
} __attribute__((packed)) audio_control_functional_descriptors = {
    .header_head = {
        .bLength = sizeof(struct usb_audio_header_descriptor_head) +
            1 * sizeof(struct usb_audio_header_descriptor_body),
        .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
        .bDescriptorSubtype = USB_AUDIO_TYPE_HEADER,
        .bcdADC = 0x0100,
        .wTotalLength =
            sizeof(struct usb_audio_header_descriptor_head) +
            1 * sizeof(struct usb_audio_header_descriptor_body),
        .binCollection = 1,
    },
    .header_body = {
        .baInterfaceNr = 0x01,
    },
};


/*
 * Table B-3: MIDI Adapter Standard AC Interface Descriptor
 */
static const struct usb_interface_descriptor audio_control_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = IF_ACTL,
        .bAlternateSetting = 0,
        .bNumEndpoints = 0,
        .bInterfaceClass = USB_CLASS_AUDIO,
        .bInterfaceSubClass = USB_AUDIO_SUBCLASS_CONTROL,
        .bInterfaceProtocol = 0,
        .iInterface = 0,

        .extra = &audio_control_functional_descriptors,
        .extralen = sizeof(audio_control_functional_descriptors)
} };

/*
 * Class-specific MIDI streaming interface descriptor
 */
const struct {
    struct usb_midi_header_descriptor header;

    struct usb_midi_in_jack_descriptor in_embedded1;
    struct usb_midi_out_jack_descriptor out_external1;

    struct usb_midi_in_jack_descriptor in_external1;
    struct usb_midi_out_jack_descriptor out_embedded1;

    struct usb_midi_in_jack_descriptor in_external2;
    struct usb_midi_out_jack_descriptor out_embedded2;

} __attribute__((packed)) midi_streaming_functional_descriptors = {
    /* Table B-6: Midi Adapter Class-specific MS Interface Descriptor */
    .header = {
        .bLength = sizeof(struct usb_midi_header_descriptor),
        .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
        .bDescriptorSubtype = USB_MIDI_SUBTYPE_MS_HEADER,
        .bcdMSC = 0x0100,
        .wTotalLength = sizeof(midi_streaming_functional_descriptors),
    },
    /* Table B-7: MIDI Adapter MIDI IN Jack Descriptor (Embedded) */
    .in_embedded1 = {
        .bLength = sizeof(struct usb_midi_in_jack_descriptor),
        .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
        .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_IN_JACK, //2
        .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED, //1
        .bJackID = 0x01,
        .iJack = 0x00,
    },
    /* Table B-10: MIDI Adapter MIDI OUT Jack Descriptor (External) */
    .out_external1 = {
        .head = {
            .bLength = sizeof(struct usb_midi_out_jack_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_OUT_JACK, //3
            .bJackType = USB_MIDI_JACK_TYPE_EXTERNAL, //2
            .bJackID = 0x02,
            .bNrInputPins = 1,
        },
        .source[0] = {
            .baSourceID = 0x01,
            .baSourcePin = 0x01,
        },
        .tail = {
            .iJack = 0x00,
        },
    },
    /* Table B-8: MIDI Adapter MIDI IN Jack Descriptor (External) */
    .in_external1 = {
        .bLength = sizeof(struct usb_midi_in_jack_descriptor),
        .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
        .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_IN_JACK, //2
        .bJackType = USB_MIDI_JACK_TYPE_EXTERNAL, //2
        .bJackID = 0x03,
        .iJack = 0x00,
    },
    /* Table B-9: MIDI Adapter MIDI OUT Jack Descriptor (Embedded) */
    .out_embedded1 = {
        .head = {
            .bLength = sizeof(struct usb_midi_out_jack_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_OUT_JACK, //3
            .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,  //1
            .bJackID = 0x04,
            .bNrInputPins = 1,
        },
        .source[0] = {
            .baSourceID = 0x03,
            .baSourcePin = 0x01,
        },
        .tail = {
            .iJack = 0x00,
        }
    },

    /* Table B-8: MIDI Adapter MIDI IN Jack Descriptor (External) */
    .in_external2 = {
        .bLength = sizeof(struct usb_midi_in_jack_descriptor),
        .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
        .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_IN_JACK, //2
        .bJackType = USB_MIDI_JACK_TYPE_EXTERNAL, //2
        .bJackID = 0x05,
        .iJack = 0x00,
    },
    /* Table B-9: MIDI Adapter MIDI OUT Jack Descriptor (Embedded) */
    .out_embedded2 = {
        .head = {
            .bLength = sizeof(struct usb_midi_out_jack_descriptor),
            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_OUT_JACK, //3
            .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,  //1
            .bJackID = 0x06,
            .bNrInputPins = 1,
        },
        .source[0] = {
            .baSourceID = 0x05,
            .baSourcePin = 0x01,
        },
        .tail = {
            .iJack = 0x00,
        }
    },
};

/*
 * Table B-5: MIDI Adapter Standard MS Interface Descriptor
 */
static const struct usb_interface_descriptor midi_streaming_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = IF_MIDI,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_CLASS_AUDIO,
        .bInterfaceSubClass = USB_AUDIO_SUBCLASS_MIDISTREAMING,
        .bInterfaceProtocol = 0,
        .iInterface = 0,

        .endpoint = bulk_endp,

        .extra = &midi_streaming_functional_descriptors,
        .extralen = sizeof(midi_streaming_functional_descriptors)
} };

static const struct {
    struct usb_cdc_header_descriptor header;
    struct usb_cdc_call_management_descriptor call_mgmt;
    struct usb_cdc_acm_descriptor acm;
    struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm0_functional_descriptors = {
    .header = {
        .bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_HEADER,
        .bcdCDC = 0x0110,
    },
    .call_mgmt = {
        .bFunctionLength = 
            sizeof(struct usb_cdc_call_management_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
        .bmCapabilities = 0,
        .bDataInterface = IF_CDAT0,
    },
    .acm = {
        .bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_ACM,
        .bmCapabilities = 0,
    },
    .cdc_union = {
        .bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_UNION,
        .bControlInterface = IF_COMM0,
        .bSubordinateInterface0 = IF_CDAT0,
    }
};

static const struct usb_endpoint_descriptor comm0_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_CDC0_I,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 16,
        .bInterval = 255,
}};
static const struct usb_endpoint_descriptor data0_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_CDC0_R,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
}, {
    .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = EP_CDC0_T,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = 64,
        .bInterval = 1,
}};

static const struct usb_interface_descriptor comm0_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = IF_COMM0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 1,
        .bInterfaceClass = USB_CLASS_CDC,
        .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
        .iInterface = 0,

        .endpoint = comm0_endp,

        .extra = &cdcacm0_functional_descriptors,
        .extralen = sizeof(cdcacm0_functional_descriptors)
}};

static const struct usb_interface_descriptor data0_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = IF_CDAT0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_CLASS_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,

        .endpoint = data0_endp,
}};

static const struct usb_interface ifaces[] = {{
    .num_altsetting = 1,
        .altsetting = audio_control_iface,
}, {
    .num_altsetting = 1,
        .altsetting = midi_streaming_iface,
}, {
    .num_altsetting = 1,
        .altsetting = comm0_iface,
}, {
    .num_altsetting = 1,
        .altsetting = data0_iface,
} };

/*
 * Table B-2: MIDI Adapter Configuration Descriptor
 */
static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0, /* can be anything, it is updated automatically
                          when the usb code prepares the descriptor */
    .bNumInterfaces = 4, /* control and data */
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80, /* bus powered */
    .bMaxPower = 0x32,

    .interface = ifaces,
};

static char usb_serial_number[25]; /* 12 bytes of desig and a \0 */

static const char * usb_strings[] = {
    "libopencm3.org",
    "MIDI interface",
    usb_serial_number
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[256];

usbd_device * init_usb(){
    usbd_device *usbd_dev;
    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, 
            usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
    return usbd_dev;
}

