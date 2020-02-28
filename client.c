/* Author:  Chance Reimer, modified from project default code
 * Purpose: To fulfill requirements in project-file-server.pdf 
 * Date:    2/28/2020
 */

#include "file-server.h"

/*******
 * Function:      print_instructions
 * Params  :      N/A
 * Purpose :      Display use case for program
 */
void print_instructions()
{
    printf("Client Commands: \n");
    printf("********Mandatory***********\n");
    printf("client server-name file-name\n");
    printf("********Optional************\n");
    printf("-s : start byte to begin reading from file, expects an int parameter after flag\n");
    printf("-e : end byte to stop reading from file, expects an int parameter after flag\n");
    printf("format: client server-name -s START_BYTE -e END_BYTE filename\n");
    printf("*******Further Options******\n");
    printf("-w : denotes flag to upload file in question. Is not compatible with -s, and -e flags\n");
    printf("format client server-name -w filename\n");
    printf("*******End of Line**********\n");
}


void trim_trail(char* input) {
    char* end = input + strlen(input) - 1;
    while(end > input && isspace(*end)) end--;
    end[1] = '\0';
}

/******
 * Checks if string is a number
 */
int isCharNum(char* input)
{
    int i;

    if(!input)
        return 0;

    trim_trail(input);

    for(i = 0; i < strlen(input); i++)
    {
        printf("Value is: %c., val: %d\n", input[i], input[i] == '\0');
        if(!isdigit(input[i]))
            return 0;
    }
    return 1;
}

/******
 * Function:    parse_args
 * Params  :    @file_name:  null ptr for grabbing file name
 *              @start_byte: null ptr for grabbing start_byte
 *              @end_byte:   null ptr for grabbing end_byte
 * Purpose :    Parses argv, searches for flags. Does not check start_byte end_byte validity
 *              however, does check for -s, -e, and -w flags used together. Will exit if case
 *              occurs
 */             
void parse_args(int argc, char** argv, int* start_byte, int* end_byte, int* write)
{
    int i, byte_select = 0;
    *write = 0;
    int select;
    while((select = getopt(argc, argv, "s:e:w")) != -1) {
        switch(select)
        {
            case 's':
                if(*write) {
                    printf("ERROR: CANNOT BYTE SELECT WRITE!\n");
                    print_instructions();
                    exit(1);
                }
                if(isCharNum(optarg)) {
                    byte_select = 1;
                    *start_byte = atoi(optarg);
                }
                else {
                    printf("START BYTE FLAG MUST HAVE INT INPUT!\n");
                    print_instructions();
                    exit(1);
                }
                break;

            case 'e':
                if(*write) {
                    printf("ERROR: CANNOT BYTE SELECT WRITE!\n");
                    print_instructions();
                    exit(1);
                }
                if(isCharNum(optarg)) {
                    byte_select = 1;
                    *end_byte = atoi(optarg);
                }
                else {
                    printf("END BYTE FLAG MUST HAVE INT INPUT!\n");
                    print_instructions();
                    exit(1);
                }
                break;

            case 'w':
                if(byte_select) {
                    printf("ERROR: CANNOT BYTE SELECT WRITE!\n");
                    print_instructions();
                    exit(1);
                }
                printf("Write enabled!\n");
                *write = 1;
                break;

            case '?':
                if(optopt == 's' || optopt == 'e') {
                    printf("START BYTE AND/OR END BYTE FLAGS REQUIRE ARGS\n");
                    print_instructions();
                    exit(1);
                }
                else if(isprint(optopt)) {
                    printf("UNKNOWN OPTION: '-%c'\n", optopt);
                    exit(1);
                }
                else {
                    printf("UNKNOWN CHARACTER: '\\x%x'\n", optopt);
                    exit(1);
                }
                break;

            default:
                printf("UNKNOWN ERROR IN PARSE ARGS\n");
                exit(1);
           
        }
    }
}

/******
 * function:    pack_start_message
 * params  :    @packet    : ptr to packet element, possibly null
 *              @start_byte: position of start byte, possible neg
 *              @end_byte  : position of end byte, possible neg
 *              @rw        : determines read or write to server based on prev flags
 * purpose :    provide a method of packaging a message to send to server
 */
void pack_start_message(MSG_PACKET* packet, int start_byte, int end_byte, int rw)
{
    enum BYTE_VAL_FLAG byte_select_flag = BYTE_RANGE_INV;
    //Check if we are given null ptr
    if(!packet)
    {
        printf("FAILURE! DID NOT assign packet memory!\n");
        return;
    }

    //If we are attempting to send a file, we only care about message type field
    if(rw) {
        packet->MSG_TYPE = CLIENT_TX;
        return;
    }
    else
        packet->MSG_TYPE = CLIENT_REC;

    if(start_byte < 0)
        start_byte = 0;
    else
        byte_select_flag = BYTE_RANGE_START;

    if(end_byte < 0)
        end_byte = 0;
    else
        byte_select_flag = BYTE_WINDOW_VAL;

    packet->ERR_NO = SUCCESS;
    packet->START_BYTE = start_byte;
    packet->END_BYTE = end_byte;
    packet->BYTE_VAL_FLAG = byte_select_flag;
}

/******
 * function:    decipher_server_reply
 * params  :    @packet: ptr to packet, should be already set
 * purpose :    read reply from server and determine correct output
 */    
int decipher_server_reply(MSG_PACKET* packet) {
    //error checking
    if(!packet) {
        printf("DECIPHER SERVER REPLY RECEIVED NULL PACKET!\n");
        return 0;
    }

    switch(packet->ERR_NO)
    {
        case SUCCESS:
            return 1;       //Successfully able to do what was asked
        case FAIL_NAME_EXIST:
            if(packet->MSG_TYPE == CLIENT_TX)
                printf("ERROR: FILE NAME SELECTED ALREADY EXISTS IN SERVER!\n");
            else
                printf("UNDETERMINED ERROR: FILE_NAME_EXIST FLAG ENCOUNTERED WHEN NOT QUERYING A UPLOAD REQUEST\n");
            break;

        case FAIL_NAME_DN_EXIST:
            if(packet->MSG_TYPE == CLIENT_REC)
                printf("ERROR: FILE NAME SELECTED DOES NOT EXIST IN SERVER!\n");
            else
                printf("UNDETERMINED ERROR: FILE_NAME_DN_EXIST FLAG ENCOUNTERED WHEN NOT QUERYING A DOWNLOAD REQUEST\n");
            break;

        case FAIL_BYTE_RANGE_INV:
            printf("ERROR: INVALID BYTE RANGE SELECTED! CHECK INPUT.\n");
            print_instructions();
            break;

        case WARNING_EOF_REACH:
            printf("WARNING: END BYTE SELECTED IS LARGER THAN FILE SIZE. FILE SIZE WILL BE SENT\n");
            return 1;

        case FAIL_UNKNOWN:
            printf("UNDETERMINED ERROR: FLAG FAIL_UNKNOWN ENCOUNTERED. POSSIBLE SERVER FAILURE.\n");
            break;

        default:
            printf("UNKNOWN REPLY IN ERR_NO STRUCTURE! COMMS TERMINATED\n");
    }

    return 0;
}

int main(int argc, char **argv)
{
  int c, s, bytes;
  char buf[BUF_SIZE];		        /* buffer for incoming file */
  struct hostent *h;		        /* info about server */
  struct sockaddr_in channel;		/* holds IP address */
  MSG_PACKET* packet = malloc(sizeof(MSG_PACKET));
  
  //check to see if sufficient arguments  
  if(argc < 3) {
    printf("ERROR: NOT ENOUGH ARGUMENTS!\n");
    print_instructions();
    exit(1);
  }

  //grab flags, if any are present
  int start_byte = -1, end_byte = -1, rw = 0;
  parse_args(argc, argv, &start_byte, &end_byte, &rw);
  
  int i, count = 0;
  char* host_name;
  char* file_name;
  for(i = optind; i < argc; i++)
  {
      if(count == 0) {
          host_name = malloc(strlen(argv[i])+1);
          strcpy(host_name, argv[i]);
      }
      else if(count == 1){
          file_name = malloc(strlen(argv[i])+1);
          strcpy(file_name, argv[i]);
      }
      printf("Args: %s\n", argv[i]);
      count++;
  }       
      
  //Based on print_instructions, we assume ALWAYS that IP addr is argv[1]
  h = gethostbyname(host_name);		/* look up host's IP address */
  printf("HOST NAME: %s\n", host_name);
  if (!h) {
     printf("FAILED TO GET HOST: %s\n", host_name);
     exit(1);
  }
  
  //Set up socket
  s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s <0) fatal("socket");
  memset(&channel, 0, sizeof(channel));
  channel.sin_family= AF_INET;
  memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
  channel.sin_port= htons(SERVER_PORT);
    
  //Connect socket w/ error check
  c = connect(s, (struct sockaddr *) &channel, sizeof(channel));
  if (c < 0) fatal("connect failed");

  /* We are expecting that final argument is filename, ALWAYS */
  write(s, file_name, strlen(file_name)+1);
  
  //After file name write, Server is expecting our negotiation packet
  printf("PACKAGING AND SENDING PACKET\n");
  pack_start_message(packet, start_byte, end_byte, rw);     //for write, active high
  if(packet == NULL)
      printf("Packet message is failing!!!\n");
  write(s, packet, sizeof(MSG_PACKET));                     //Write our packet to socket
  if(!packet)
      printf("PACKET IS NULL!?\n");
  else
      printf("SENT PACKET!\nMSG_HEADER: %u, ERR: %u, START: %u, END, %u, FLAG: %u\n",
          packet->MSG_TYPE, packet->ERR_NO, packet->START_BYTE,
          packet->END_BYTE, packet->BYTE_VAL_FLAG);
  read(s, buf, BUF_SIZE);                                   //We are expecting a reply
  printf("READ PACKET!\n");
  memcpy(packet, buf, sizeof(MSG_PACKET));                  //copy the buffer memory over
  if(!decipher_server_reply(packet))                        //error checking
      exit(1);

  FILE *fileIO;
  char dirPath[] = "out/";
  char* filePath = malloc((strlen(dirPath) + strlen(file_name)+2)*sizeof(char));
  if(filePath == NULL) {
      printf("FAILED TO MALLOC\n");
  }
  printf("Successfully opened file!\n");
  sprintf(filePath, "%s%s", dirPath, file_name);
  printf("Path made: %s\n", filePath);

  if(rw) {
    fileIO = fopen(filePath, "rb");
    if(fileIO == NULL) {
        printf("FAILURE IN OPENING FILE: Ensure path exists!\n");
        exit(1);
    }

    /* Go get the file and write it to standard output. */
    while (1) {
        bytes = fread(buf, 1, sizeof(buf), fileIO);                     /* read from file */
        if (bytes <= 0) {
            break;                                              /* check for end of file */
        }

        write(s, buf, bytes); /* read from socket */
        printf("Sent %d bytes\n", bytes);
    }
  }
  else {
    fileIO = fopen(filePath, "wb");
    if(fileIO == NULL) {
        printf("FAILURE IN OPENING FILE!\n");
        exit(1);
    }
    
    /* Go get the file and write it to standard output. */
    while (1) {
            bytes = read(s, buf, BUF_SIZE);	/* read from socket */
            printf("We received something!, bytes received: %d\n", bytes);
            if (bytes <= 0) {
                printf("Ending program\n");
                break;
            }
            printf("We got here?\n");
            fwrite(buf, bytes, 1,  fileIO);
            printf("We wrote once!\n");
    }
  }

  fclose(fileIO);
  //free all malloced mem
  free(filePath);
  free(packet);
  free(file_name);
  free(host_name);
}
