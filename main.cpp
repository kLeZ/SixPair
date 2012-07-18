#include<iostream>
#include<iomanip>
#include<cstdio>
#include<cstdlib>
#include<libusb-1.0/libusb.h>

const int VENDOR = 0x054c;
const int PRODUCT = 0x0268;
const std::string HCI_BT_ADDR_FMT = "%*s\n%*s %x:%x:%x:%x:%x:%x";
const std::string BT_ADDR_FMT = "%x:%x:%x:%x:%x:%x";

void printdev( libusb_device_descriptor* desc, libusb_config_descriptor* config, const libusb_interface* iface, const libusb_interface_descriptor* ifacedesc, const libusb_endpoint_descriptor* epdesc );
void printbtaddr( unsigned char btaddr[6] );
void fatal ( const char* msg );
void show_master ( libusb_device_handle *devh, int itfnum );
void set_master ( libusb_device_handle *devh, int itfnum, unsigned char btaddr[6] );
void get_internalbtaddr( unsigned char btaddr[6] );
std::string get_usage(char **argv);

int main (int argc, char **argv)
{
	libusb_device **devs; // List of usb devices
	libusb_context *ctx = NULL; // USB session
	int res = 0;
	res = libusb_init(&ctx); // Initialize the USB session
	if (res < 0) fatal("Error in context initialization: " + res);
	else
	{
		bool found = false;
		ssize_t ndevs; // Number of USB devices
		ssize_t currentdev;
		libusb_set_debug(ctx, 3); // Set verbosity level to 3
		ndevs = libusb_get_device_list(ctx, &devs); // Get the device list
		if (ndevs < 0) std::cerr << "Error in retrieving devices list" << std::endl;
		else std::cout << ndevs << " devices in list" << std::endl;
		for (currentdev = 0; currentdev < ndevs; currentdev++)
		{
			libusb_device_descriptor desc;
			int res = libusb_get_device_descriptor(devs[currentdev], &desc);

			std::ios_base::fmtflags original_flags = std::cout.flags();
			std::cout <<
			std::setfill('0') << std::setw(4) <<
			std::uppercase << std::hex << (int)desc.idVendor <<
			":" <<
			std::setfill('0') << std::setw(4) <<
			std::uppercase << std::hex << (int)desc.idProduct <<
			std::endl;
			std::cout.flags(original_flags);

			if (res < 0) std::cerr << "Error in retrieving device descriptor" << std::endl;
			else
			{
				libusb_config_descriptor *config;
				libusb_get_config_descriptor(devs[currentdev], 0, &config);

				const libusb_interface *iface;
				const libusb_interface_descriptor *ifacedesc;
				const libusb_endpoint_descriptor *epdesc;

				for (int i = 0; i < config->bNumInterfaces; i++)
				{
					iface = &config->interface[i];

					for (int j = 0; j < iface->num_altsetting; j++)
					{
						ifacedesc = &iface->altsetting[j];

						for (int k = 0; k < ifacedesc->bNumEndpoints; k++)
						{
							epdesc = &ifacedesc->endpoint[k];

							if (desc.idVendor == VENDOR && desc.idProduct == PRODUCT && ifacedesc->bInterfaceClass == 3)
							{
								printdev(&desc, config, iface, ifacedesc, epdesc);

								libusb_device_handle* handle;
								handle = libusb_open_device_with_vid_pid(ctx, VENDOR, PRODUCT);

							#ifdef __linux__
								libusb_detach_kernel_driver(handle, ifacedesc->bInterfaceNumber);
							#endif

								libusb_claim_interface(handle, ifacedesc->bInterfaceNumber);

								show_master(handle, ifacedesc->bInterfaceNumber);

								unsigned char btaddr[6];
								if (argc >= 2)
								{
									res = sscanf(argv[1], BT_ADDR_FMT.c_str(), &btaddr[0], &btaddr[1], &btaddr[2], &btaddr[3], &btaddr[4], &btaddr[5]);
									if (res != 6) fatal(get_usage(argv).c_str());
								}
								else get_internalbtaddr(btaddr);

								set_master(handle, ifacedesc->bInterfaceNumber, btaddr);

								std::cout << "Master BD_ADDR set to ";
								printbtaddr(btaddr);
								libusb_close(handle);
								std::cout << "Device closed, attempting to flush all busses..." << std::endl;
								res = 0;
								found = true;
							}
						}
					}
				}
				if (!found) std::cout << "Nothing found at this address..." << std::endl;
				std::cout << "Freeing device config descriptor..." << std::endl;
				libusb_free_config_descriptor(config);
				std::cout << "Device config descriptor free." << std::endl;
			}
		}
		if (!found) std::cout << "No controller found on USB busses" << std::endl;
// 		std::cout << "Freeing device list..." << std::endl;
// 		libusb_free_device_list(devs, 1);
// 		std::cout << "Device list free." << std::endl;
		std::cout << "Exiting..." << std::endl;
		libusb_exit(ctx);
	}
	std::cout << "Free of resources completed, now exiting gracefully... w/ code: " << res << std::endl;
	return res;
}

void printdev(libusb_device_descriptor* desc, libusb_config_descriptor* config, const libusb_interface* iface, const libusb_interface_descriptor* ifacedesc, const libusb_endpoint_descriptor* epdesc)
{
	std::cout << "Number of possible configurations: " << (int)desc->bNumConfigurations << std::endl;
	std::cout << "Device class: " << (int)desc->bDeviceClass << std::endl;
	std::cout << "Vendor ID: " << desc->idVendor << std::endl;
	std::cout << "Product ID: " << desc->idProduct << std::endl;
	std::cout << "Interfaces: " << (int)config->bNumInterfaces << std::endl;
	std::cout << "Number of alternate settings: " << iface->num_altsetting << std::endl;
	std::cout << "Interface number: " << (int)ifacedesc->bInterfaceNumber << std::endl;
	std::cout << "Interface endpoints: " << (int)ifacedesc->bNumEndpoints << std::endl;
	std::cout << "Descriptor Type: " << (int)epdesc->bDescriptorType << std::endl;
	std::cout << "EP Address: " << (int)epdesc->bEndpointAddress << std::endl;
}

void fatal(const char *msg)
{
	std::cerr << msg << std::endl;
	std::exit(1);
}

void show_master ( libusb_device_handle *devh, int itfnum )
{
	std::cout << "Current Bluetooth master: ";

	unsigned char msg[8];
	int res = 0;
	res = libusb_control_transfer ( devh, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 0x01, 0x03f5, itfnum, msg, sizeof ( msg ), 5000 );
	if ( res < 0 ) std::cerr << "libusb_control_transfer" << std::endl;
	else
	{
		unsigned char btaddr[6];
		for (int i = 2; i < sizeof(msg); i++) btaddr[i] = msg[i];
		printbtaddr(btaddr);
	}
}

void printbtaddr( unsigned char btaddr[6] )
{
	std::ios_base::fmtflags original_flags = std::cout.flags();
	std::cout
	<< std::setw(2) << std::uppercase << std::hex << (int)btaddr[0] << ":"
	<< std::setw(2) << std::uppercase << std::hex << (int)btaddr[1] << ":"
	<< std::setw(2) << std::uppercase << std::hex << (int)btaddr[2] << ":"
	<< std::setw(2) << std::uppercase << std::hex << (int)btaddr[3] << ":"
	<< std::setw(2) << std::uppercase << std::hex << (int)btaddr[4] << ":"
	<< std::setw(2) << std::uppercase << std::hex << (int)btaddr[5] << std::endl;
	std::cout.flags(original_flags);
}

void get_internalbtaddr ( unsigned char btaddr[6] )
{
	FILE *f = popen("hcitool dev", "r");
	if (!f || fscanf(f, HCI_BT_ADDR_FMT.c_str(), &btaddr[0], &btaddr[1], &btaddr[2], &btaddr[3], &btaddr[4], &btaddr[5]) != 6)
	{
		fatal("Unable to retrieve local bd_addr from `hcitool dev`.\nPlease enable Bluetooth or specify an address manually.");
	}
	pclose(f);
}

std::string get_usage(char **argv)
{
	std::string usage;
	usage.append("Usage:\n").append(argv[0]).append(" [<bd_addr of master>]");
	return usage;
}

void set_master ( libusb_device_handle* devh, int itfnum, unsigned char btaddr[6] )
{
	std::cout << "Setting master bd_addr to ";
	printbtaddr(btaddr);

	unsigned char msg[8] = { 0x01, 0x00, btaddr[0], btaddr[1], btaddr[2], btaddr[3], btaddr[4], btaddr[5] };
	int res = 0;
	res = libusb_control_transfer ( devh, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 0x09, 0x03f5, itfnum, msg, sizeof ( msg ), 5000 );
	if ( res < 0 ) fatal("libusb_control_transfer");
}


// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;


