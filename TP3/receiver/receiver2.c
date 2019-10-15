#include "../serial.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    puts("opening");
    int fd = llopen(COM2, RECEIVER);

    char buf[1024];
    puts("receiving");
    int nr = llread(fd, buf);

    buf[nr] = 0;

    printf("\nFull message:%s\n", buf);

    return 0;
}