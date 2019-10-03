/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07

#define MERDOU -1

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    int fd, res;
    struct termios oldtio,newtio;
    char buf[255];
    int sum = 0, speed = 0;

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */


    fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
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

    //set masking
    unsigned char set[5];
    set[0]=FLAG;
    set[1]=A;
    set[2]=C_SET;
    set[3]=set[1]^set[2];
    set[4]=FLAG;
    //set sending
    write(fd, set, 5);
    //UA
    unsigned char ua[5];
    //for(int i=0; i<5; i++) {
        printf("waiting for ua1\n");
        read(fd, &ua, 5);
        printf("waiting for ua2\n");
    //}
    //UA check
    if(ua[0]!=FLAG || ua[1]!=A || ua[2]!=C_UA || ua[3]!=(A^C_UA) || ua[4]!=FLAG) {
      printf("FLAG:%x \n A:%x \n UA:%x \n BCC:%x \n FLAG:%x \n", ua[0], ua[1], ua[2], ua[3], ua[4]);
      printf("ESTA UA BYTE ESTÁ OH, ASSIM, UMA BALENTE MERDA\n");
      return MERDOU;
    }

    for (int i = 0; i < 25; i++) {
      buf[i] = 'a';
    }

    /*testing*/
    buf[25] = '\0';

    res = write(fd,buf,26);
    printf("%d bytes written\n", res);

  /*
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar
    o indicado no gui�o
  */

    int i = 0;
    char c;

    while(1) {
      read(fd, &c, 1);
      buf[i] = c;
      i++;
      if (c == '\0') {
        break;
      }
    }

    printf("%s\n", buf);

    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(fd);
    return 0;
}
