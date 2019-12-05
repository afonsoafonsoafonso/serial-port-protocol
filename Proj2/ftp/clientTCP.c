/*      (C)2000 FEUP  */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
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
#define USER_STR "user "
#define PASS_STR "pass "
#define PASV "pasv\n"

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

void end(int sockfd) {
  sendMessage(sockfd, "quit", 5);
  exit(0);
}

int startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

int main(int argc, char **argv) {

  if (argc < 3) {
    puts("Usage: clientTPC {server_address} {file}");
    return 0;
  }

  char *server_addr = argv[1];
  char *file_path = argv[4];

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

  char response[BUFF_SIZE];
  FILE* sock = fdopen(sockfd, "r");
  int header = 1;
  while (header) {
    fgets(response, BUFF_SIZE, sock);

    //write(STDOUT_FILENO, response, strlen(response));

    if (startsWith("220 ", response)) {
      header = 0;
    }
  };
  puts("Connected");

  // user anonymous 331
  puts("Login.");
  char userStr[strlen(USER_STR) + strlen(argv[2]) + 1];
  sprintf(userStr, "%s%s\n", USER_STR, argv[2]);
  int res = sendMessage(sockfd, userStr, strlen(userStr));
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
  char passStr[strlen(PASS_STR) + strlen(argv[3]) + 1];
  sprintf(passStr, "%s%s\n", PASS_STR, argv[3]);
  res = sendMessage(sockfd, passStr, strlen(passStr));
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

  int recvSocktfd = openSocket(connection.address, connection.port);

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

  char buffer[BUFF_SIZE];
  int nr;
  while ((nr = recv(recvSocktfd, buffer, BUFF_SIZE, 0)) > 0) {
    write(STDOUT_FILENO, buffer, nr);
  }

  close(recvSocktfd);

  puts("Ending");

  end(sockfd);
}
