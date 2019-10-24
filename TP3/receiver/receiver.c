//#include "../serial.h"
#include "../application_layer.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {

    if (argc < 2) {
      puts("Args: {port_number}");
      return -1;
    }

    int port = atoi(argv[1]);

    receive_file(port);

    return 0;
}
