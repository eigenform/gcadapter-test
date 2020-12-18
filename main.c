#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdint.h>

typedef uint8_t u8;

// Global handle for device
static libusb_device_handle *handle = NULL;

#define VENDOR	0x057e
#define PRODUCT 0x0337

// Get a handle to the GC adapter
int usb_init()
{
	if (libusb_init(NULL) < 0) 
	{
		printf("Couldn't initialize libusb\n");
		return -1;
	}

	handle = libusb_open_device_with_vid_pid(NULL, VENDOR, PRODUCT);
	if (libusb_kernel_driver_active(handle, 0) == 1)
	{
		printf("Kernel driver is already active\n");
		if (libusb_detach_kernel_driver(handle, 0) != 0) {
			printf("Couldn't detach kernel driver\n");
			return -1;
		}
	}

	if (libusb_claim_interface(handle, 0) != 0)
	{
		printf("Couldn't claim interface\n");
		return -1;
	}
	return 0;
}


int main() 
{
	int res = usb_init();
	if (res != 0) {
		libusb_exit(NULL);
		return -1;
	}
	libusb_exit(NULL);
}


