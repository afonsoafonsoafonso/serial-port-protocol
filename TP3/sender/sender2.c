#include "../serial.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    int fd = llopen(COM0, SENDER);

    char buf[1024];
    for (int i = 0; i < 300; i++) {
        buf[i] = i%2 == 0 ? '~' : '}';
    }
    int nr = llwrite(fd, buf, 300);

    if (nr < 0){
        puts("error");
    }

    puts("\nDisconecting...\n");
    llclose(fd);

    return 0;
}
