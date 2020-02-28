/* This is the server code */
#include "file-server.h"
#include <sys/fcntl.h>

#define QUEUE_SIZE 10



int DEBUG = 1;   //define global for debug

//Assumes file exists already when called
int file_size(char * file_name) {
    FILE* fptr;
    fptr = fopen(file_name, "rb");
    if(fptr == NULL) {
        printf("FAILED TO OPEN FILE: %s\n", file_name);
        return -1;
    }

    fseek(fptr, 0L, SEEK_END);
    int sz_file = ftell(fptr);
    fclose(fptr);
    return sz_file;
}

void eval_client_request(MSG_PACKET* packet, char* file_name, int*start, int*end, int*size_file, int*rw) {
    //Check header
    //If TX, we do not care about rest of packet
    if(packet->MSG_TYPE == CLIENT_TX){
        *rw = 1;
        if( access(file_name, F_OK) != -1)
            packet->ERR_NO = FAIL_NAME_EXIST;
        else
            packet->ERR_NO = SUCCESS;
        return;
    }
    //IF RX, we continue to other packet portions
    else if(packet->MSG_TYPE == CLIENT_REC) {
        *rw = 0;
        if( access(file_name, F_OK) != -1) {
            packet->ERR_NO = SUCCESS;
            *size_file = file_size(file_name);
            if(*size_file < 0) {
                packet->ERR_NO = FAIL_UNKNOWN;
                return;
            }
        }
        else {
            packet->ERR_NO = FAIL_NAME_DN_EXIST;
            return;
        }
    }
    //We only have the two, if we get here bad stuff happened
    else {
        packet->MSG_TYPE == CLIENT_TX;
        packet->ERR_NO = FAIL_UNKNOWN;
        return;
    }

    //here we check the byteflag, to see what values to set
    switch(packet->BYTE_VAL_FLAG) {
        case BYTE_RANGE_INV:
            *start = 0;
            *end = *size_file;
            break;
        
        case BYTE_RANGE_START:
            *start = packet->START_BYTE;
            *end = *size_file;
            break;

        case BYTE_WINDOW_VAL:
            *start = packet->START_BYTE;
            *end = packet->END_BYTE;
            break;

        default:
            packet->ERR_NO = FAIL_UNKNOWN;
            return;
    }

    //printf("Size of File: %d, start: %d, start > size: %d\n", *size_file, *start, *start > *size_file);

    if(*start >= *end || *start > *size_file)
        packet->ERR_NO = FAIL_BYTE_RANGE_INV;
    else if(*end > *size_file)
        packet->ERR_NO = WARNING_EOF_REACH;
        *end = *size_file;
}

int main(int argc, char *argv[])
{	
  int s, b, l, fd, sa, bytes, on = 1, counter, prev_percent;
  char buf[BUF_SIZE];		                                            /* buffer for outgoing file */
  struct sockaddr_in channel;		                                    /* holds IP address */
  FILE* fp;

  /* Build address structure to bind to socket. */
  memset(&channel, 0, sizeof(channel));	                                /* zero channel */
  channel.sin_family = AF_INET;
  channel.sin_addr.s_addr = htonl(INADDR_ANY);
  channel.sin_port = htons(SERVER_PORT);

  /* Passive open. Wait for connection. */
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);                        /* create socket */
  if (s < 0) fatal("socket failed");
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));

  b = bind(s, (struct sockaddr *) &channel, sizeof(channel));
  if (b < 0) fatal("bind failed");

  l = listen(s, QUEUE_SIZE);		                                    /* specify queue size */
  if (l < 0) fatal("listen failed");
  
  /* for grabbing the address from the socket */
  struct sockaddr_in client_addr;                                       /* denote needed structs */
  struct in_addr client_ip;
  socklen_t socket_len = sizeof(client_addr);

  char str_ip[INET_ADDRSTRLEN];                                         /* for packaging ip as str */
  char* file_name;                                                      /* for saving filename     */

  /* Socket is now set up and bound. Wait for connection and process it. */
  while (1) {
        sa = accept(s, (struct sockaddr *)&client_addr, &socket_len);		                                    /* block for connection request */
        if (sa < 0) fatal("accept failed");
        
        //set up ptrs for file name and packet, will malloc when necessary.
        // REMEMBER TO FREE
        int start, end, size_file, rw;
        char* file_name;
        MSG_PACKET* packet;

        //Handle file_name from client
        read(sa, buf, BUF_SIZE);		                             //read file_name
        file_name = malloc(sizeof(char)* (strlen(buf)+1));           //Allocate mem
        strcpy(file_name, buf);                                      //copy filename

        //Handle packet
        read(sa, buf, BUF_SIZE);
        packet = malloc(sizeof(MSG_PACKET));                         //Allocate MEM
        memcpy(packet, buf, sizeof(MSG_PACKET));                     //Copy to ptr
        
        /*printf("RECEIVED PACKET!:\n MSG_HEADER: %u, ERR: %u, START: %u, END, %u, FLAG: %u\n", 
                packet->MSG_TYPE, packet->ERR_NO, packet->START_BYTE, 
                packet->END_BYTE, packet->BYTE_VAL_FLAG); 
        */
        //Generate the reply packet, and setup our values
        eval_client_request(packet, file_name, &start, &end, &size_file, &rw);

        write(sa, packet, sizeof(MSG_PACKET));

        /*printf("SENT PACKET!:\n MSG_HEADER: %u, ERR: %u, START: %u, END, %u, FLAG: %u\n",
                packet->MSG_TYPE, packet->ERR_NO, packet->START_BYTE,
                packet->END_BYTE, packet->BYTE_VAL_FLAG);
        */
        //if what was asked cannot be done, do not continue
        if(packet->ERR_NO != SUCCESS &&  packet->ERR_NO != WARNING_EOF_REACH) {
            continue;
        }

        /*grab ip address as str */
        client_ip = client_addr.sin_addr;
        inet_ntop(AF_INET, &client_ip, str_ip, INET_ADDRSTRLEN);
        
        if(DEBUG) {
            printf("Sending %s to %s\n", file_name, str_ip);
        }
        
        if(rw) {                    //If 1, means we are writing
            fp = fopen(file_name, "wb");
            if(fp == NULL) {
                printf("FAILURE IN OPENING FILE!\n");
                exit(1);
            }

            /* Go get the file and write it to standard output. */
            while (1) {
                bytes = read(sa, buf, BUF_SIZE); /* read from socket */
                if (bytes <= 0) {
                    break;
                }
                fwrite(buf, bytes, 1,  fp);
            }
        }
        else {
            /* Get and return the file. */
            fp = fopen(file_name, "rb");	                                    /* open the file to be sent back */
            if (fp == NULL) fatal("open failed");
       
            //setup the file read
            fseek(fp, start, SEEK_SET);
            counter = 0; prev_percent = 0;
            printf("END: %d, START: %d, SIZE_FILE: %d\n", end, start, size_file);
            int bytes_to_send = end-start;
            int percent_sent;
            while (1) {
                    bytes = fread(buf, 1, sizeof(buf), fp);	                    /* read from file */
                    if (bytes <= 0) {
                        if(DEBUG)
                            printf("Finished sending %s to %s\n", file_name, str_ip);
                        break;		                                        /* check for end of file */ 
                    }
                    counter += bytes;
                    percent_sent = (int)((((double)counter)/bytes_to_send)*100);
                    
                    if(DEBUG) {
                        while(prev_percent+10 <= percent_sent) {
                            prev_percent += 10;
                            printf("Sent %d%% of %s\n", prev_percent, file_name);
                        }
                    }
                    write(sa, buf, bytes);		                            /* write bytes to socket */
            }
        }
        fclose(fp);			                                            /* close file */
        close(sa);			                                            /* close connection */
        free(file_name);                                                /* free memory malloc*/
  }
}
