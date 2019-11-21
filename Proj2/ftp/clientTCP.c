/*      (C)2000 FEUP  */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define BUFF_SIZE 1024
#define FTP_PORT 21
#define USER_STR "user anonymous\n"
#define PASS_STR "pass pass\n"
#define PASV "pasv\n"

static sem_t semaphore;

int openSocket(const char *address, const int port) {
  struct sockaddr_in server_addr;
  int sockfd;

  /*server address handling*/
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr =
      inet_addr(address); /*32 bit Internet address network byte ordered*/
  server_addr.sin_port =
      htons(port); /*server TCP port must be network byte ordered */

  /*open an TCP socket*/
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket()");
    exit(0);
  }

  /*connect to the server*/
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect()");
    exit(0);
  }

  return sockfd;
}

void flushSocket(int sockfd) {
	char buf[100];
	int res;
	do {
		res = recv(sockfd, buf, 100, MSG_DONTWAIT);
		if (res <= 0) {
			return;
		}
		write(STDOUT_FILENO, buf, res);
	} while (res > 0);
}

int sendMessage(int sockfd, char *message, unsigned int length) {
  int nr = 0;
  size_t res;
  //puts("Writing");
	flushSocket(sockfd);
  do {
    res = send(sockfd, message, length, 0);
    if (res < 0) {
      return -1;
    }
    nr += res;
  } while (nr < res);

  return nr;
}

int readResponseLine(int sockfd, char *response) {
  //puts("Reading response.");
  size_t read = recv(sockfd, response, BUFF_SIZE, 0);

  if (read < 0) {
    return -1;
  }

  return read;
}

int getResponseCode(char *response) {
  char numStr[4];
  strncpy(numStr, response, 3);

	int code= atoi(numStr);
	//printf("%d\n", code);
  return code;
}

typedef struct {
  char *address;
  int port;
} ConnectionInfo;

void *thread_retrieveFile(void *arg) {
  ConnectionInfo info = *(ConnectionInfo *)arg;

  int socktfd = openSocket(info.address, info.port);

  sem_post(&semaphore);

  char buffer[BUFF_SIZE];
  int nr;
  while ((nr = recv(socktfd, buffer, BUFF_SIZE, 0)) > 0) {
    write(STDOUT_FILENO, buffer, nr);
  }

  close(socktfd);

  return 0;
}

void end(int sockfd) {
  sendMessage(sockfd, "quit", 5);
  exit(0);
}

int main(int argc, char **argv) {

  if (argc < 3) {
    puts("Usage: clientTPC {server_address} {file}");
    return 0;
  }

  char *server_addr = argv[1];
  char *file_path = argv[2];

  struct hostent *host;

  puts("Fetching host ip.");
  if ((host = gethostbyname(server_addr)) == NULL) {
    perror("gethostbyname");
    exit(1);
  }

  char *server_ip = inet_ntoa(*((struct in_addr *)host->h_addr));
  puts("Fetched.");
  printf("%s\n", server_ip);

  puts("Connecting.");
  int sockfd = openSocket(server_ip, FTP_PORT);

  int i;
  char response[BUFF_SIZE];
  //FILE* f = fdopen(sockfd, "r");
  for (int x = 0; x < 10; x++) {
    printf("x = %d\n", x);
    i = recv(sockfd, response, BUFF_SIZE, 0);
    write(STDOUT_FILENO, response, i);
  };
  puts("Connected");

  // user anonymous 331
  puts("Login.");
  int res = sendMessage(sockfd, USER_STR, strlen(USER_STR));
  if (res < 0) {
    end(sockfd);
  }

  if ((res = readResponseLine(sockfd, response)) < 0) {
    end(sockfd);
  }

  if (getResponseCode(response) != 331) {
    end(sockfd);
  }

  // pass pass 230
	puts("Sending password");
  res = sendMessage(sockfd, PASS_STR, strlen(PASS_STR));
  if (res < 0) {
    end(sockfd);
  }

  if ((res = readResponseLine(sockfd, response)) < 0) {
    end(sockfd);
  }

  if (getResponseCode(response) != 230) {
    end(sockfd);
  }

  // pasv
  // 227 Entering Passive Mode (193,137,29,15,217,159).
  // 41
  puts("Setting up tranfer.");
  res = sendMessage(sockfd, PASV, strlen(PASV));
  if (res < 0) {
    end(sockfd);
  }

  if ((res = readResponseLine(sockfd, response)) < 0) {
    end(sockfd);
  }

  if (getResponseCode(response) != 227) {
    end(sockfd);
  }

  int port1;
  int port2;
  int temp;
  sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).", &temp,
         &temp, &temp, &temp, &port1, &port2);

  ConnectionInfo connection;
  connection.address = server_ip;
  connection.port = port1 * 256 + port2;

  sem_init(&semaphore, 0, 0);
  pthread_t thread;
  pthread_create(&thread, NULL, thread_retrieveFile, (void *)&connection);

  sem_wait(&semaphore);

  // retr
  char file_request[6 + strlen(file_path)];
  sprintf(file_request, "retr %s\n", file_path);
  printf("Retrieving: %s\n", file_path);
  res = sendMessage(sockfd, file_request, strlen(file_request));
  if (res < 0) {
    end(sockfd);
  }

  if ((res = readResponseLine(sockfd, response)) < 0) {
    end(sockfd);
  }

  if (getResponseCode(response) == 550) {
    puts("Error retrieving file.");
    end(sockfd);
  }

  if ((res = readResponseLine(sockfd, response)) < 0) {
    end(sockfd);
  }

  pthread_join(thread, NULL);

  puts("Ending");

  end(sockfd);
}
