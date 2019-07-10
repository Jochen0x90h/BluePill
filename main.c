#include <stddef.h>
#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/st_usbfs.h>

// stm32f103xx data sheet: https://www.st.com/resource/en/datasheet/CD00161566.pdf
// stm32f103xx reference manual: https://www.st.com/content/ccc/resource/technical/document/reference_manual/59/b9/ba/7f/11/af/43/d5/CD00171190.pdf/files/CD00171190.pdf/jcr:content/translations/en.CD00171190.pdf
//   usb: chapter 23, page 622
// usb overview: https://www.beyondlogic.org/usbnutshell/usb5.shtml
// libopencm3 example: https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f1/stm32-maple/usb_cdcacm/cdcacm.c
// other usb example: https://github.com/Erlkoenig90/f1usb
// usbmon: https://www.kernel.org/doc/Documentation/usb/usbmon.txt


inline void ledOn() {
	gpio_clear(GPIOC, GPIO13);
}

inline void ledOff() {
	gpio_set(GPIOC, GPIO13);
}

inline void ledToggle() {
	gpio_toggle(GPIOC, GPIO13);
}


// usb descriptors, see libusb.h
enum DescriptorType {
	DESCRIPTOR_DEVICE = 0x01,
	DESCRIPTOR_CONFIGURATION = 0x02,
	DESCRIPTOR_INTERFACE = 0x04,
	DESCRIPTOR_ENDPOINT = 0x05
};

enum EndpointType {
	ENDPOINT_CONTROL = 0,
	ENDPOINT_ISOCHRONOUS = 1,
	ENDPOINT_BULK = 2,
	ENDPOINT_INTERRUPT = 3
};

struct UsbDeviceDescriptor {
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
} __attribute__((packed));

struct UsbConfigDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} __attribute__((packed));

struct UsbInterfaceDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} __attribute__((packed));

struct UsbEndpointDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} __attribute__((packed));


// device descriptor
static const struct UsbDeviceDescriptor device = {
	.bLength = sizeof(struct UsbDeviceDescriptor),
	.bDescriptorType = DESCRIPTOR_DEVICE,
	.bcdUSB = 0x0200, // USB 2.0
	.bDeviceClass = 0xff, // no class
	.bDeviceSubClass = 0xff,
	.bDeviceProtocol = 0xff,
	.bMaxPacketSize0 = 64, // max packet size for endpoint 0
	.idVendor = 0x0483, // STMicroelectronics
	.idProduct = 0x5721, // Interrupt Demo
	.bcdDevice = 0x0100, // device version
	.iManufacturer = 0, // index into string table
	.iProduct = 0, // index into string table
	.iSerialNumber = 0, // index into string table
	.bNumConfigurations = 1
};

// configuration descriptor
struct Configuration {
	struct UsbConfigDescriptor config;
	struct UsbInterfaceDescriptor interface;
	struct UsbEndpointDescriptor endpoint1;
	struct UsbEndpointDescriptor endpoint2;
} __attribute__((packed));

static const struct Configuration configuration = {
	.config = {
		.bLength = sizeof(struct UsbConfigDescriptor),
		.bDescriptorType = DESCRIPTOR_CONFIGURATION,
		.wTotalLength = sizeof(struct Configuration),
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = 0x80, // bus powered
		.bMaxPower = 50 // 100 mA
	},
	.interface = {
		.bLength = sizeof(struct UsbInterfaceDescriptor),
		.bDescriptorType = DESCRIPTOR_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = 0xff, // no class
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0
	},
	.endpoint1 = {
		.bLength = sizeof(struct UsbEndpointDescriptor),
		.bDescriptorType = DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = 0x81, // in 1
		.bmAttributes = ENDPOINT_INTERRUPT,
		.wMaxPacketSize = 16,
		.bInterval = 1 // polling interval
	},
	.endpoint2 = {
		.bLength = sizeof(struct UsbEndpointDescriptor),
		.bDescriptorType = DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = 0x01, // out 1
		.bmAttributes = ENDPOINT_INTERRUPT,
		.wMaxPacketSize = 16,
		.bInterval = 1 // polling interval
	}
};


// control request, transferred in the setup packet
struct UsbRequest {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};

struct UsbRequest request;



void setupUsb() {
	// clear interrupts of usb
	SET_REG(USB_ISTR_REG, 0);

	// packet memory layout
	// offset | size | description
	//      0 |   32 | buffer table for 4 endpoints
	//     32 |   64 | tx buffer of endpoint 0
	//     96 |   64 | rx buffer of endpoint 0
	//    160 |   16 | tx buffer of endpoint 1
	//    176 |   16 | rx buffer of endpoint 2

	// set buffer table address inside packet memory (relative to USB_PMA_BASE)
	SET_REG(USB_BTABLE_REG, 0);
	
	// setup buffers for endpoint 0
	SET_REG(USB_EP_TX_ADDR(0), 32);
	//SET_REG(USB_EP_TX_COUNT(0), ??); set tx buffer size when actually sending
	SET_REG(USB_EP_RX_ADDR(0), 96);
	SET_REG(USB_EP_RX_COUNT(0), 0x8000 | (1 << 10)); // rx buffer size is 64

	// setup endpoint 0
	SET_REG(USB_EP_REG(0), USB_EP_TYPE_CONTROL | USB_EP_RX_STAT_VALID | 0);

	// enable usb at address 0
	SET_REG(USB_DADDR_REG, USB_DADDR_EF);
}

void setupEndpoints() {
	// clear rx and tx flags, endpoint type, kind and address
	uint16_t clear = USB_EP_RX_CTR | USB_EP_TX_CTR | USB_EP_TYPE | USB_EP_KIND | USB_EP_ADDR;

	// setup buffers for endpoint 1
	SET_REG(USB_EP_TX_ADDR(1), 160);
	//SET_REG(USB_EP_TX_COUNT(1), ??); set tx buffer size when actually sending
	SET_REG(USB_EP_RX_ADDR(1), 176);
	SET_REG(USB_EP_RX_COUNT(1), 8 << 10); // rx buffer size is 16

	// set endpoint type and address (overrides clear)
	uint16_t set = USB_EP_TYPE_INTERRUPT | 1;

	// stall send, ready to receive, clear other toggle bits
	uint16_t epReg = GET_REG(USB_EP_REG(1));
	SET_REG(USB_EP_REG(1), ((epReg ^ USB_EP_TX_STAT_STALL ^ USB_EP_RX_STAT_VALID) & ~clear) | set);
}

inline int min(int a, int b) {
	return a < b ? a : b;
}


void send(int ep, const void *data, int size) {
	// copy data from flash into tx buffer
	const uint16_t * src = (const uint16_t*)data;
	uint16_t * dst = (uint16_t*)USB_GET_EP_TX_BUFF(ep);
	int s = (size + 1) / 2;
	for (int i = 0; i < s; ++i) {
		*dst = *src;
		++src;
		dst += 2; // ABP1 bus is 32 bit only
	}
	
	// set size of packet in tx buffer
	SET_REG(USB_EP_TX_COUNT(ep), size);
	
	
	// clear tx flag and don't change other toggle flags
	uint16_t clear = USB_EP_TX_CTR | USB_EP_RX_DTOG | USB_EP_TX_DTOG | USB_EP_RX_STAT;

	// don't change rx flag
	uint16_t set = USB_EP_RX_CTR;

	// indicate that we are ready to send
	uint16_t epReg = GET_REG(USB_EP_REG(ep));
	SET_REG(USB_EP_REG(ep), ((epReg ^ USB_EP_TX_STAT_VALID) & ~clear) | set);
}

void sendStall(int ep) {
	// clear tx flag and don't change other toggle flags
	uint16_t clear = USB_EP_TX_CTR | USB_EP_RX_DTOG | USB_EP_TX_DTOG | USB_EP_RX_STAT;

	// don't change rx flag
	uint16_t set = USB_EP_RX_CTR;

	// indicate that we are ready to send
	uint16_t epReg = GET_REG(USB_EP_REG(ep));
	SET_REG(USB_EP_REG(ep), ((epReg ^ USB_EP_TX_STAT_STALL) & ~clear) | set);
}

void receive(int ep) {
	// clear rx flag and don't change other toggle flags
	uint16_t clear = USB_EP_RX_CTR | USB_EP_RX_DTOG | USB_EP_TX_DTOG | USB_EP_TX_STAT;

	// don't change tx flag
	uint16_t set = USB_EP_TX_CTR;

	// indicate that we are ready to receive
	uint16_t epReg = GET_REG(USB_EP_REG(ep));
	SET_REG(USB_EP_REG(ep), ((epReg ^ USB_EP_RX_STAT_VALID) & ~clear) | set);
}



enum Mode {
	IDLE,
	SET_ADDRESS,
	AWAIT_TX,
	GET_DESCRIPTOR,
};
uint8_t address = 0;

int main(void) {
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_USB);
	
	
	// 23.4.2 System and power-on reset
	
	// switch on usb transceiver, but keep reset
	SET_REG(USB_CNTR_REG, USB_CNTR_FRES);
	
	// wait for at least 1us (see data sheet: Table 43. USB startup time)
	for (int i = 0; i < 72; i++)
		__asm__("nop");
			
	// exit reset of usb
	SET_REG(USB_CNTR_REG, 0);
		
	
	// set PC13 to output for the LED
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	ledOn();


	// setup in default state
	setupUsb();

	// wait for incoming request or reset
	enum Mode mode = IDLE;
	while (1) {
		// check reset
		if (GET_REG(USB_ISTR_REG) & USB_ISTR_RESET) {
			// reset detected: setup in default state
			setupUsb();
		}

		// check control endpoint
		uint16_t ep0 = GET_REG(USB_EP_REG(0));
		if (ep0 & USB_EP_RX_CTR) {
			if (ep0 & USB_EP_SETUP) {
				// received a setup packet from the host
				if ((GET_REG(USB_EP_RX_COUNT(0)) & 0x3ff) >= sizeof(struct UsbRequest)) {

					// copy request from rx buffer to system memory
					uint16_t *src = (uint16_t *) USB_GET_EP_RX_BUFF(0);
					uint16_t *dst = (uint16_t *) &request;
					for (int i = 0; i < sizeof(struct UsbRequest) / 2; ++i) {
						*dst = *src;
						src += 2; // ABP1 bus is 32 bit only
						++dst;
					}

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
						} else if (request.bRequest == 0x09) {
							// set configuration
							mode = AWAIT_TX;
							uint8_t bConfigurationValue = request.wValue;
							setupEndpoints();

							// send first data
							send(1, &device, 4);

							// setup zero length packet in tx buffer for status stage
							send(0, NULL, 0);
						} else {
							// unsupported request: stall
							sendStall(0);
						}
					} else if (request.bmRequestType == 0x80) {
						// read request to standard device
						if (request.bRequest == 0x06) {
							// get descriptor
							mode = GET_DESCRIPTOR;

							uint8_t descriptorType = request.wValue >> 8;
							if (descriptorType == DESCRIPTOR_DEVICE) {
								// copy device descriptor to tx buffer
								int size = min(sizeof(struct UsbDeviceDescriptor), request.wLength);
								send(0, &device, size);
							} else if (descriptorType == DESCRIPTOR_CONFIGURATION) {
								int size = min(sizeof(struct Configuration), request.wLength);
								send(0, &configuration, size);
							}
						} else {
							// unsupported request: stall
							sendStall(0);
						}
					} else if (request.bmRequestType == 0x01) {
							// write request to standard interface
							if (request.bRequest == 0x0b) {
								// set interface
								mode = AWAIT_TX;
								uint8_t bInterface = request.wIndex;
								uint8_t bAlternateSetting = request.wValue;

								// setup zero length packet in tx buffer for status stage
								send(0, NULL, 0);
							} else {
								// unsupported request: stall
								sendStall(0);
							}
					} else if (request.bmRequestType == 0x02) {
						// write request to standard endpoint
						if (request.bRequest == 0x01) {
							// clear feature
							mode = AWAIT_TX;

							// setup zero length packet in tx buffer for status stage
							send(0, NULL, 0);
						} else {
							// unsupported request: stall
							sendStall(0);
						}
					} else {
						// unsupported request type: stall
						sendStall(0);
					}
				} else {
					// request too short: stall
					sendStall(0);
				}
			} else {
				// received a packet from the host
				switch (mode) {
					case GET_DESCRIPTOR:
						// zlp received (status stage)
						//ledOff();
						mode = IDLE;
						break;
				}
			}

			// enable receiving again
			receive(0);
		}
		if (ep0 & USB_EP_TX_CTR) {
			// sent a packet to the host
			switch (mode) {
				case SET_ADDRESS:
					// zlp sent (status stage), now we can set the address
					SET_REG(USB_DADDR_REG, USB_DADDR_EF | address);
					mode = IDLE;
					break;
				case AWAIT_TX:
					// zlp sent (status stage)
					mode = IDLE;
					break;
				case GET_DESCRIPTOR:
					// todo: prepare next data packet (data stage)
					send(0, NULL, 0);

					break;
			}

		}


		// check data endpoint
		uint16_t ep1 = GET_REG(USB_EP_REG(1));
		if (ep1 & USB_EP_TX_CTR) {
			ledToggle();

			// send next data
			send(1, &device, 4);
		}
		if (ep1 & USB_EP_RX_CTR) {
			if (*USB_GET_EP_RX_BUFF(1))
				ledOn();
			else
				ledOff();

			// receive next data
			receive(1);
		}
	}
	return 0;
}
