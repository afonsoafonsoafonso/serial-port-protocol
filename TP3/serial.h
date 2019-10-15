#define FALSE 0
#define TRUE 1

#define BAUDRATE B38400

#define ESCAPE 0x7d

#define FLAG 0x7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_N0 0x00
#define C_N1 0x40
#define C_RR0 0x05
#define C_RR1 0x85
#define C_DISC 0x0B
#define C_REJ0 0x01
#define C_REJ1 0x81

#define MAX_TRIES 3
#define TIMEOUT_THRESHOLD 3

#define COM0 0
#define COM1 1
#define COM2 2

enum open_mode {SENDER, RECEIVER};

int llopen(int port, enum open_mode mode);
int llwrite(int fd, char* buffer, unsigned int length);
int llread(int fd, char* buffer);
int llclose(int fd);

void printAction(int sent, unsigned char c_byte, int n_data);