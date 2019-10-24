#include "../serial.h"
#include "../application_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {

    if (argc < 3) {
      puts("Args: {port_number} {file_path}");
      return -1;
    }

    int port = atoi(argv[1]);

    send_file(port, argv[2]);

    return 0;
}
