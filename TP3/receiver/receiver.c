#include "../serial.h"
#include "../application_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char* argv[]) {

    if (argc < 2) {
      puts("Args: {port_number}");
      return -1;
    }

    int port = atoi(argv[1]);

    srand(time(0));

    int fd = receive_file(port);

    close(fd);

    return 0;
}
