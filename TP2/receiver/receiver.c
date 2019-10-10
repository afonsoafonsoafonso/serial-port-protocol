/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07

volatile int STOP=FALSE;

void awaitSet(int serialPortFD) {
    char c;
    int nr;
    
    enum state {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK};
    enum state curr = START;

	char check = 0;

    while (STOP==FALSE) {
		nr = read(serialPortFD, &c, 1);
		printf("nc = %d, %x\n", nr, (int) c);
    
		switch (curr) {
			case START:
				if (c == FLAG) {
					puts("Received Flag");
					curr = FLAG_RCV;
				}
				break;
			case FLAG_RCV:
				if (c == A) {
					puts("Received A");
					curr = A_RCV;
					check ^= c;
				} else if (c == FLAG) {
				} else {
					curr = START;
				}
				break;
			case A_RCV:
				if (c == C_SET) {
					puts("Received C_SET");
					curr = C_RCV;
					check ^= c;
				} else if (c == FLAG) {
					curr = FLAG_RCV;
				} else {
					curr = START;
				}
				break;
			case C_RCV:
				if (c == check) {
					puts("BCC is ok");
					curr = BCC_OK;
				} else if (c == FLAG) {
					curr = FLAG_RCV;
				} else {
					curr = START;
				}
				break;
			case BCC_OK:
				if (c == FLAG) {
					puts("SET END");
					STOP = TRUE;
				} else {
					curr = START;
				}
				break;
		}
    }
	puts("Exiting.");
}

int main(int argc, char** argv)
{
    int fd;
    struct termios oldtio,newtio;


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY );
    if (fd <0) {perror("/dev/ttyS0"); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

	awaitSet(fd);

	sleep(1);
  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no gui�o 
  */

    tcflush(fd, TCIOFLUSH);

	char answer[] = {FLAG, A, C_UA, A^C_UA, FLAG};

	for (int i = 0; i < 5;) {
		int res = write(fd, answer + i, 5 - i);
		i += res;
		printf("%d bytes resent.\n", res);
	}
    

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
