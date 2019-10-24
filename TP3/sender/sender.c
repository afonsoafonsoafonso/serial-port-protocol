#include "../serial.h"
#include "../application_layer.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
  
    send_file("evereste.jpg");

    // int fd = llopen(COM0, SENDER);

    // char *buf = "a~}}}~}~}}}}}~~~";
    // unsigned int size = strlen(buf);

    // int i = 0;
    // for (; i < 5; i++) {
    //     int nr = llwrite(fd, buf, size);
    // }

    // llwrite(fd, buf, 1);

    // puts("\nDisconecting...\n");
    // llclose(fd);

    return 0;
}
