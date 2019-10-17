#include "../serial.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    puts("Opening.\n");
    int fd = llopen(COM0, RECEIVER);

    char buf[10000];
    int nr;
    int i =0;
    puts("Starting reading.\n");
    while (1) {
    nr = llread(fd, buf+i);
    if (nr > 0) {
            i += nr;
        } else if (nr == 0) {
            break;
        }
    }

    buf[nr] = 0;

    printf("\nFull message:\n%s\n", buf);

    llclose(fd);

    return 0;
}
