#include "serial.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define BAUDRATE B38400

#define ESCAPE 0x7d

#define FLAG 0x7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_N0 0x00
#define C_N1 0x40
#define C_RR0 0x05
#define C_RR1 0x85
#define C_DISC 0x0B
#define C_REJ0 0x01
#define C_REJ1 0x81

#define MAX_TRIES 3
#define TIMEOUT_THRESHOLD 3
#define BUFFER_SIZE 100

static struct termios oldtio, newtio;
static enum open_mode current_mode;
static struct sigaction oldSigAction;
static struct sigaction sigHandler;

struct header {
  unsigned char address;
  unsigned char control;
};

void alarmHandler(int sig) {
  puts("Time out signal.");
  return;
}

int awaitControl(int serialPortFD, unsigned char control) {
  char c;
  int nr;

  enum state { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK };
  enum state curr = START;

  char check = 0;

  int STOP = FALSE;
  while (STOP == FALSE) {
    nr = read(serialPortFD, &c, 1);
    if (nr < 0) {
      return errno;
    }

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
      } else if (c == FLAG) {
      } else {
        curr = START;
      }
      break;
    case A_RCV:
      if (c == control) {
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
        curr = BCC_OK;
      } else if (c == FLAG) {
        curr = FLAG_RCV;
      } else {
        curr = START;
      }
      break;
    case BCC_OK:
      if (c == FLAG) {
        STOP = TRUE;
      } else {
        curr = START;
      }
      break;
    }
  }
  printAction(0, control, 0);
  return 0;
}

int sendControl(int serialPortFD, unsigned char control) {
  tcflush(serialPortFD, TCIOFLUSH);
  unsigned char answer[] = {FLAG, A, control, A ^ control, FLAG};
  for (int i = 0; i < 5;) {
    int res = write(serialPortFD, answer + i, 5 - i);
    if (res < 0) {
      return -1;
    }
    i += res;
  }
  printAction(1, control, 0);
  return 0;
}

int readHeader(int fd, struct header *header) {
  enum state { START, FLAG_RCV, A_RCV, C_RCV };
  enum state curr = START;

  unsigned char c;
  int nr;
  unsigned char check = 0;

  int STOP = FALSE;
  while (!STOP) {
    nr = read(fd, &c, 1);
    if (nr < 0) {
      return -2;
    }

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
      if (c == C_DISC || c == C_N0 || c == C_N1 || c == C_RR0 || c == C_RR1 || c == C_REJ0 || c == C_REJ1 || C_UA) {
        check ^= c;
        header->control = c;
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

int setup_terminal(int fd) {

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
    return -1;
  }

  return 0;
}

int restore_terminal(int fd) {

  if (tcsetattr(fd, TCSANOW, &oldtio)) {
    return -1;
  }
  return 0;
}

int open_receiver(int fd) {
  awaitControl(fd, C_SET);

  if (sendControl(fd, C_UA)) {
    return -1;
  }

  return fd;
}

int open_sender(int fd) {
  int timeout_count = 0;
  int STOP = FALSE;

  while (STOP == FALSE && timeout_count < MAX_TRIES) {
    if (sendControl(fd, C_SET)) {
      return -1;
    }

    alarm(TIMEOUT_THRESHOLD);

    int res = awaitControl(fd, C_UA);
    alarm(0);
    if (res != 0) {
      puts("Timed out.");
      timeout_count++;
    } else {
      STOP = TRUE;
      timeout_count = 0;
    }    
  }

  if (timeout_count >= MAX_TRIES) {
    puts("Exceeded maximum atempts to connect.");
    return -1;
  }

  return fd;
}

int llopen(int port, enum open_mode mode) {
  if (port != COM0 && port != COM1 && port!= COM2) {
    puts("Invalid COM number.");
    return -1;
  }

  current_mode = mode;

  char format[] = "/dev/ttyS%d";
  char path[11];
  path[10] = 0;
  sprintf(path, format, port);

  int fd = open(path, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(path);
    return -1;
  }

  sigHandler.sa_handler = alarmHandler;

  if (sigaction(SIGALRM, &sigHandler, &oldSigAction) || siginterrupt(SIGALRM, 1)) {
      printf("sigaction failed\n");
      return -1;
  }


  setup_terminal(fd);

  switch (mode) {
    case SENDER:
      return open_sender(fd);
    case RECEIVER:
      return open_receiver(fd);
  }
}

int llwrite(int fd, char *buffer, unsigned int length) {
  unsigned int current_pointer = 0;
  char enumeration = C_N0;
  while(1) {
    char message[6+BUFFER_SIZE*2];
    message[0] = FLAG;
    message[1] = A;
    message[2] = enumeration;
    message[3] = message[1] ^ message[2];

    char check = 0;
    int i = 0;
    int j = 0;
    for (; i < BUFFER_SIZE && current_pointer + i < length; i++) {
      unsigned char byte = buffer[current_pointer+i];
      if (byte == FLAG || byte == ESCAPE) {
        message[4+j] = ESCAPE;
        j++;
        message[4+j] = byte ^ 0x20;
        j++;
      } else {
        message[4+j] = byte;
        j++;
      }
      check ^= byte;
    }

    message[4+j] = check;
    message[5+j] = FLAG;

    tcflush(fd, TCIOFLUSH);
    int nr = write(fd, message, 6+j);
    if (nr < 0) {
      return -1;
    }
    printAction(TRUE, 'I', nr);

    alarm(TIMEOUT_THRESHOLD);

    struct header header;
    int res = readHeader(fd, &header);
    alarm(0);
    if (res < 0) {
      continue;
    }

    unsigned char c;
    nr = read(fd, &c, 1);
    if (nr < 0) {
      puts("Error reading FLAG");
      return -1;
    }
    if (c != FLAG) {
      printf("%x Not FLAG\n", c);
      return -1;
    }

    if (enumeration == C_N0) {
      if (header.control == C_RR1) {
        printAction(0, C_RR1, 0);
        enumeration = C_N1;
        current_pointer += i;
      } else if (header.control == C_REJ0 || header.control == C_REJ1) {
        printAction(0, C_REJ0, 0);
        continue;
      } else if (header.control == C_N1) {
        enumeration = C_N1;
        current_pointer += i;
      }
    } else if (enumeration == C_N1) {
      if (header.control == C_RR0) {
        printAction(0, C_RR0, 0);
        enumeration = C_N0;
        current_pointer += i;
      } else if (header.control == C_REJ1 || header.control == C_REJ0) {
        printAction(0, C_REJ1, 0);
        continue;
      } else if (header.control == C_N0) {
        enumeration = C_N0;
        current_pointer += i;
      }
    }
    
    if (current_pointer >= length) {
      return current_pointer;
    }

  }
}

int llread(int fd, char *buffer) {
  unsigned int current_pointer = 0;
  char waitingFor = C_N0;
  while (1) {
    struct header header;

    //puts("Reading header");
    if (readHeader(fd, &header) != 0) {
      if (waitingFor == C_N0) {
        sendControl(fd, C_REJ0);
      } else if (waitingFor == C_N1) {
        sendControl(fd, C_REJ1);
      }
      
      continue;
    }
  
    if (header.control != waitingFor) {
      if (header.control == C_DISC) {
        char c;
        int nr = read(fd, &c, 1);
        if (c == FLAG) {
					printAction(0, C_DISC, 0);
          while (1) {
            sendControl(fd, C_DISC);
            alarm(TIMEOUT_THRESHOLD + 1);

            int res = readHeader(fd, &header);
            alarm(0);
            
            if (res == -2) {
              return current_pointer;
            } else if (res == -1) {
              continue;
            }
						int c;
            read(fd, &c, 1);
            if (c != FLAG) {
              continue;
            }
						printAction(0, header.control,0);
            if (header.control == C_DISC) {
              continue;
            } else if (header.control == C_UA) {
              return current_pointer;
            }
          }
        } else {
          continue;
        }
      }
      printf("got %x but was wainting for %x\n", header.control, waitingFor);
      sendControl(fd, waitingFor);
      continue;
    }

    char c;
    char buf[512];
    int i = 0;

    int nr;
    printf("\n---------------------------\n");
    int escape_mode = 0;
    while (i < 512) {
      nr = read(fd, &c, 1);
      //printf("Received: %x\n", c);

      if (c == FLAG) {
        break;
      }
      
      if (c == ESCAPE) {
        escape_mode=1;
        continue;
      } else {
        if (escape_mode) {
          c ^= 0x20;
          escape_mode=0;
        }
        buf[i] = c;
        i++;
      }
    }

    char check = 0;
    for (int j = 0; j < i-1; j++) {
      check ^= buf[j];
    }

    printAction(0, 'I', i-1);
    buf[i-1] = 0;

    if (buf[i-1] != check) {
      if (waitingFor == C_N0) {
        sendControl(fd, C_REJ0);
      } else if (waitingFor == C_N1) {
        sendControl(fd, C_REJ1);
      }
      continue;
    }


    if (header.control == C_N0) {
      printAction(0, C_N0, 0);
      sendControl(fd, C_RR1);
      waitingFor = C_N1;
    } else if (header.control == C_N1) {
      printAction(0, C_N1, 0);
      sendControl(fd, C_RR0);
      waitingFor = C_N0;
    } 


    for (int j = 0; j < i-1; j++) {
      buffer[current_pointer+j] = buf[j];
    }
    current_pointer += i-1;

  }
}

int llclose(int fd) {
  if (current_mode == SENDER) {
    while (1) {
      sendControl(fd, C_DISC);
      alarm(TIMEOUT_THRESHOLD);

      int res = awaitControl(fd, C_DISC);
      alarm(0);
      if (res == 0) {
        sendControl(fd, C_UA);
        break;
      }
    }
  }

  close(fd);
  sigaction(SIGALRM, &oldSigAction, NULL);
  return 0;
}

void printAction(int sent, unsigned char c_byte, int n_data) {
  char type[10];
  switch (c_byte) {
    case C_SET:
      strcpy(type,"C_SET");
      break;
    case C_UA:
      strcpy(type,"C_UA");
      break;
    case C_N0:
      strcpy(type,"C_N0");
      break;
    case C_N1:
      strcpy(type,"C_N1");
      break;
    case C_RR0:
      strcpy(type,"C_RR0");
      break;
    case C_RR1:
      strcpy(type,"C_RR1");
      break;
    case C_DISC:
      strcpy(type,"C_DISC");
      break;
    case C_REJ0:
      strcpy(type,"C_REJ0");
      break;
    case C_REJ1:
      strcpy(type,"C_REJ1");
      break;
    case 'I':
      if(sent==1)
        printf("Sent %d data bytes.\n", n_data);
      else
        printf("Received %d data bytes.\n", n_data);
      return;
  }
  if(sent==1) 
    printf("Sent %s byte.\n", type);
  else
    printf("Received %s byte.\n", type);

  return;
}
