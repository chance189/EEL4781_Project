/* This page contains a client program that can request a file from the server program
 * on the next page. The server responds by sending the whole file.
 */

#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define SERVER_PORT 8000		/* arbitrary, but client & server must agree */
#define BUF_SIZE 4096		        /* block transfer size */


char good_html_request_str[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
char file_not_found_str[]  = "<!DOCTYPE html>\r\n<html>\r\n<head>\r\n<title>404 Error</title>\r\n</head>\r\n<body>\r\n<h1>404 Error: File not found in server</h1>\r\n</body>\r\n<html>\r\n";

/*****
 * Enums defined to negotiate type of message information sent from end to end
 */
enum MSG_TYPE {
    CLIENT_REC = 0,
    CLIENT_TX
};

enum ERR_NO {
    SUCCESS = 0,
    FAIL_NAME_EXIST,
    FAIL_NAME_DN_EXIST,
    FAIL_BYTE_RANGE_INV,            //If start byte > end byte 
    WARNING_EOF_REACH,              //If END_BYTE > sizeof(file), warn is returned, check END_BYTE for sizeof(file)
    FAIL_UNKNOWN                    //Should be sent back if any fields are not a decided enum value
};

enum BYTE_VAL_FLAG {
    BYTE_RANGE_INV = 0,
    BYTE_RANGE_START,
    BYTE_WINDOW_VAL
};

/*****
 * Struct for message definition
 * Use fixed size so that the message will remain same length on both server an client side,
 * regardless of size of (int) in respective environments
 * 
 * Choose 8 bit for message definition, as flags do not exceed above 255, save memory
 *
 * File sizes are limited for sending, as the uint32_t will have a max of 2^32 - 1,
 * thus the file size cannot exceed ~4GB
 */
typedef struct MSG_PACKET {
    uint8_t MSG_TYPE;
    uint8_t ERR_NO;
    uint32_t START_BYTE;
    uint32_t END_BYTE;
    uint8_t BYTE_VAL_FLAG;
    char FILE_NAME[256];
} MSG_PACKET;

void fatal(char *string)
{
  printf("%s\n", string);
  exit(1);
}

void read_packet(int socket, unsigned int size_p, void*buf)
{
    int bytes_read = 0;
    int bytes_rec;
    while(bytes_read < size_p) {
        bytes_rec = read(socket, buf+bytes_read, size_p-bytes_read);
        if(bytes_rec < 1)
        {
            printf("Failure in reading from socket\n");
            exit(1);
        }
    bytes_read += bytes_rec;
    }
}
#endif  //FILE_SERVER_H
