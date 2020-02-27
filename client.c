/* This page contains a client program that can request a file from the server program
 * on the next page. The server responds by sending the whole file.
 */

#include "file-server.h"


int main(int argc, char **argv)
{
  int c, s, bytes;
  char buf[BUF_SIZE];		/* buffer for incoming file */
  struct hostent *h;		/* info about server */
  struct sockaddr_in channel;		/* holds IP address */

  if (argc != 3) fatal("Usage: client server-name file-name");
  h = gethostbyname(argv[1]);		/* look up host's IP address */
  if (!h) fatal("gethostbyname failed\n");

  printf("Successfully got host name\n");

  s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s <0) fatal("socket");
  printf("Socket good?\n");
  memset(&channel, 0, sizeof(channel));
  channel.sin_family= AF_INET;
  memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
  channel.sin_port= htons(SERVER_PORT);

  printf("what the fuck\n");
  c = connect(s, (struct sockaddr *) &channel, sizeof(channel));
  if (c < 0) fatal("connect failed");

  /* Connection is now established. Send file name including 0 byte at end. */
  write(s, argv[2], strlen(argv[2])+1);
  
  printf("Got to open!\n");
  FILE *fileIO;
  char dirPath[] = "out/";
  printf("Wow I'm fucking dumb\n");
  char* filePath = malloc((strlen(dirPath) + strlen(argv[2])+2)*sizeof(char));
  if(filePath == NULL) {
      printf("FAILED TO MALLOC\n");
  }
  printf("Successfully opened file!\n");
  sprintf(filePath, "%s%s", dirPath, argv[2]);
  printf("Path made: %s\n", filePath);
  fileIO = fopen(filePath, "wb");
  /* Go get the file and write it to standard output. */
  while (1) {
        bytes = read(s, buf, BUF_SIZE);	/* read from socket */
        printf("We received something!, bytes received: %d\n", bytes);
        if (bytes <= 0) {
            printf("Ending program\n");
            fclose(fileIO);
            exit(0);		/* check for end of file */
        }
        //write(1, buf, bytes);	     	/* write to standard output */
        fwrite(buf, bytes, 1,  fileIO);
        printf("We wrote once!\n");
  }
  fclose(fileIO);
}
