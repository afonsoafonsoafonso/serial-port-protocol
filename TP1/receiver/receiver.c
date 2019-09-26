#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#define BAUDRATE B38400

int main() {
	int fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY);
	
	if (fd < 0) {
		perror("/dev/ttyS0");
		exit(1);
	}
	
	struct termios oldtio, newtio;
	if ( tcgetattr(fd,&oldtio) == -1) {
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   
    newtio.c_cc[VMIN]     = 1;
    
    tcflush(fd, TCIOFLUSH);
	
	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr - new");
      exit(-1);
    }

	
	char buffer[256];
	char c;
	int i = 0;
	
	while (1) {
		read(fd, &c, 1);
		buffer[i] = c;
		i++;
		if (c == '\0') {
			break;
		}
	}
	
	printf("%s/n", buffer);
	
	write(fd, buffer, i);
	
	sleep(1);
	
	if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr - old");
      exit(-1);
    }
	
	return 0;
}
