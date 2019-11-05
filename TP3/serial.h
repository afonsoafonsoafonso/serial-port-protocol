#define FALSE 0
#define TRUE 1

#define COM0 0
#define COM1 1
#define COM2 2

#define MAX_BUFFER_SIZE 1024

#define TOO_BIG_ERROR -4

enum open_mode {SENDER, RECEIVER};

void setFER(float newFER);
void setTprop(float newT);

int llopen(int port, enum open_mode mode);
int llwrite(int fd, char* buffer, unsigned int length);
int llread(int fd, char* buffer);
int llclose(int fd);

void printAction(int sent, unsigned char c_byte, int n_data);
