/* This is the server code */
#include "file-server.h"
#include <sys/fcntl.h>
#include <pthread.h>

#define QUEUE_SIZE 10

typedef struct args_to_thread {
    int socket_accept;
    char client_ip_str[INET_ADDRSTRLEN];
} args_to_thread;

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

char* eval_client_request(MSG_PACKET* packet, int*start, int*end, int*size_file, int*rw) {
    char* file_name = malloc(strlen(packet->FILE_NAME)+1);
    strcpy(file_name, packet->FILE_NAME);
    
    //Check header
    //If TX, we do not care about rest of packet
    if(packet->MSG_TYPE == CLIENT_TX){
        *rw = 1;
        if( access(file_name, F_OK) != -1)
            packet->ERR_NO = FAIL_NAME_EXIST;
        else
            packet->ERR_NO = SUCCESS;
        
        return file_name;
    }
    //IF RX, we continue to other packet portions
    else if(packet->MSG_TYPE == CLIENT_REC) {
        *rw = 0;
        if( access(file_name, F_OK) != -1) {
            packet->ERR_NO = SUCCESS;
            *size_file = file_size(file_name);
            if(*size_file < 0) {
                packet->ERR_NO = FAIL_UNKNOWN;
                return file_name;
            }
        }
        else {
            packet->ERR_NO = FAIL_NAME_DN_EXIST;
            return file_name;
        }
    }
    //We only have the two, if we get here bad stuff happened
    else {
        packet->MSG_TYPE == CLIENT_TX;
        packet->ERR_NO = FAIL_UNKNOWN;
        return file_name;
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
            return file_name;
    }

    //printf("Size of File: %d, start: %d, start > size: %d\n", *size_file, *start, *start > *size_file);

    if(*start >= *end || *start > *size_file)
        packet->ERR_NO = FAIL_BYTE_RANGE_INV;
    else if(*end > *size_file) {
        packet->ERR_NO = WARNING_EOF_REACH;
        *end = *size_file;
    }
    return file_name;
}

/******
 * function:    isget_req
 * params  :    void
 * purpose :    the function reads from global buffer, no need to have any passing of params
 */
char* isget_req(char* buf) {
    char get_buf[4];
    int i = 0;
    char* file_name;
    while(!isspace(buf[i]) && i < (sizeof(get_buf)-1)) {  //don't overflow our buf, stop at space
        get_buf[i] = buf[i];
        i++;
    } 
    get_buf[i] = '\0';                          //add null terminator to string
    if(strcasecmp(get_buf, "GET")==0)
    {
        i+=2;                                   //incr and ignore forward slash
        file_name = malloc(256*sizeof(char));
        int j = 0;
        //printf("Size of file: %lu\n", sizeof(file_name));
        while(!isspace(buf[i]) && j < (256-1)) {
            file_name[j] = buf[i];
            //printf("Here is our c: %c\n", file_name[j]);
            i++;
            j++;
        }
        file_name[j] = '\0';
        return file_name;
    }
    else
        return NULL;
}

/*****
 * function: handle_client_req
 * params  : @sa : number returned from accept in main
 *           @buf: buffer that has read data
 * purpose : handle when there is no get request
 */  
void handle_client_req(int sa, char* buf, char* str_ip) {  
    int start, end, size_file, rw;
    char* file_name;
    MSG_PACKET* packet;
    
    //handle packet, buf already has it read in
    packet = malloc(sizeof(MSG_PACKET));                         //Allocate MEM
    memcpy(packet, buf, sizeof(MSG_PACKET));                     //Copy to ptr

    /*
    printf("RECEIVED PACKET!:\n MSG_HEADER: %u, ERR: %u, START: %u, END, %u, FLAG: %u\n", 
        packet->MSG_TYPE, packet->ERR_NO, packet->START_BYTE, 
        packet->END_BYTE, packet->BYTE_VAL_FLAG); 
    */
    //Generate the reply packet, and setup our values
    file_name = eval_client_request(packet, &start, &end, &size_file, &rw);
    
    write(sa, packet, sizeof(MSG_PACKET));

    /*
    printf("SENT PACKET!:\n MSG_HEADER: %u, ERR: %u, START: %u, END, %u, FLAG: %u\n",
        packet->MSG_TYPE, packet->ERR_NO, packet->START_BYTE,
        packet->END_BYTE, packet->BYTE_VAL_FLAG);
    */

    //if what was asked cannot be done, do not continue
    if(packet->ERR_NO != SUCCESS &&  packet->ERR_NO != WARNING_EOF_REACH) {
        return;
    }

    if(DEBUG) {
        printf("Sending %s to %s\n", file_name, str_ip);
    }

    FILE* fp;
    int prev_percent, counter, bytes;

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
    
        int bytes_to_send = end-start;
        int percent_sent;
        while (1) {
            bytes = counter == end ? 0 : fread(buf, 1, sizeof(buf), fp);	                    /* read from file */
            if (bytes <= 0) {
                if(DEBUG)
                    printf("Finished sending %s to %s\n", file_name, str_ip);
                break;		                                        /* check for end of file */ 
            }

            if((bytes + counter) > end)
                bytes = bytes_to_send-counter;

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

/*****
 * function:    handle_get_req
 * params  :    @sa         : socket accept int
 *              @buf        : buffer with the read request
 *              @file_name  : file_name retrieved by the isget_req
 *              @client_ip  : for debug statements
 */
void handle_get_req(int sa, char* buf, char* file_name, char* str_ip) {
    if(DEBUG) {
        printf("Sending %s to %s\n", file_name, str_ip);
    }
    //printf("This is in our buffer: %s\n", buf);
    memset(buf, '\0', BUF_SIZE);                                    //clean buffer
    strcpy(buf, good_html_request_str);                             //copy in good request 
    write(sa, buf, BUF_SIZE);                                       //send
 
    if( access(file_name, F_OK) == -1) {                            //filename extracted is not valid
       memset(buf, '\0', BUF_SIZE);
       strcpy(buf, file_not_found_str);
       write(sa, buf, BUF_SIZE);
       free(file_name);
       close(sa);
       return; 
    }

    //reached here, so filename does exist
    FILE* fp;
    fp = fopen(file_name, "rb");

    if(fp == NULL) fatal("FILE OPEN ERROR IN HANDLE GET REQ");
    
    int size_file = file_size(file_name);    
    int bytes, counter = 0, prev_percent=0, percent_sent=0;
    while (1) {
        bytes = fread(buf, 1, sizeof(buf), fp); 
        if (bytes <= 0) {
            if(DEBUG)
                printf("Finished sending %s to %s\n", file_name, str_ip);
            break;                                              /* check for end of file */
        }

        counter += bytes;
        percent_sent = (int)((((double)counter)/size_file)*100);

        if(DEBUG) {
            while(prev_percent+10 <= percent_sent) {
                prev_percent += 10;
                printf("Sent %d%% of %s\n", prev_percent, file_name);
            }
        }
        write(sa, buf, bytes);                                  /* write bytes to socket */
    }
    fclose(fp);
    free(file_name);
    close(sa);
}

/*****
 * function: accept_client_request
 * params  : args: ptr to int from socket accept
 * purpose : evaluate input from socket and handle GET or client app request
 */
void accept_client_request(void *args) {
    args_to_thread* my_args = (args_to_thread*)args;
    int sa = my_args->socket_accept;  //Get the port socket
    char* str_ip = malloc(sizeof(my_args->client_ip_str));
    strcpy(str_ip, my_args->client_ip_str);

    char buf[BUF_SIZE];
    char* file_name;

    //Grab request from socket
    memset(buf, '\0', BUF_SIZE);                                    //clean buffer
    read(sa, buf, BUF_SIZE);                
    //read_packet(sa, sizeof(MSG_PACKET), buf);
    file_name = isget_req(buf);                                                            
    if(!file_name) {                                                //If null returned, means no GET found    
        handle_client_req(sa, buf, str_ip);
    }
    else {
        handle_get_req(sa, buf, file_name, str_ip);
    }
    free(my_args);                                                  //was malloced in main
    free(str_ip);
}

int main(int argc, char *argv[])
{	
  int s, b, l, fd, sa, bytes, on = 1, counter, prev_percent;
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
  char str_ip[INET_ADDRSTRLEN];

  pthread_t pthread_id;

  /* Socket is now set up and bound. Wait for connection and process it. */
  while (1) {
      sa = accept(s, (struct sockaddr *)&client_addr, &socket_len);		                                    /* block for connection request */
      if (sa < 0) fatal("accept failed");
      
      //Grab client address as string to pass into thread
      client_ip = client_addr.sin_addr;
      inet_ntop(AF_INET, &client_ip, str_ip, INET_ADDRSTRLEN);
        
      args_to_thread* args = malloc(sizeof(args_to_thread));
      args->socket_accept = sa;
      strcpy(args->client_ip_str, str_ip);
      if(pthread_create(&pthread_id, NULL, (void*)accept_client_request, (void*)args) != 0)
      {
          printf("FAILURE IN THREAD CREATE!\n");
          exit(1);
      } 
  }
}
