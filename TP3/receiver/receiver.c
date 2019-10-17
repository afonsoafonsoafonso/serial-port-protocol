#include "../serial.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    puts("Opening.\n");
    int fd = llopen(COM0, RECEIVER);

    char buf[10000];
    puts("Starting reading.\n");
    int nr = llread(fd, buf);

    buf[nr] = 0;

    printf("\nFull message:\n%s\n", buf);

    llclose(fd);

    return 0;
}
