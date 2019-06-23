#include <stdio.h>
#include <string.h>
#include <libusb.h>

// https://github.com/libusb/libusb/blob/master/examples/listdevs.c
static void print_devs(libusb_device **devs)
{
	libusb_device *dev;
	int i = 0, j = 0;
	uint8_t path[8]; 

	// iterate over devices
	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return;
		}

		printf("%04x:%04x (bus %d, device %d)",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));

		r = libusb_get_port_numbers(dev, path, sizeof(path));
		if (r > 0) {
			printf(" path: %d", path[0]);
			for (j = 1; j < r; j++)
				printf(".%d", path[j]);
		}
		printf("\n");
	}
}

// https://github.com/libusb/libusb/blob/master/examples/testlibusb.c
static int print_device(libusb_device *dev, int level)
{
	libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	unsigned char string[256];
	int ret;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return -1;
	}

	printf("%04x:%04x\n", desc.idVendor, desc.idProduct);
	printf("\tBus: %d\n", libusb_get_bus_number(dev));
	printf("\tDevice: %d\n", libusb_get_device_address(dev));
	
	ret = libusb_open(dev, &handle);
	if (LIBUSB_SUCCESS == ret) {
		printf("\tOpen\n");

		// manufacturer
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0)
				printf("\t\tManufacturer: %s\n", string);
		}

		// product
		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				printf("\t\tProduct: %s\n", string);
		}
		
	
		libusb_close(handle);
	}

		// configurations
		for (int i = 0; i < desc.bNumConfigurations; i++) {
			libusb_config_descriptor *config;
			ret = libusb_get_config_descriptor(dev, i, &config);
			if (LIBUSB_SUCCESS == ret) {
				printf("\tConfiguration[%d]\n", i);
				printf("\t\tTotalLength:         %d\n", config->wTotalLength);
				printf("\t\tNumInterfaces:       %d\n", config->bNumInterfaces);
				printf("\t\tConfigurationValue:  %d\n", config->bConfigurationValue);
				printf("\t\tConfiguration:       %d\n", config->iConfiguration);
				printf("\t\tAttributes:          %02xh\n", config->bmAttributes);
				printf("\t\tMaxPower:            %d\n", config->MaxPower);

			}
			
			// interfaces
			for (int j = 0; j < config->bNumInterfaces; j++) {
				libusb_interface const & interface = config->interface[j];
				
				// alternate settings
				for (int k = 0; k < interface.num_altsetting; k++) {
					libusb_interface_descriptor const & descriptor = interface.altsetting[k];

					printf("\t\tInterface[%d][%d]\n", j, k);
					//printf("\t\t\tInterfaceNumber:   %d\n", descriptor.bInterfaceNumber);
					//printf("\t\t\tAlternateSetting:  %d\n", descriptor.bAlternateSetting);
					printf("\t\t\tNumEndpoints:      %d\n", descriptor.bNumEndpoints);
					printf("\t\t\tInterfaceClass:    %d\n", descriptor.bInterfaceClass);
					printf("\t\t\tInterfaceSubClass: %d\n", descriptor.bInterfaceSubClass);
					printf("\t\t\tInterfaceProtocol: %d\n", descriptor.bInterfaceProtocol);
					printf("\t\t\tInterface:         %d\n", descriptor.iInterface);

					// endpoints
					for (int l = 0; l < descriptor.bNumEndpoints; l++) {
						libusb_endpoint_descriptor const & endpoint = descriptor.endpoint[l];
						
						printf("\t\t\tEndpoint[%d]\n", l);
						printf("\t\t\t\tEndpointAddress: %02xh\n", endpoint.bEndpointAddress);
						printf("\t\t\t\tAttributes:      %02xh\n", endpoint.bmAttributes);
						printf("\t\t\t\tMaxPacketSize:   %d\n", endpoint.wMaxPacketSize);
						printf("\t\t\t\tInterval:        %d\n", endpoint.bInterval);
						printf("\t\t\t\tRefresh:         %d\n", endpoint.bRefresh);
						printf("\t\t\t\tSynchAddress:    %d\n", endpoint.bSynchAddress);
					}
				}
			
			}
			
			libusb_free_config_descriptor(config);
		}


	return 0;
}

int main(void) {
	libusb_device **devs;
	int r;
	ssize_t cnt;

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		libusb_exit(NULL);
		return (int) cnt;
	}

	// print list of devices
	print_devs(devs);

	
	for (int i = 0; devs[i]; ++i) {
		print_device(devs[i], 0);
	}

	libusb_free_device_list(devs, 1);

	libusb_exit(NULL);
	return 0;
}
