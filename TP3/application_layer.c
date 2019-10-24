#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "serial.h"

#define BUFFER_SIZE 256

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

int receive_file(int port) {

    int spfd = llopen(port, RECEIVER);

    puts("Connecting.");
    if (spfd < 0) {
      puts("Failed to connenct.");
      return -1;
    }
    puts("Connencted.");

    unsigned char packet[MAX_BUFFER_SIZE];
    unsigned int filename_size; //LValue
    unsigned int read_pointer=0;
    int ud_size = sizeof(unsigned int);

    int new_file;

    llread(spfd, packet);

    if(packet[0]==C_START) {
        printf("\nC_START RECEIVED\n");
    }

    if(packet[1]==T_NAME) {
        printf("\nT_NAME RECEIVED");
    }

    read_pointer+=2;

    struct tlv_filename file_name;
    memcpy(&filename_size, packet+read_pointer, ud_size);
    read_pointer+=ud_size;

    file_name.length = filename_size;

    printf("\nFILE NAME LENGTH: %u\n", file_name.length);

    file_name.value = (char*)malloc(sizeof(char)*filename_size);

    for(int i=0; i<filename_size; i++) {
        file_name.value[i] = packet[i+read_pointer];
    }

    read_pointer+=filename_size;

    printf("\nFILE NAME: %s\n", file_name.value);

    struct tlv_filesize file_size;

    if(packet[read_pointer]==T_SIZE) {
        printf("RECEIVED T_SIZE\n");
    }
    read_pointer++;
    int size_start = read_pointer;
    memcpy(&file_size.length, packet+read_pointer, ud_size);
    read_pointer+=ud_size;

    memcpy(&file_size.value, packet+read_pointer, file_size.length);
    read_pointer += file_size.length;

    printf("FILE SIZE L: %u\n", file_size.length);
    printf("FILE SIZE VALUE: %u\n", file_size.value);


    printf("\n");

    int packet_counter = 0;
    unsigned int data_size;

    // FICHEIRO E MERDAS DO ESTRONDO
    new_file = open(file_name.value, O_CREAT | O_WRONLY);

    puts("Starting data transmission.");
    while(TRUE) {
        printf("\n");
        while(llread(spfd, packet)<0);

        if(packet[0]==C_END) break;

        printf("C: %x\n", packet[0]);
        printf("N; %d\n", packet[1]);

        data_size = (unsigned int)packet[2]*256 + (unsigned int)packet[3];
        printf("L2L1: %d\n\n", data_size);

        write(new_file, &packet[4], data_size);
    }

    llread(spfd, packet);

    llclose(spfd);

    close(new_file);

    return 0;
}

int send_file(int port, char* filePath){
    char* fileName = basename(filePath);

    int fd = open(filePath, O_RDWR);
    if (fd < 0) {
        perror(filePath);
        return -1;
    }

    FILE* fp = fopen(filePath, "r");
    fseek(fp, 0L, SEEK_END);
    unsigned int LValue = ftell(fp);
    fclose(fp);

    puts("Connecting");
    int spfd = llopen(port, SENDER);
    if (spfd < 0) {
      puts("Failed to connect");
      return -1;
    }


    unsigned int nameSize = strlen(fileName);
    unsigned int ui_size = sizeof(unsigned int);

    unsigned int controlSize = 3 + nameSize + 3 * ui_size;
    unsigned char control_start[controlSize];
    unsigned int writePointer = 0;

    printf("name size: %d\n", nameSize);
    printf("name: %s\n", fileName);
    printf("file size size: %d\n", ui_size);
    printf("file size: %d\n\n", LValue);

    control_start[0] = C_START;
    control_start[1] = T_NAME;
    writePointer += 2;

    memcpy(control_start+writePointer, (unsigned char*)&nameSize, ui_size);
    writePointer+=ui_size;

    sprintf(control_start+writePointer, "%s", fileName);
    writePointer += nameSize;

    control_start[nameSize + 7] = T_SIZE;
    writePointer++;

    memcpy(control_start+writePointer, (unsigned char*)&ui_size, ui_size);
    writePointer+=ui_size;

    memcpy(control_start+writePointer, (unsigned char*)&LValue, ui_size);
    writePointer+=ui_size;

    printf("\n");

    // Send Start Control Packet
    puts("Sending control");
    int nr = llwrite(spfd, control_start, controlSize);

    if (nr < 0) {
      puts("Error transmitting.");
      llclose(spfd);
      return -1;
    }

    int packetCounter = 0;
    puts("Starting data transmission.");
    while(TRUE){
        unsigned char packet[4+BUFFER_SIZE];
        int nr = read(fd, packet+4, BUFFER_SIZE);

        if( nr <= 0) {
          if (nr == 0) {
            break;
          } else {
            puts("Error reading file.");
            llclose(spfd);
            return -1;
          }
        };

        unsigned char header[4];
        packet[0] = C_DATA;
        packet[1] = packetCounter;
        packet[2] = nr / 256;
        packet[3] = nr % 256;

        nr = llwrite(spfd, packet, nr+4);

        if (nr <= 0) {
          if (nr == 0) {
            break;
          } else {
            puts("Error transmitting.");
            llclose(spfd);
            return -1;
          }
        }

        packetCounter++;

    }

    // Send End Control Packet
    puts("Finishing");
    control_start[0] = C_END;
    nr = llwrite(spfd, control_start, controlSize);

    if (nr < 0) {
      puts("Error transmitting.");
      llclose(spfd);
      return -1;
    }

    llclose(spfd);

    return 0;
}
