#include "../serial.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    puts("Opening.\n");
    int fd = llopen(COM0, RECEIVER);

    unsigned char buf[10000];
    int nr;
    int i =0;
    int j =0;
    puts("Starting reading.\n");
    while (1) {
        nr = llread(fd, buf+i);
        if (nr > 0) {
            i += nr;
        } else if (nr == 0) {
            break;
        }
        for (; j < i; j++) {
            printf("%x ", buf[j]);
        }
        printf("\n");
    }

    buf[i] = 0;

    printf("\nFull message:\n");
    for (int j = 0; j < i; j++) {
        printf("%x", buf[j]);
    }
    printf("\n");

    llclose(fd);

    return 0;
}
