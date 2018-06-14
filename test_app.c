/* This app for testing read data from usb */
/*+++++++++++++++++++++++++++++++ HEADER **********************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/*+++++++++++++++++++++++++++++++ DEFINE **********************/

#define BUF_SIZE 512
#define DEV_NAME 	"/dev/myusb0"
int main(int argc, char *argv[])
{
	int fd;
	char read_buf[BUF_SIZE] = {0, };
	char write_data[] = "Hello USB - Test app is run successfully -> Thanks for watching!";
	char test_data[1024];
	fd = open(DEV_NAME, O_RDWR, 0777);
	int ret; 
	if(fd < 0)
	{
		printf("Error when open device\n");
		return -1;
	}

	//Try to write to device
	//memset(test_data, 0x5A, 1024);
	if(write(fd, write_data, (ssize_t)(sizeof(write_data))) < 0)
	{
		printf("Error when write data to usb\n");
		return -1;
	}

	//Try to read from USB
	ret = read(fd, read_buf, BUF_SIZE);
	if(ret < 0)
	{
		printf("Error when read from USB\n");
		perror("error");
		return -1;
	}
	else
	{
		printf("Read data back: %s\n", read_buf);
	}

	close(fd);
	return 0;
}