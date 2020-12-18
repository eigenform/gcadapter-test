#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <time.h>

typedef uint8_t u8;

#define BUFFER_LEN 37
static u8 buffer[BUFFER_LEN] = { 0 };

static libusb_device_handle *handle = NULL;
static u8 endpoint_in = 0;
static u8 endpoint_out = 0;

#define VENDOR	0x057e
#define PRODUCT 0x0337

void hexdump(char *desc, void *addr, int len)
{
	unsigned char *pc = (unsigned char*)addr;
	unsigned char buff[17];
	int i;

	if (desc != NULL) printf ("%s:\n", desc);
	if (len == 0) 
	{
		printf("  ZERO LENGTH\n");
		return; 
	}
	if (len < 0)
	{
		printf("  NEGATIVE LENGTH: %i\n",len);
		return;
	}

	for (i = 0; i < len; i++)
	{
		if ((i % 16) == 0) 
		{
			if (i != 0) 
				printf("  %s\n", buff);
			printf("  %04x ", i); 
		}
		
		printf(" %02x", pc[i]);
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	while ((i % 16) != 0)
	{
		printf("   ");
		i++;
	}
	
	printf("  %s\n", buff);
}

struct timespec tdiff(struct timespec a, struct timespec b)
{
	struct timespec d;
	if ((b.tv_nsec - a.tv_nsec) < 0)
	{
		d.tv_sec = b.tv_sec - a.tv_sec - 1;
		d.tv_nsec = 1000000000 + b.tv_nsec - a.tv_nsec;
	}
	else
	{
		d.tv_sec = b.tv_sec - a.tv_sec;
		d.tv_nsec = b.tv_nsec - a.tv_nsec;
	}
	return d;
}



void get_endpoints(libusb_device *dev)
{
	struct libusb_config_descriptor *config = NULL;
	libusb_get_config_descriptor(dev, 0, &config);
	for (u8 ic = 0; ic < config->bNumInterfaces; ic++)
	{
		const struct libusb_interface *container = &config->interface[ic];
		for (int i = 0; i < container->num_altsetting; i++)
		{
			const struct libusb_interface_descriptor *idesc = &container->altsetting[i];
			for (u8 e = 0; e < idesc->bNumEndpoints; e++)
			{
				const struct libusb_endpoint_descriptor *endpoint = &idesc->endpoint[e];
				if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
					endpoint_in = endpoint->bEndpointAddress;
				else
					endpoint_out = endpoint->bEndpointAddress;
			}
		}
	}
}

// Get a handle to the GC adapter
int usb_init()
{
	if (libusb_init(NULL) < 0) 
	{
		printf("Couldn't initialize libusb\n");
		return -1;
	}

	ssize_t num_devs = 0;
	libusb_device **devlist;
	num_devs = libusb_get_device_list(NULL, &devlist);
	for (ssize_t idx = 0; idx < num_devs; ++idx) 
	{
		libusb_device *dev = devlist[idx];
		struct libusb_device_descriptor tmp;
		libusb_get_device_descriptor(dev, &tmp);
		if (tmp.idVendor == VENDOR && tmp.idProduct == PRODUCT) {
			get_endpoints(dev);
			break;
		}
	}
	libusb_free_device_list(devlist, num_devs);

	// Open handle to device
	handle = libusb_open_device_with_vid_pid(NULL, VENDOR, PRODUCT);
	if (libusb_kernel_driver_active(handle, 0) == 1)
	{
		printf("Kernel driver is already active\n");
		if (libusb_detach_kernel_driver(handle, 0) != 0) {
			printf("Couldn't detach kernel driver\n");
			libusb_close(handle);
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

void usb_cleanup()
{
	if (handle) 
	{
		libusb_release_interface(handle, 0);
		libusb_close(handle);
	}
}


int main() 
{
	int res = usb_init();
	if (res != 0) {
		libusb_exit(NULL);
		return -1;
	}

	u8 cmd_id = 0x13;
	libusb_interrupt_transfer(handle, 
		endpoint_out, &cmd_id, sizeof(cmd_id), &res, 32);

	for (int i = 0; i < 10; i++)
	{
		struct timespec start;
		struct timespec end;
		struct timespec diff;

		clock_gettime(CLOCK_MONOTONIC, &start);
		libusb_interrupt_transfer(handle, 
			endpoint_in, buffer, sizeof(buffer), &res, 32);
		clock_gettime(CLOCK_MONOTONIC, &end);

		//hexdump("buffer", &buffer, BUFFER_LEN);
		diff = tdiff(start, end);
		double ns = (diff.tv_sec * 1000000000) + diff.tv_nsec;
		double ms = ns / 1000000;
		printf("Took %.06lfms\n", ms);
	}
	usb_cleanup();
	libusb_exit(NULL);
}

