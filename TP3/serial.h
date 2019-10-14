#define COM0 0
#define COM1 1

enum open_mode {SENDER, RECEIVER};

int llopen(int port, enum open_mode mode);
int llwrite(int fd, char* buffer, unsigned int length);
int llread(int fd, char* buffer);
int llclose(int fd);