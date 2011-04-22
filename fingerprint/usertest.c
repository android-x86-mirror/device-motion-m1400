/**
 * AES2501 Device Driver
 * Userspace test program
 *
 * Compile with:
 * gcc -I. -o usertest usertest.c
 */


#include <aes2501.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>


#define BUFFER_SIZE 256


int main(int argc, char *argv[])
{
	int fd_in;
	int fd_out;
	char buffer[BUFFER_SIZE];
	size_t len;

	fd_in = -1;
	fd_out = -1;


	int data, rdata;



	fd_in = open("/dev/aes2501", O_RDONLY);
	if (fd_in == -1) {
		perror("open() -- ");
		goto err;
	}

	fd_out = open("/root/pic.pnm", O_CREAT | O_WRONLY);
	if (fd_out == -1) {
		perror("open() -- ");
		goto err;
	}



	data = 0x55555555;
	ioctl(fd_in, AES2501_IOC_TEST, data);
	//ioctl(fd_in, CASE2, &rdata);

	printf("IOCTL test: written: '%x' - received: '%x'\n", data, rdata);




	/* Write the fingerprint */
	while ((len = read(fd_in, buffer, BUFFER_SIZE)) > 0)
		write(fd_out, buffer, len);

	close(fd_in);
	close(fd_out);

	return EXIT_SUCCESS;

 err:

	if (fd_in != -1)
		close(fd_in);

	if (fd_out != -1)
		close(fd_out);

	return EXIT_FAILURE;

}
