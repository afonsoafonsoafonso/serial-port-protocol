#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "serial.h"

#define BUFFER_SIZE 100

#define C_DATA 0x01
#define C_START 0x02
#define C_END 0x03
#define T_NAME 10
#define T_SIZE 11

struct tlv {
    unsigned char Type;
    unsigned char Lenght;
};

int receive_file() {
    char packet[BUFFER_SIZE];
    int fileSize;
    unsigned char fileSizeBytes[4];
    int spfd = llopen(COM2, RECEIVER);

    llread(spfd, packet);

    if(packet[0]==C_START) {
        printf("\nC_START RECEIVED\n");
    }
    if(packet[1]==T_NAME) {
        printf("\nT_NAME RECEIVED");
    }

    /*
    fileSizeBytes[0]=packet[2];
    fileSizeBytes[1]=packet[3];
    fileSizeBytes[2]=packet[4];
    fileSizeBytes[3]=packet[5];

    fileSize=(int)fileSizeBytes;
    */
    //fileSize = (int)packet[2] | ((int)packet[3]<< 8) | ((int)packet[4]<<16) | ((int)packet[5]<< 24);
    ((unsigned char*)&fileSize)[0] = packet[2];
    ((unsigned char*)&fileSize)[1] = packet[3];
    ((unsigned char*)&fileSize)[2] = packet[4];
    ((unsigned char*)&fileSize)[3] = packet[5];
    printf("\n%d\n", fileSize);

    for(int i=2; i<8; i++) {
        printf("BYTE: %x\n", packet[i]);
    }

}

int send_file(char* filePath){
    char* fileName = basename(filePath);

    int fd = open(filePath, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    FILE* fp =fopen(filePath, "r");
    fseek(fp, 0L, SEEK_END);
    unsigned int fileSize = ftell(fp);
    fclose(fp);

    int spfd = llopen(2, SENDER);

    unsigned int nameSize = strlen(fileName);
    unsigned int controlSize = 3 + nameSize + 3* sizeof(unsigned int);
    unsigned char control_start[controlSize];

    control_start[0] = C_START;
    control_start[1] = T_NAME;
    //*(&control_start[2]) = nameSize;
    control_start[2] = ((unsigned char*)&nameSize)[0];
    control_start[3] = ((unsigned char*)&nameSize)[1];
    control_start[4] = ((unsigned char*)&nameSize)[2];
    control_start[5] = ((unsigned char*)&nameSize)[3];
    printf("ADWADAWDAWD: %d\n", nameSize);
    sprintf(&control_start[6], "%s", fileName);
    control_start[nameSize + 7] = T_SIZE;
    *(&control_start[nameSize + 8]) = 4;
    *(&control_start[nameSize + 12]) = fileSize;
    
    // Send Start Control Packet
    puts("Sending control");
    llwrite(spfd, control_start, controlSize);
    puts("sent");
    int packetCounter = 0;
    while(TRUE){
        char packet[BUFFER_SIZE];
        puts("reading");
        int nr = read(fd, packet, BUFFER_SIZE);
        puts("read");
        if( nr <= 0) break; 
        unsigned char header[4];
        header[0] = C_DATA;
        header[1] = packetCounter;
        header[2] = nr / 256;
        header[3] = nr % 256;

        puts("Sending header");
        llwrite(spfd, header, 4);
        puts("Sending data");
        llwrite(spfd, packet, nr);
        packetCounter++;
        
    }

    // Send End Control Packet
    control_start[0] = C_END;
    puts("Finishing");
    llwrite(spfd, control_start, controlSize);

    llclose(spfd);

    return 0;
}