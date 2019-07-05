#include <stddef.h>
#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/st_usbfs.h>
//#include <libopencm3/usb/usbd.h>
//#include <libopencm3/usb/cdc.h>

// stm32f103xx reference manual: https://www.st.com/content/ccc/resource/technical/document/reference_manual/59/b9/ba/7f/11/af/43/d5/CD00171190.pdf/files/CD00171190.pdf/jcr:content/translations/en.CD00171190.pdf
//   usb: chapter 23, page 622
// usb overview: https://www.beyondlogic.org/usbnutshell/usb5.shtml
// usb example: https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/stm32-maple/usb_cdcacm/cdcacm.c


// usb device descriptor, see libusb.h
struct usbDeviceDescriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
};

static const struct usbDeviceDescriptor device = {
	.bLength = sizeof(struct usbDeviceDescriptor),
	.bDescriptorType = 0x01,//USB_DT_DEVICE,
	.bcdUSB = 0x0200, // USB 2.0
	.bDeviceClass = 0, //USB_CLASS_CDC, // communications device class
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64, // max packet size for endpoint 0
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200, // device version
	.iManufacturer = 0, // index into string table
	.iProduct = 0, // index into string table
	.iSerialNumber = 0, // index into string table
	.bNumConfigurations = 1,
};


struct usbRequest {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};

struct usbRequest request;

//uint8_t buffer[128];


void setupUsb() {
	// clear interrupts of usb
	SET_REG(USB_ISTR_REG, 0);

	// packet memory layout
	// offset | size | description
	//      0 |    8 | buffer table
	//      8 |   64 | tx buffer of endpoint 0
	//     72 |   64 | rx buffer of endpoint 0

	// set buffer table address inside packet memory (USB_PMA_BASE)
	SET_REG(USB_BTABLE_REG, 0);
	
	// setup buffers for endpoint 0
	SET_REG(USB_EP_TX_ADDR(0), 8);
	//SET_REG(USB_EP_TX_COUNT(0), ??); set tx buffer size when actually sending
	SET_REG(USB_EP_RX_ADDR(0), 72);
	SET_REG(USB_EP_RX_COUNT(0), 0x8000 | (2 << 10)); // rx buffer size is 64

	// setup endpoint 0
	SET_REG(USB_EP_REG(0), USB_EP_TYPE_CONTROL | USB_EP_RX_STAT_VALID);

	// enable usb at address 0
	SET_REG(USB_DADDR_REG, USB_DADDR_EF);
}

inline int min(int a, int b) {
	return a < b ? a : b;
}

enum Mode {
	IDLE,
	SET_ADDRESS,
	GET_DESCRIPTOR,
};
uint8_t address = 0;

void send(int ep, const void *data, int size) {
	// copy data from flash into tx buffer
	const uint16_t * src = (const uint16_t*)data;
	uint16_t * dst = (uint16_t*)USB_GET_EP_TX_BUFF(ep);
	int s = (size + 1) / 2;
	for (int i = 0; i < s; ++i) {
		*dst = *src;
		++src; // ABP1 is 32 bit only
		dst += 2;
	}
	
	// set size of packet in tx buffer
	SET_REG(USB_EP_TX_COUNT(ep), size);
	
	
	// don't change rx flag
	uint16_t one = USB_EP_RX_CTR; 

	// clear tx flag and don't change toggle other toggle flags
	uint16_t zero = USB_EP_TX_CTR | USB_EP_RX_DTOG | USB_EP_TX_DTOG | USB_EP_RX_STAT;

	// indicate that we are ready to send
	uint16_t epReg = GET_REG(USB_EP_REG(ep));
	SET_REG(USB_EP_REG(ep), one | ((epReg ^ USB_EP_TX_STAT_VALID) & ~zero));
}

int main(void) {
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_USB);
	
	
	// 23.4.2 System and power-on reset
	
	// switch on usb transceiver, but keep reset
	SET_REG(USB_CNTR_REG, USB_CNTR_FRES);
	
	// wait
	for (int i = 0; i < 0x80000; i++)
		__asm__("nop");
			
	// exit reset of usb
	SET_REG(USB_CNTR_REG, 0);
		
	
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	gpio_clear(GPIOC, GPIO13); // led on


	// setup in default state
	setupUsb();

	// wait for incoming request or reset
	enum Mode mode = IDLE;
	while (1) {
		if (GET_REG(USB_ISTR_REG) & USB_ISTR_RESET) {
			// reset detected: setup in default state
			setupUsb();		
		}
		uint16_t ep0 = GET_REG(USB_EP_REG(0));
		if (ep0 & USB_EP_RX_CTR) {
			if (ep0 & USB_EP_SETUP) {
				// received a setup packet from the host
				if ((GET_REG(USB_EP_RX_COUNT(0)) & 0x3ff) >= sizeof(struct usbRequest)) {
					
					// copy request from rx buffer to system memory
					uint16_t * src = (uint16_t*)USB_GET_EP_RX_BUFF(0);
					uint16_t * dst = (uint16_t*)&request;
					for (int i = 0; i < sizeof(struct usbRequest)/2; ++i) {
						*dst = *src;
						src += 2; // ABP1 is 32 bit only
						++dst;
					}
	
					// enable receiving again
					SET_REG(USB_EP_REG(0), USB_EP_TX_CTR | USB_EP_TYPE_CONTROL | (USB_EP_RX_STAT_NAK ^ USB_EP_RX_STAT_VALID));
					
					// check request type
					// https://www.beyondlogic.org/usbnutshell/usb6.shtml			
					if (request.bmRequestType == 0x00) {
						// write request to standard device
						if (request.bRequest == 0x05) {
							// set address
							mode = SET_ADDRESS;
							address = request.wValue;
							
							// setup zero length packet in tx buffer for status stage
							send(0, NULL, 0);
						}
					} else if (request.bmRequestType == 0x80) {
						// read request to standard device
						//gpio_set(GPIOC, GPIO13); // led off						
						if (request.bRequest == 0x06) {
							// get descriptor
							mode = GET_DESCRIPTOR;
							
							// copy descriptor from flash into tx buffer
							/*uint16_t * src = (uint16_t*)&device;
							uint16_t * dst = (uint16_t*)USB_GET_EP_TX_BUFF(0);
							int size = min(sizeof(struct usbDeviceDescriptor), request.wLength)+1;
							for (int i = 0; i < size/2; ++i) {
								*dst = *src;
								++src; // ABP1 is 32 bit only
								dst += 2;
							}
							SET_REG(USB_EP_TX_COUNT(0), size);
							
							// indicate that we are ready to send
							SET_REG(USB_EP_REG(0), USB_EP_RX_CTR | USB_EP_TYPE_CONTROL | (USB_EP_TX_STAT_NAK ^ USB_EP_TX_STAT_VALID));
							*/
							
							// copy device descriptor to tx buffer
							int size = min(sizeof(struct usbDeviceDescriptor), request.wLength);
							send(0, &device, size);
						}
					}					
				} else {
					
					// error: enable receiving again, stall send
					//receive(0, NULL, 0);
					//stallSend(0);
					
				}
			} else {
				// received a packet from the host
				switch (mode) {
				case GET_DESCRIPTOR:
					// zlp received (status stage)
				gpio_set(GPIOC, GPIO13); // led off
					mode = IDLE;
					break;					
				}
				
			}
		}
		if (ep0 & USB_EP_TX_CTR) {
			// sent a packet to the host
			switch (mode) {
			case SET_ADDRESS:
				// zlp sent (status stage), now we can set the address
				SET_REG(USB_DADDR_REG, USB_DADDR_EF | address);
				mode = IDLE;
				break;
			case GET_DESCRIPTOR:
				// prepare next data packet (data stage)
				send(0, NULL, 0);
				
				//SET_REG(USB_EP_TX_COUNT(0), 0);

				// indicate that we are ready to send
				//SET_REG(USB_EP_REG(0), USB_EP_RX_CTR | USB_EP_TYPE_CONTROL | (USB_EP_TX_STAT_NAK ^ USB_EP_TX_STAT_VALID));
				
				break;
			}

		}
	}
	return 0;
}
