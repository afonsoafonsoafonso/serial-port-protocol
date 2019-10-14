/*Non-Canonical Input Processing*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_N0 0x00
#define C_N1 0x40
#define C_RR0 0x85
#define C_RR1 0x05
#define C_DISC 0x0B
#define C_REJ0 0x81
#define C_REJ1 0x01

struct header {
  char address;
  char control;
};

int readHeader(int fd, struct header *header) {
  enum state { START, FLAG_RCV, A_RCV, C_RCV };
  enum state curr = START;

  char c;
  int nr;
  char check = 0;

  int STOP = FALSE;
  while (!STOP) {
    nr = read(fd, &c, 1);
    printf("nc = %d, %x\n", nr, (int)c);

    switch (curr) {
    case START:
      if (c == FLAG) {
        curr = FLAG_RCV;
      }
      break;
    case FLAG_RCV:
      if (c == A) {
        curr = A_RCV;
        check ^= c;
        header->address = c;
      } else if (c == FLAG) {
      } else {
        curr = START;
      }
      break;
    case A_RCV:
      if (c == C_DISC || c == C_N0 || c == C_N1) {
        check ^= c;
        header->control = c;
        printf("C: %x\n", c);
        curr=C_RCV;
      } else {
        curr = START;
      }
      break;
    case C_RCV:
      if (check == c) {
        STOP = TRUE;
      } else {
        return -1;
      }
    }
  }

  return 0;
}

void awaitControl(int serialPortFD, unsigned char control) {
  tcflush(serialPortFD, TCIOFLUSH);
  char c;
  int nr;

  enum state { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK };
  enum state curr = START;

  char check = 0;

  int STOP = FALSE;
  while (STOP == FALSE) {
    nr = read(serialPortFD, &c, 1);
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
      if (c == control) {
        puts("Received C byte");
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
        puts("CONTROL END");
        STOP = TRUE;
      } else {
        curr = START;
      }
      break;
    }
  }
  puts("Exiting.");
}

void sendControl(int serialPortFD, unsigned char control) {
  char answer[] = {FLAG, A, control, A ^ control, FLAG};
  for (int i = 0; i < 5;) {
    int res = write(serialPortFD, answer + i, 5 - i);
    i += res;
    printf("%d bytes resent.\n", res);
  }
}

int main(int argc, char **argv) {
  int fd;
  struct termios oldtio, newtio;

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror("/dev/ttyS2");
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
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  awaitControl(fd, C_SET);

  tcflush(fd, TCIOFLUSH);

  sendControl(fd, C_UA);

  while (1) {
    //printf("WHIIIIILLEEEE UNOOOOO\n");
    struct header header;

    char waitingFor = C_N0;
    //printf("Stuck at readHeader?\n");
    if (readHeader(fd, &header)) {
      sendControl(fd, C_REJ0);
      continue;
    }
    //printf("Stuck at sendControl(waitingFor)?\n");
    if (header.control != waitingFor) {
      if (header.control == C_DISC) {
        tcflush(fd, TCIOFLUSH);
        puts("C_DISC received, disconnecting.");
        sendControl(fd, C_DISC);
        awaitControl(fd, C_UA);
        break;
      }
      tcflush(fd, TCIOFLUSH);
      sendControl(fd, waitingFor);
      continue;
    }

    char c;
    char prev = 0;
    char buf[512];
    char check = 0;
    int i = 0;

    int nr;
    printf("\n---------------------------\n");
    while (i < 512) {
      nr = read(fd, &c, 1);
      printf("Received: %x\n", c);

      if (c == FLAG) {
        break;
      }
      
      buf[i] = c;
      printf("BUF: %x ; CHECK: %x\n", buf[i], check^c);
      i++;
      check ^= prev;
      prev = c;
    }
    printf("---------------------------\n");

    printf("RECEIVED %d BYTES\n", i-1);
    printf("CHECK BYTE: %x\n", check);
    printf("CHECK BYTE ON BUF: %x\n", buf[i-1]);

    if (buf[i-1] != check) {
      if (waitingFor == C_N0) {
        sendControl(fd, C_REJ0);
      } else if (waitingFor == C_N1) {
        sendControl(fd, C_REJ1);
      }
      continue;
    }

    printf("Header.control: %x\n", header.control);

    if (header.control == C_N0) {
      printf("RCV: SENDING C_RR1\n");
      sendControl(fd, C_RR1);
      waitingFor = C_N1;
    } else if (header.control == C_N1) {
      printf("RCV: SENDING C_RR0\n");
      sendControl(fd, C_RR0);
      waitingFor = C_N0;
    } 

    buf[i-1] = 0;

    printf("%s\n\n",buf);
  }

  //TODO receiver FLAG escape

  printf("\n");

  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);
  return 0;
}
