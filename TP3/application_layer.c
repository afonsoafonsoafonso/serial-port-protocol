#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "serial.h"

#define BUFFER_SIZE 200

#define C_DATA 0x01
#define C_START 0x02
#define C_END 0x03
#define T_NAME 10
#define T_SIZE 11

struct tlv_filename {
    //unsigned char type;
    unsigned int length;
    unsigned char* value;
};

struct tlv_filesize {
    //unsigned char type;
    unsigned int length;
    unsigned int value;
};

int receive_file() {
    unsigned char packet[MAX_BUFFER_SIZE];
    int filename_size; //LValue
    int spfd = llopen(COM2, RECEIVER);

    int new_file;

    llread(spfd, packet);

    if(packet[0]==C_START) {
        printf("\nC_START RECEIVED\n");
    }
    if(packet[1]==T_NAME) {
        printf("\nT_NAME RECEIVED");
    }

    struct tlv_filename file_name;

    ((unsigned char*)&filename_size)[0] = packet[2];
    ((unsigned char*)&filename_size)[1] = packet[3];
    ((unsigned char*)&filename_size)[2] = packet[4];
    ((unsigned char*)&filename_size)[3] = packet[5];

    file_name.length = filename_size;

    file_name.value = (char*)malloc(sizeof(char)*filename_size);

    for(int i=0; i<filename_size; i++) {
        file_name.value[i] = packet[i+6];
    }

    printf("\nFILE NAME: %s\n", file_name.value);

    struct tlv_filesize file_size;

    file_size.length = packet[filename_size+7];

    ((unsigned char*)&file_size.value)[0] = packet[filename_size+12];
    ((unsigned char*)&file_size.value)[1] = packet[filename_size+13];
    ((unsigned char*)&file_size.value)[2] = packet[filename_size+14];
    ((unsigned char*)&file_size.value)[3] = packet[filename_size+15];

    printf("\n FILE SIZE VALUE: %d\n", file_size.value);

    int packet_counter = 0;
    unsigned int data_size;

    // FICHEIRO E MERDAS DO ESTRONDO
    new_file = open(file_name.value, O_CREAT | O_WRONLY);

    while(TRUE) {
        printf("\n");
        while(llread(spfd, packet)<0);

        if(packet[0]==C_END) break;

        printf("C: %x\n", packet[0]);   
        printf("N; %d\n", packet[1]);

        data_size = (unsigned int)packet[2]*256 + (unsigned int)packet[3];
        printf("L2L1: %d\n", data_size);
        printf("\n");

        for(int i =0; i<data_size; i++) {
            printf("%x ", packet[4+i]);
        }
        write(new_file, &packet[4], data_size);
    }

    llread(spfd, packet);

    close(new_file);

    return 0;
}

int send_file(char* filePath){
    char* fileName = basename(filePath);

    int fd = open(filePath, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    FILE* fp =fopen(filePath, "r");
    fseek(fp, 0L, SEEK_END);
    unsigned int LValue = ftell(fp);
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
    *(&control_start[nameSize + 12]) = LValue;
    
    // Send Start Control Packet
    puts("Sending control");
    llwrite(spfd, control_start, controlSize);
    puts("sent");
    int packetCounter = 0;
    while(TRUE){
        unsigned char packet[4+BUFFER_SIZE];
        puts("reading");
        int nr = read(fd, packet+4, BUFFER_SIZE);
        puts("read");
        if( nr <= 0) break; 
        unsigned char header[4];
        packet[0] = C_DATA;
        packet[1] = packetCounter;
        packet[2] = nr / 256;
        packet[3] = nr % 256;

        puts("Sending data");
        llwrite(spfd, packet, nr+4);
        packetCounter++;
        
    }

    // Send End Control Packet
    control_start[0] = C_END;
    puts("Finishing");
    llwrite(spfd, control_start, controlSize);

    llclose(spfd);

    return 0;
}