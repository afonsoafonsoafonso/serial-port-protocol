#include "../serial.h"
#include "../application_layer.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {

    send_file("test.txt");

    /*int fd = llopen(COM0, SENDER);

    char *buf = "~~}}}~}~}}}}}~~~";
    unsigned int size = strlen(buf);

    int i = 0;
    while (i < size) {
        int nr = llwrite(fd, buf, size);
        if (nr > 0) {
            i += nr;
        } else if (nr == 0) {
            break;
        }
    }

    puts("\nDisconecting...\n");
    llclose(fd);
*/
    return 0;
}
