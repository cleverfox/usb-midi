#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "hw.h"
#include "usb_dev.h"


#define CDC
usbd_device *usb;
/*
 * All references in this file come from Universal Serial Bus Device Class
 * Definition for MIDI Devices, release 1.0.
 */

/* SysEx identity message, preformatted with correct USB framing information */
const uint8_t sysex_identity[] = {
	0x04,	/* USB Framing (3 byte SysEx) */
	0xf0,	/* SysEx start */
	0x7e,	/* non-realtime */
	0x00,	/* Channel 0 */
	0x04,	/* USB Framing (3 byte SysEx) */
	0x7d,	/* Educational/prototype manufacturer ID */
	0x66,	/* Family code (byte 1) */
	0x66,	/* Family code (byte 2) */
	0x04,	/* USB Framing (3 byte SysEx) */
	0x51,	/* Model number (byte 1) */
	0x19,	/* Model number (byte 2) */
	0x00,	/* Version number (byte 1) */
	0x04,	/* USB Framing (3 byte SysEx) */
	0x00,	/* Version number (byte 2) */
	0x01,	/* Version number (byte 3) */
	0x00,	/* Version number (byte 4) */
	0x05,	/* USB Framing (1 byte SysEx) */
	0xf7,	/* SysEx end */
	0,0,	/* Padding */
};


static char set[]="0123456789ABCDEF";
void xcout(unsigned char c){
   usart_send_blocking(USART1, set[(c>>4)&0x0f]);
   usart_send_blocking(USART1, set[c&0x0f]);
}

/*
 * Table B-1: MIDI Adapter Device Descriptor
 */

static void usbmidi_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;

	char buf[64];
	int len = usbd_ep_read_packet(usbd_dev, EP_MIDI_I, buf, 64);

	/* This implementation treats any message from the host as a SysEx
	 * identity request. This works well enough providing the host
	 * packs the identify request in a single 8 byte USB message.
	 */
        uint8_t handled=0;
	if (len>=4) {
            if((buf[0]==0x07 || buf[0]==0x06)){ //sysex
                if(buf[1]==0xf0){
                    usart_send_blocking(USART1, 'm');
                    usart_send_blocking(USART1, 's');
                    while (usbd_ep_write_packet(usbd_dev, EP_MIDI_O1, sysex_identity,
                                sizeof(sysex_identity)) == 0);
                    handled=1;
                }
            }
	}

        if(!handled){
            usart_send_blocking(USART1, 'M');
            xcout(buf[0]);
            int i=1;
            usart_send_blocking(USART1, ' ');
            for(;i<len;i++){
                xcout(buf[i]);
                usart_send_blocking(USART3, buf[i]);
            }
            usart_send_blocking(USART1, '_');
        }
	gpio_toggle(GPIOB, GPIO8);
}

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;

	char buf[64];
	int len = usbd_ep_read_packet(usbd_dev, EP_CDC_R, buf, 64);

	if (len) {
		usbd_ep_write_packet(usbd_dev, EP_CDC_T, buf, len);
		buf[len] = 0;
	}
        usart_send_blocking(USART1, 'S');
        int i=0;
        for(;i<len;i++){
            xcout(buf[i]);
        }
	gpio_toggle(GPIOB, GPIO9);
}

static int cdcacm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch(req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void *)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		// usbd_ep_write_packet(0x83, buf, 10);
		return 1;
		}
	case USB_CDC_REQ_SET_LINE_CODING: 
		if(*len < sizeof(struct usb_cdc_line_coding))
			return 0;

		return 1;
	}
	return 0;
}


static void usb_set_config(usbd_device *usbd_dev, uint16_t wValue) {
	(void)wValue;


	usbd_ep_setup(usbd_dev, EP_MIDI_I, USB_ENDPOINT_ATTR_BULK, 64, usbmidi_data_rx_cb);
	usbd_ep_setup(usbd_dev, EP_MIDI_O1, USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, EP_MIDI_O2, USB_ENDPOINT_ATTR_BULK, 64, NULL);

	usbd_ep_setup(usbd_dev, EP_CDC_R, USB_ENDPOINT_ATTR_BULK, 64, cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, EP_CDC_T, USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, EP_CDC_I, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);
}


static void button_send_event(usbd_device *usbd_dev, int pressed)
{
	char buf[4] = { 0x08, /* USB framing: virtual cable 0, note on */
			0x88, /* MIDI command: note on, channel 1 */
			60,   /* Note 60 (middle C) */
			127,   /* "Normal" velocity */
	};
        buf[2] = pressed;

        buf[0] = 0x09;
        buf[1] = 0x90;
	while (usbd_ep_write_packet(usbd_dev, EP_MIDI_O1, buf, sizeof(buf)) == 0);

        buf[0] = 0x08;
        buf[1] = 0x80;
	while (usbd_ep_write_packet(usbd_dev, EP_MIDI_O1, buf, sizeof(buf)) == 0);

}

static void button_poll(usbd_device *usbd_dev)
{
	static uint32_t button_state = 0;

	/* This is a simple shift based debounce. It's simplistic because
	 * although this implements debounce adequately it does not have any
	 * noise suppression. It is also very wide (32-bits) because it can
	 * be polled in a very tight loop (no debounce timer).
	 */
	uint32_t old_button_state = button_state;
	button_state = (button_state << 1) | (GPIOC_IDR & 2);
        if ((0 == button_state) != (0 == old_button_state)) {
            usart_send_blocking(USART1, '.');
            /*
            usart_send_blocking(USART1, 'p');
            usart_send_blocking(USART1, 'r');
            usart_send_blocking(USART1, 'e');
            usart_send_blocking(USART1, 'v');
            usart_send_blocking(USART1, 'e');
            usart_send_blocking(USART1, 'd');
            usart_send_blocking(USART1, '\r');
            usart_send_blocking(USART1, '\n');
            */
            button_send_event(usbd_dev, !!button_state);
        }
}

void usart2_isr(void) {
    if (((USART_CR1(USART1) & USART_CR1_RXNEIE) != 0) &&
            ((USART_SR(USART1) & USART_SR_RXNE) != 0)) {
        //button_state=!button_state;
        //button_send_event(usb, button_state);
    }

    /* Check if we were called because of TXE. */
    if (((USART_CR1(USART2) & USART_CR1_TXEIE) != 0) &&
            ((USART_SR(USART2) & USART_SR_TXE) != 0)) {
        /*
           uint8_t status = atomQueueGet(&uart2_tx, 0, &data);
           if(status == ATOM_OK){
           usart_send_blocking(USART2, data);
           }else{
           */
        USART_CR1(USART2) &= ~USART_CR1_TXEIE;
    }
}


void usart1_isr(void) {
    static uint8_t data = 'A';
    //atomIntEnter();

    if (((USART_CR1(USART1) & USART_CR1_RXNEIE) != 0) &&
            ((USART_SR(USART1) & USART_SR_RXNE) != 0)) {
        data = usart_recv(USART1);

        if(data=='\r' || data=='\n'){
            usart_send_blocking(USART1, '\r');
            usart_send_blocking(USART1, '\n');
        }else{
            usart_send_blocking(USART1, data);
            button_send_event(usb, data);
        }

        //atomQueuePut(&uart1_rx,0, (uint8_t*) &data);
    }

    /* Check if we were called because of TXE. */
    if (((USART_CR1(USART1) & USART_CR1_TXEIE) != 0) &&
            ((USART_SR(USART1) & USART_SR_TXE) != 0)) {
        /*
        uint8_t status = atomQueueGet(&uart1_tx, 0, &data);
        if(status == ATOM_OK){
            usart_send_blocking(USART1, data);
        }else{
        */
            USART_CR1(USART1) &= ~USART_CR1_TXEIE;
        //}
    }
    //atomIntExit(0);
}

void usart3_isr(void) {
    static uint8_t data = 'A';
    //atomIntEnter();
    if (((USART_CR1(USART3) & USART_CR1_RXNEIE) != 0) &&
            ((USART_SR(USART3) & USART_SR_RXNE) != 0)) {
        data = usart_recv(USART3);
        usart_send_blocking(USART1, data);

        //atomQueuePut(&uart3_rx,0, (uint8_t*) &data);
    }

    /* Check if we were called because of TXE. */
    if (((USART_CR1(USART3) & USART_CR1_TXEIE) != 0) &&
            ((USART_SR(USART3) & USART_SR_TXE) != 0)) {
        /*
        uint8_t status = atomQueueGet(&uart3_tx, 0, &data);
        if(status == ATOM_OK){
            usart_send_blocking(USART3, data);
        }else{
        */
            USART_CR1(USART3) &= ~USART_CR1_TXEIE;
        //}
    }
    //atomIntExit(0);
}


int main(void) {
	rcc_clock_setup_in_hsi_out_48mhz();
        //rcc_clock_setup_in_hse_8mhz_out_24mhz();

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_AFIO);
        rcc_periph_clock_enable(RCC_USART1);
        rcc_periph_clock_enable(RCC_USART2);
        rcc_periph_clock_enable(RCC_USART3);

	AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON;

	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, 0, GPIO15);

        usart_setup();

	//desig_get_unique_id_as_string(usb_serial_number, sizeof(usb_serial_number));
        //usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
        /*
	usbd_dev = usbd_init(&otgfs_usb_driver, &dev, &config,
			usb_strings, 3,
			usbd_control_buffer, sizeof(usbd_control_buffer));
                        */


	gpio_set(GPIOA, GPIO15);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO15);

	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO8|GPIO9);

	/* Button pin */

        gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO1);
        gpio_set(GPIOC, GPIO1);

        usb=init_usb();
	usbd_register_set_config_callback(usb, usb_set_config);

        while (1){
            usbd_poll(usb);
            button_poll(usb);
        }
}