/* This is the server code */
#include "file-server.h"
#include <sys/fcntl.h>

#define QUEUE_SIZE 10



int DEBUG = 1;   //define global for debug


int main(int argc, char *argv[])
{	
  int s, b, l, fd, sa, bytes, on = 1, counter, sz_file, prev_percent;
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
        
        read(sa, buf, BUF_SIZE);		                                /* read file name from socket */
        /*grab ip address as str */
        client_ip = client_addr.sin_addr;
        inet_ntop(AF_INET, &client_ip, str_ip, INET_ADDRSTRLEN);
        
        /*move the sent filename into a char array */
        file_name = malloc(sizeof(char)*(strlen(buf)+1)); //account for null terminator
        strcpy(file_name, buf);
        
        printf("We have received: %s\n", file_name);
        
        if(DEBUG) {
            printf("Sending %s to %s\n", file_name, str_ip);
        }

        /* Get and return the file. */
        fp = fopen(file_name, "rb");	                                    /* open the file to be sent back */
        if (fp == NULL) fatal("open failed");
       
        /* grab size of file */
        fseek(fp, 0L, SEEK_END);
        sz_file = ftell(fp);
        rewind(fp);
        counter = 0; prev_percent = 0;
        while (1) {
                bytes = fread(buf, 1, sizeof(buf), fp);	                    /* read from file */
                if (bytes <= 0) {
                    if(DEBUG)
                        printf("Finished sending %s to %s\n", file_name, str_ip);
                    break;		                                        /* check for end of file */ 
                }
                counter += bytes;
                printf("Counter: %d, prev_percent: %d\n", counter, prev_percent);
                if(DEBUG) {
                    if(((int)(((double)counter)/sz_file))*100 > prev_percent) {
                        prev_percent = (int)((double)counter/sz_file)*100;
                        printf("Sent %d%% of %s\n", prev_percent, file_name);
                    }
                }
                write(sa, buf, bytes);		                            /* write bytes to socket */
        }
        fclose(fp);			                                            /* close file */
        close(sa);			                                            /* close connection */
        free(file_name);                                                /* free memory malloced */
  }
}
