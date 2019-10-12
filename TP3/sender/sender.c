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
#define C_N0 0x00
#define C_N1 0x40
#define C_RR0 0x85
#define C_RR1 0x05
#define C_DISC 0x0B
#define C_REJ0 0x81
#define C_REJ1 0x01

#define MAX_TRIES 3
int timeout_count = 0;

#define ERROR -1

int packetConstructor(unsigned char** packet) {
  int input1;
  int input2;
  char c[5];
  int n_data;

  printf("~~~~~~~~~~~~~~~~~~~~~~~~~\n\n1: I (information packet) ; 2: S/U (control packet)\n");

  scanf("%d", &input1);

  switch(input1) {
    //CONTROL PACKET
    case 2:
      c[0]=FLAG; c[1]=A; c[4]=FLAG;
      printf("1:SET ; 2:DISC ; 3:UA ; 4: RR(R=0) ; 5:RR(R=1) ; 6:REJ(R=0) ; 7:REJ(R=1)\n");
      scanf("%d", &input2);
      switch(input2) {
        case 1:
          c[2]=C_SET;
          break;
        case 2:
          c[2]=C_DISC;
          break;
        case 3:
          c[2]=C_UA;
          break;
        case 4:
          c[2]=C_RR0;
          break;
        case 5:
          c[2]=C_RR1;
          break;
        case 6:
          c[2]=C_REJ0;
          break;
        case 7:
          c[2]=C_REJ1;
          break;
      }
      c[3]=c[1]^c[2];
      //free(*packet);
      *packet = (char*)malloc(5*sizeof(char));
      for(int i=0; i<sizeof(c); i++) {
        (*packet)[i]=c[i];
        //printf("%x\n", packet[i]);
      }
      return 5;
      break;
    // INFORMATION PACKET
    case 1:
      //printf("1:BCC2 error ; 2:No BCC2 error\n");
      //scanf("%d", &input2);
      printf("Type number of data bytes:");
      scanf("%d", &n_data);
      char i_packet[n_data+6];
      i_packet[0]=FLAG; i_packet[1]=A; i_packet[2]=0x00; i_packet[3]=i_packet[1]^i_packet[2]; i_packet[n_data+5]=FLAG;
      int bcc2=0;
      for(int i=0; i<n_data; i++) {
        i_packet[i+4]='A';
        bcc2^=i_packet[i+4];
      }
      i_packet[4+n_data] = bcc2;
      //free(*packet);
      *packet = (char*)malloc((n_data+6)*sizeof(char));
      for(int i=0; i<sizeof(i_packet); i++) {
        (*packet)[i]=i_packet[i];
        //printf("\nP:%x\n\n", *(packet+i));
        //printf("IP:%x\n", i_packet[i]);
      }
      return 6+n_data;
      break;
  }
}

int receiveUA(int serialPortFD) {
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
  struct sigaction oldSigAction;
  struct sigaction sigHandler;
  sigHandler.sa_handler = alarmHandler;

  if (sigaction(SIGALRM, &sigHandler, &oldSigAction) || siginterrupt(SIGALRM, 1)) {
      printf("sigaction failed\n");
      return ERROR;
  }

  int STOP = FALSE;

  while (STOP == FALSE && timeout_count<MAX_TRIES) {
    /*
    unsigned char set[5];
    set[0] = FLAG;
    set[1] = A;
    set[2] = C_SET;
    set[3] = set[1] ^ set[2];
    set[4] = FLAG;
    // set sending
    tcflush(fd, TCIOFLUSH);
    */
    unsigned char* set;
    packetConstructor(&set);
    int w = write(fd, set, 5);

    printf("SET sent ; W:%d\n", w);

    alarm(3);

    if (receiveUA(fd) == 0) {
      printf("UA received\n");
      STOP = TRUE;
      timeout_count = 0;
    }
    alarm(0);
  }

  STOP = FALSE;
  timeout_count = 0;

  while(1) {
    unsigned char* i_packet;
    int packet_size = packetConstructor(&i_packet);
    tcflush(fd, TCIOFLUSH);
    int w = write(fd, i_packet, packet_size);
    printf("I packet sent ; W:%d\n", w);
  }
/*
  while (STOP == FALSE && timeout_count<MAX_TRIES) {
    unsigned char bcc2;
    unsigned char inf[6+20]; //6 das merdas de controlo e 20 a's para encher chouriço
    inf[0] = FLAG;
    inf[1] = A;
    inf[2] = 0x00; //para já. depois altera-se consoante o protocolo (0 ou 1)
    inf[3] = inf[1] ^ inf[2];
    for(int i=4; i<25; i++) { //enche D4-D24 com lixo para teste
      inf[i]='a';
      if(i==4) bcc2 = inf[i];
      else bcc2 = bcc2^inf[i];
    }
    inf[25] = bcc2;
    inf[26] = FLAG;

    // I packet sending
    tcflush(fd, TCIOFLUSH);
    int w = write(fd, inf, 5);

    printf("I packet sent ; W:%d\n", w); 

    STOP = TRUE;
  }*/
/*
  for (int i = 0; i < 25; i++) {
    buf[i] = 'a';
  }
*/
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

  sigaction(SIGALRM, &oldSigAction, NULL);

  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
