#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include "serial.h"

#define C_DATA 0x01
#define C_START 0x02
#define C_END 0x03
#define T_NAME 10
#define T_SIZE 11

struct tlv {
    unsigned char Type;
    unsigned char Lenght;
};

int send_file(char* filePath){
    char* fileName = basename(filePath);

    int fd = open(filePath, O_RDONLY);
    FILE* fp =fdopen(fd, "r");
    fseek(fp, 0L, SEEK_END);
    unsigned int fileSize = ftell(fp);
    fclose(fp);

    int spfd = llopen(0, SENDER);

    unsigned int nameSize = strlen(fileName);
    unsigned int controlSize = 3 + nameSize + 3* sizeof(unsigned int);
    unsigned char control_start[controlSize];

    control_start[0] = C_START;
    control_start[1] = T_NAME;
    *(&control_start[2]) = nameSize;
    sprintf(&control_start[6], "%s", fileName);
    control_start[nameSize + 7] = T_SIZE;
    *(&control_start[nameSize + 8]) = 4;
    *(&control_start[nameSize + 12]) = fileSize;
    
    // Send Start Control Packet
    llwrite(spfd, control_start, controlSize);
    int packetCounter = 0;
    while(TRUE){
        char packet[100];
        int nr = read(fd, packet, 100);
        if( nr <= 0) break; 
        unsigned char header[4];
        header[0] = C_DATA;
        header[1] = packetCounter;
        header[2] = nr / 256;
        header[3] = nr % 256;

        llwrite(spfd, header, 4);
        llwrite(spfd, packet, 100);
        packetCounter++;
        
    }

    // Send End Control Packet
    control_start[0] = C_END;
    llwrite(spfd, control_start, 1);

    llclose(fd);
}