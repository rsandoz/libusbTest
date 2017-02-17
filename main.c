#include <stdio.h>
#include "lusb0_usb.h"

//#pragma comment(lib, "libusb32.lib")
#pragma comment(lib, "libusb64.lib")

#define DELAY 5
#define REPORT_SIZE 0x5A

unsigned char* arr[6] = {
 "\xFF\x00\x00\x00\xFF\x00\x00\x00\xFF\x00\xFF\xFF\xFF\x00\xFF\xFF\xFF\x00"
,"\xFF\xFF\x00\xFF\x00\x00\x00\xFF\x00\x00\x00\xFF\x00\xFF\xFF\xFF\x00\xFF"
,"\xFF\x00\xFF\xFF\xFF\x00\xFF\x00\x00\x00\xFF\x00\x00\x00\xFF\x00\xFF\xFF"
,"\x00\xFF\xFF\xFF\x00\xFF\xFF\xFF\x00\xFF\x00\x00\x00\xFF\x00\x00\x00\xFF"
,"\x00\x00\xFF\x00\xFF\xFF\xFF\x00\xFF\xFF\xFF\x00\xFF\x00\x00\x00\xFF\x00"
,"\x00\xFF\x00\x00\x00\xFF\x00\xFF\xFF\xFF\x00\xFF\xFF\xFF\x00\xFF\x00\x00"
};

struct usb_dev_handle* open(unsigned int uiVID, unsigned int uiPID, unsigned int uiMI) {
	for (struct usb_bus* bus = usb_get_busses(); bus; bus = bus->next) {
		for (struct usb_device* dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == uiVID && dev->descriptor.idProduct == uiPID) {
				struct usb_dev_handle* udev;
				if (udev = usb_open(dev)) {
					struct usb_config_descriptor* config_descriptor;
					if (dev->descriptor.bNumConfigurations && (config_descriptor = &dev->config[0])) {
						for (int intfIndex = 0; intfIndex < config_descriptor->bNumInterfaces; intfIndex++) {
							if (config_descriptor->interface[intfIndex].num_altsetting) {
								struct usb_interface_descriptor* intf = &config_descriptor->interface[intfIndex].altsetting[0];
								if (intf->bInterfaceNumber == 0 && intf->bAlternateSetting == 0) {
									printf("Device %04X:%04X opened!\n", uiVID, uiPID);
									return udev;
								} else {
									usb_close(udev);
									printf("device %04X:%04X NOT opened!\n", uiVID, uiPID);
								}
							} else {
								usb_close(udev);
								printf("device %04X:%04X NOT opened!\n", uiVID, uiPID);
							}
						}
					} else {
						usb_close(udev);
						printf("device %04X:%04X NOT opened!\n", uiVID, uiPID);
					}
				}
			}
		}
	}
}

void command(struct usb_dev_handle* udev, unsigned char* request) {
	request[88] = 0; // crc
	for(unsigned char i = 2; i < 88; i++)
		request[88] ^= request[i]; // crc
	int len = usb_control_msg(udev
			, USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_ENDPOINT_OUT
			,0x09  // request - HID_REQ_SET_REPORT
			,0x300 // value
			,0x00  // index
			,request   // data
			,REPORT_SIZE
			,5000); // USB timeout
	Sleep(DELAY);
	if(len == REPORT_SIZE) {
		char* response = malloc(REPORT_SIZE);
		len = usb_control_msg(udev
				, USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_ENDPOINT_IN
				,0x01  // request HID_REQ_GET_REPORT
				,0x300 // value
				,0x00  // response_index
				,response   // data
				,REPORT_SIZE
				,5000); // USB timeout
		Sleep(DELAY);
		if(len == REPORT_SIZE) {
			if(response[2] != request[2] || response[5] != request[5] || response[6] != request[6])
				printf("Response doesnt match request\n");
			else
				switch (response[0]) {
				case 0x01:
					printf("Device is busy\n");
					break;
				case 0x02: // success
					break;
				case 0x03:
					printf("Command failed\n");
					break;
				case 0x04:
					printf("Command timed out\n");
					break;
				case 0x05:
					printf("Command not supported\n");
					break;
				default:
					printf("Unknown error response %02x\n", response[0]);
					break;
				}
		} else
			printf("Response failed - usb_control_msg. length: %d\n", len);
		free(response);
	} else
		printf("Request failed - usb_control_msg. length: %d\n", len);
}

void command_frame_kbd(struct usb_dev_handle* udev, unsigned char offset) {
	unsigned char* request = malloc(REPORT_SIZE);
	for (unsigned char row = 0; row < 6; row++) {
		memset(request, 0, REPORT_SIZE);
		memcpy(&request[0],"\x00\xFF\x00\x00\x00\x46\x03\x0B\xFF\x00\x01\x18",12);
		request[9] = row;
		for (int i = 0; i < 4; i++)
			memcpy(&request[18*i+12], arr[offset % _countof(arr)], 18);
		command(udev, request);
	}
	free(request);
}

void command_frame_ff(struct usb_dev_handle* udev, unsigned char offset) {
	unsigned char* request = malloc(REPORT_SIZE);
	memset(request, 0, REPORT_SIZE);
	memcpy(&request[0],"\x00\xFF\x00\x00\x00\x32\x03\x0C\x01\x0E",10);
	for (int i = 0; i < 3; i++)
		memcpy(&request[18*i+12], arr[offset % _countof(arr)], 18);
	command(udev, request);
	free(request);
}

void command_custom(struct usb_dev_handle* udev) {
	unsigned char * request = malloc(REPORT_SIZE);
	memset(request, 0, REPORT_SIZE);
	memcpy(&request[0],"\x00\xFF\x00\x00\x00\x02\x03\x0A\x05\x01",9);
	command(udev, request);
	free(request);
}

void test_winusb(unsigned int uiVID, unsigned int uiPID, unsigned int uiMI) {
	struct usb_dev_handle* udev = open(uiVID, uiPID, uiMI);
	for (unsigned char offset = 0; offset < 8; offset++) {
		switch (uiPID) {
		case 0x0C00:
			command_frame_ff(udev,offset);
			break;
		case 0x0210:
			command_frame_kbd(udev,offset);
			break;
		}
		command_custom(udev);
	}
	Sleep(DELAY*100);
	for (unsigned char offset = 0; offset < 8; offset++) {
		switch (uiPID) {
		case 0x0C00:
			command_frame_ff(udev,offset);
			break;
		case 0x0210:
			command_frame_kbd(udev,offset);
			break;
		}
		command_custom(udev);
	}
	Sleep(DELAY*100);
	usb_close(udev);
}

int main(int argc, char **argv) {
	printf("Press enter to continue...");
	getc(stdin);
	printf("\n");

	usb_init();
	usb_find_busses();
	usb_find_devices();

    test_winusb(0x1532, 0x0210, 0);//pro2016 kbd
    //test_winusb(0x1532, 0x0C00, 0);//firefly
    //test_winusb(0x1532, 0x005C, 0);//death adder elite
    //test_winusb(0x1532, 0x0114, 0);//death stalker kbd

	printf("Press enter to continue...");
	getc(stdin);
	printf("\n");

	return 0;
}
