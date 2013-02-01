#include <stdio.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>

#define READ_FILE "/var/www/web-sockets/rfid-read.txt"
#define WRITE_FILE "/var/www/web-sockets/rfid-write.txt"
#define CONFIRM_FILE "/var/www/web-sockets/rfid-confirm.txt"

// Takes the string name of the serial port (e.g. "/dev/tty.usbserial","COM1")
// and a baud rate (bps) and connects to that port at that speed and 8N1.
// opens the port in fully raw mode so you can send binary data.
// returns valid fd, or -1 on error.
int serialport_init(const char* serialport, int baud) {
	struct termios toptions;
	int fd;
	
	fd = open(serialport, O_RDWR | O_NOCTTY);
	if (fd == -1)  {
	  perror("init_serialport: Unable to open port ");
	  return -1;
	}
	if (tcgetattr(fd, &toptions) < 0) {
	  perror("init_serialport: Couldn't get term attributes");
	  return -1;
	}
	speed_t brate = baud;
	switch(baud) {
	case 4800:   brate=B4800;   break;
	case 9600:   brate=B9600;   break;
	case 19200:  brate=B19200;  break;
	case 38400:  brate=B38400;  break;
	case 57600:  brate=B57600;  break;
	case 115200: brate=B115200; break;
	}
	cfsetispeed(&toptions, brate);
	cfsetospeed(&toptions, brate);

	// 8N1
	toptions.c_cflag &= ~PARENB;
	toptions.c_cflag &= ~CSTOPB;
	toptions.c_cflag &= ~CSIZE;
	toptions.c_cflag |= CS8;
	// no flow control
	toptions.c_cflag &= ~CRTSCTS;
	toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
	toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
	toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
	toptions.c_oflag &= ~OPOST; // make raw
	toptions.c_cc[VMIN]  = 0;
	toptions.c_cc[VTIME] = .1;

	if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
	  perror("init_serialport: Couldn't set term attributes");
	  return -1;
	}
	return fd;
}

// Function to return "password" from an RFID tag in the form of a 4 byte unsigned char array
unsigned char *get_password(int rfid_file) {
	unsigned int sequence, chars_left = 0;
	unsigned int password_place = 0;
	unsigned int password_next = 0;
	unsigned char last = 0;
	unsigned char buffer[1];
	static unsigned char password[4];
	time_t start, now;
	
	// Array of serial protocol
	unsigned char serial_length[3] = {10, 12, 14};
	unsigned char serial_array[3][10] = {
			{170, 187, 6, 0, 0, 0, 1, 1, 7, 7},
			{170, 187, 6, 0, 0, 0, 1, 2, 82, 81},
			{170, 187, 6, 0, 0, 0, 2, 2, 4, 4}};

	// Loop through protocol
	for(sequence = 0; sequence < 3; sequence++) {

		// Send bytes
		write(rfid_file, serial_array[sequence], 10);
		
		// Read bytes
		chars_left = serial_length[sequence];
		start = time(NULL);
		while(chars_left > 0) {

			// Check for read failure
			now = time(NULL);
			if(now - start > 1) {
				return 0;
			}

			// Read incoming character
			if(read(rfid_file, buffer, 1) == 1) {
				chars_left -= 1;

				// Read password
				if(password_next == 1) {
					password[password_place] = buffer[0];
					password_place++;
				}

				// Detect that password is coming next
				if((sequence == 2) && (buffer[0] == 0) && (last == 2)) {
					password_next = 1;
				}
				last = buffer[0];
				buffer[0] = 0;
			}
		}
	}
	return password;
}

// Function to confirm a 4 byte unsigned char "password" for an RFID tag
unsigned int confirm_password(int rfid_file, unsigned char *password) {
	unsigned int chars_left;
	unsigned char checksum;
	unsigned char buffer[1];
	time_t start, now;

	// Calculate password checksum
	checksum = (1^password[0]^password[1]^password[2]^password[3]);
	
	// Array of serial protocol, first element is characters expected to return
	unsigned char serial_array[13] = {170, 187, 9, 0, 0, 0, 3, 2, password[0], password[1], password[2], password[3], checksum};

	// Send bytes
	write(rfid_file, serial_array, 13);

	// Read bytes
	chars_left = 11;
	start = time(NULL);
	while(chars_left > 0) {

		// Check for read failure
		now = time(NULL);
		if(now - start > 1) {
			printf("Failed to confirm password!\n");
			return 0;
		}

		// Read incoming character
		if(read(rfid_file, buffer, 1) == 1) {
			chars_left -= 1;
			buffer[0] = 0;
		}
	}
	return 1;
}

// Function to return the contents of RFID tag block (block_number) in a pointer to a 16 byte unsigned char array
unsigned char *get_block(int rfid_file, unsigned int block_number) {
	unsigned int sequence, chars_left;
	unsigned char checksum_1, checksum_2;
	unsigned char buffer[1];
	static unsigned char block_data[16];
	unsigned char last = 0;
	unsigned int block_place = 0;
	unsigned int block_next = 0;
	time_t start, now;

	// Calculate password checksum
	checksum_1 = (101^block_number^255^255^255^255^255^255);
	checksum_2 = (10^block_number);
	
	// Array of serial protocol
	unsigned char serial_length[2] = {17, 10};
	unsigned char serial_return[2] = {10, 25};
	unsigned char serial_array[2][17] = {{170, 187, 13, 0, 0, 0, 7, 2, 96, block_number, 255, 255, 255, 255, 255, 255, checksum_1},
						{170, 187, 6, 0, 0, 0, 8, 2, block_number, checksum_2}};
	
	// Loop through protocol
	for(sequence = 0; sequence < 2; sequence++) {

		// Send bytes
		write(rfid_file, serial_array[sequence], serial_length[sequence]);
		
		// Read bytes
		chars_left = serial_return[sequence];
		start = time(NULL);
		while(chars_left > 0) {

			// Check for read failure
			now = time(NULL);
			if(now - start > 1) {
				printf("Failed to read block data!\n");
				return 0;
			}

			// Read incoming character
			if(read(rfid_file, buffer, 1) == 1) {
				chars_left -= 1;

				// Read data
				if(block_next == 1) {
					block_data[block_place] = buffer[0];
					block_place++;
				}

				// Detect that block is coming next
				if((sequence == 1) && (buffer[0] == 0) && (last == 2)) {
					block_next = 1;
				}
				last = buffer[0];
				buffer[0] = 0;
			}
		}
	}
	return block_data;
}

// Function to write the contents of a 16 byte unsigned char array into an RFID block (block_number)
unsigned int write_block(int rfid_file, unsigned int block_number, unsigned char *write_data) {
	unsigned int chars_left, sequence;
	unsigned char checksum;
	unsigned char buffer[1];
	time_t start, now;

	// Calculate password checksum
	checksum = (15^write_data[0]^write_data[1]^write_data[2]^write_data[3]^write_data[4]^write_data[5]^write_data[6]^write_data[7]^write_data[8]^write_data[9]^write_data[10]^write_data[11]^write_data[12]^write_data[13]^write_data[14]^write_data[15]);
	
	// Array of serial protocol
	unsigned char serial_length[2] = {17, 26};
	unsigned char serial_return[2] = {11, 10};
	unsigned char serial_array[2][26] = {{170, 187, 13, 0, 0, 0, 7, 2, 96, 4, 255, 255, 255, 255, 255, 255, 97},
				{170, 187, 22, 0, 0, 0, 9, 2, 4, write_data[0], write_data[1], write_data[2], write_data[3], write_data[4], write_data[5], write_data[6], write_data[7], write_data[8], write_data[9], write_data[10], write_data[11], write_data[12], write_data[13], write_data[14], write_data[15], checksum}};

	// Loop through protocol
	for(sequence = 0; sequence < 2; sequence++) {

		// Send bytes
		write(rfid_file, serial_array[sequence], serial_length[sequence]);
		
		// Read bytes
		chars_left = serial_return[sequence];
		start = time(NULL);
		while(chars_left > 0) {

			// Check for read failure
			now = time(NULL);
			if(now - start > 1) {
				printf("Failed to write block data!\n");
				return 0;
			}

			// Read incoming character
			if(read(rfid_file, buffer, 1) == 1) {
				chars_left -= 1;
				buffer[0] = 0;
			}
		}
	}
	return 1;
}

// Function to convert the 4 bytes of a unsigned char array (starting at index offset) into an unsigned integer.
unsigned int convert_from_hex(unsigned char *data, unsigned int offset) {
	unsigned int convert = 0;
	unsigned int temp = 0;
	
	// First byte.
	convert = data[offset];
	convert = convert << 24;
	temp = convert;
	
	// Second byte.
	convert = data[offset + 1];
	convert = convert << 16;
	temp = temp | convert;
	
	// Third byte.
	convert = data[offset + 2];
	convert = convert << 8;
	temp = temp | convert;
	
	// Fourth byte.
	convert = data[offset + 3];
	return (convert | temp);
}

// Functiton to convert an unsigned integer into a 16 byte unsigned char array (starting at index $offset).
unsigned char *convert_to_hex(unsigned int value, unsigned int offset) {
	unsigned int temp = 0;
	static unsigned char data[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	data[offset + 3] = value;
	data[offset + 2] = value >> 8;
	data[offset + 1] = value >> 16;
	data[offset] = value >> 24;
	return data;
}

// Main, loop endlessly...
int main() {
	int rfid_file;
	FILE *rfid_read_file = NULL;
	FILE *rfid_write_file = NULL;
	FILE *rfid_confirm_file = NULL;
	time_t time_stamp;
	unsigned char *password_data;
	unsigned char *block_data;
	unsigned int block_id, write_block_id, write_block_number;
	unsigned int password, write_password;
	unsigned int block_number = 4;

	// Open RFID scanner file handle
	rfid_file = serialport_init("/dev/ttyUSB0", 115200);
	if(rfid_file) {
		// Wait for tags to scan
		printf("RFID reader+writer output: \n");
		while(1) {
			// Authenticate with tag
			password_data = get_password(rfid_file);
			if(password_data && confirm_password(rfid_file, password_data)) {
				// Get block data
				block_data = get_block(rfid_file, block_number);
				if(block_data) {
					// Convert block data to unsigned int
					block_id = convert_from_hex(block_data, 12);
					password = convert_from_hex(password_data, 0);
					printf("(%u) block#%u: %u.\n", password, block_number, block_id);
					
					// Write block data from tag to file
					rfid_read_file = fopen(READ_FILE, "w");
					if(rfid_read_file == NULL) {
						printf("Error opening RFID READ_FILE file!\n");
						return 0;
					}
					else {
						// time,password,block_number,block_id
						fprintf(rfid_read_file, "%u,%u,%u,%u\n", time(NULL), password, block_number, block_id);
						fclose(rfid_read_file);
					}
				}
				else {
					printf("Failed to read block data!\n");
				}
				// Write pending block data from file to tag
				rfid_write_file = fopen(WRITE_FILE, "r");
				if(rfid_write_file == NULL) {
					printf("Error opening RFID WRITE_FILE file!\n");
					return 0;
				}
				else {
					// Read write order from file
					fscanf(rfid_write_file, "%u,%u,%u", &time_stamp, &write_block_number, &write_block_id);
					fclose(rfid_write_file);

					// If current tag has no ID and write order less than 10 seconds old, write ID to tag
					if(block_id == 0 && (time_stamp + 11) > time(NULL)) {
						// Convert unsigned int $block_id into hex array $block_data, write to tag
						block_data = convert_to_hex(write_block_id, 12);
						if(write_block(rfid_file, write_block_number, block_data)) {
							// Confirm tag write
							rfid_confirm_file = fopen(CONFIRM_FILE, "w");
							if(rfid_confirm_file) {
								fprintf(rfid_confirm_file, "%u,%u,%u\n", time(NULL), write_block_number, write_block_id);
								printf("WROTE (%u) block#%u: %u.\n", password, write_block_number, write_block_id);
								fclose(rfid_confirm_file);
							}
							else {
								printf("Error opening RFID CONFIRM_FILE!\n");
							}
						}
						else {
							printf("Failed to write new ID to tag!\n");
						}
					}
				}
				sleep(1);
			}
			// Flush input & output buffers to stop the RFID scanner from freaking out
			if(tcflush(rfid_file, TCIOFLUSH) == -1) {
				printf("Error flushing RFID buffer!\n");
				close(rfid_file);
				return 0;
			}
		}
	}
	else {
		printf("Failed to open RFID file!\n");
		return 0;
	}
	return 1;
	close(rfid_file);
}
