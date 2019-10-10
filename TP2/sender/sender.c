/*Non-Canonical Input Processing*/

#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>


#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07

#define MAX_TRIES 3

#define ERROR -1

int timeout_count = 0;

int receiveUA(int serialPortFD) {
  tcflush(serialPortFD, TCIOFLUSH);
  char c;
  int nr;

  enum state { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK };
  enum state curr = START;

  char check = 0;

  int STOP = FALSE;
  while (STOP == FALSE) {
    puts("Awaiting byte");
    nr = read(serialPortFD, &c, 1);
    if (nr < 0) {
      if (errno == EINTR) {
        puts("Timed out. Sending again.");
        return ERROR;
      }
      continue;
    }
    printf("nc = %d, %x\n", nr, (int)c);

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
      if (c == C_UA) {
        puts("Received C_UA");
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

  return 0;
}

void alarmHandler(int sig) {
  timeout_count++;
  puts("Time out signal.");
  if(timeout_count>=MAX_TRIES)
    printf("Exceeded maximum number of tries (%d).\n", MAX_TRIES);
  return;
}

int main(int argc, char **argv) {
  int fd, res;
  struct termios oldtio, newtio;
  char buf[255];
  int sum = 0, speed = 0;

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open("/dev/ttyS2", O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  memset(&newtio, 0, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) pr�ximo(s) caracter(es) !!!
  */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  //Alarm handler setup
  //struct sigaction oldSigAction;
  //struct sigaction sigHandler;
  //sigHandler.sa_handler = alarmHandler;

  if (signal(SIGALRM, alarmHandler) || siginterrupt(SIGALRM, 1)) {
      printf("sigaction failed\n");
      return ERROR;
  }

  int STOP = FALSE;

  while (STOP == FALSE && timeout_count<MAX_TRIES) {
    unsigned char set[5];
    set[0] = FLAG;
    set[1] = A;
    set[2] = C_SET;
    set[3] = set[1] ^ set[2];
    set[4] = FLAG;
    // set sending
    tcflush(fd, TCIOFLUSH);
    //sleep(1);
    int w = write(fd, set, 5);

    printf("SET sent ; W:%d\n", w);
    alarm(2);

    if (receiveUA(fd) == 0) {
      printf("UA received\n");
      STOP = TRUE;
      timeout_count = 0;
    }
    alarm(0);
  }

  for (int i = 0; i < 25; i++) {
    buf[i] = 'a';
  }

  /*testing*/
  /*
  buf[25] = '\0';

  res = write(fd, buf, 26);
  printf("%d bytes written\n", res);
  */
  /*
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a
    respeitar o indicado no gui�o
  */

  //printf("%s\n", buf);

  //sigaction(SIGALRM, &oldSigAction, NULL);

  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
