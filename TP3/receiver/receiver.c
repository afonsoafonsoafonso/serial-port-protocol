#include "../serial.h"
#include "../application_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char* argv[]) {

    if (argc < 2) {
      puts("Args: {port_number} {FER} {a}");
      return -1;
    }

    int port = atoi(argv[1]);

    srand(time(0));

    if (argc >= 3) {
      float FER = atof(argv[2]);
      if (FER >= 0 && FER <= 1) {
        setFER(FER);
      } else {
        puts("invalid FER");
      }
    }

    if (argc >= 4) {
      float a = atof(argv[3]);
      setTprop(a);
    }

    int fd = receive_file(port);

    close(fd);

    return 0;
}
