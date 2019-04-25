/*
** client2.c - client that send a connection request.
*/

// The header include shared functions and defines like server port.
#include "shared.h"
#include <limits.h>

// Encapsulate a connection request.
void MarshalRequest(char* buf, unsigned long sub_no, char tech) {
    if (sub_no > UINT_MAX || tech < 2 || tech > 5) {
        fprintf(stderr, "Subscriber number or tech out of bound.\n");
        exit(1);
    }        

    // Start of packet id.
    buf[0] = 0xFF;
    buf[1] = 0xFF;

    // Client id.
    buf[2] = CLIENTID;

    // Package type.
    buf[3] = 0xFF;
    buf[4] = 0xF8;

    // Segment number.
    buf[5] = 0x01;

    // Set data length.
    buf[6] = 0x05;

    // Set tech.
    buf[7] = tech;

    // Set subscriber number.
    buf[8] = (sub_no&0xFF000000) >> 24;
    buf[9] = (sub_no&0xFF0000) >> 16;
    buf[10] = (sub_no&0xFF00) >> 8;
    buf[11] = sub_no&0xFF;

    // Set end of packet id.
    buf[12] = 0xFF;
    buf[13] = 0xFF;
}

// Intepret the server response, return the response type.
// (also print additional error messages if packet format is not within expectation)
char CheckResponse(const char* response, const char* request) {
    for (int i = 0; i < 14; i++) {
        if (i != 4) {  // Except for byte 4, rest of response should be identical
            if (response[i] != request[i]) {
                fprintf(stderr, "Invalid response packet.\n");
                exit(1);
            }
        }
    }
    // Check for response type, on the critical byte.
    switch (response[4]) {
        case (char)0xF9 :
            return 'p';  // not paid.
        case (char)0xFA :
            return 'e';  // does not exist.
        case (char)0xFB :
            return 'a';  // accepted.
        default :
            return 'u';  // unknown.
    }
}

int main(int argc, char *argv[])
{
    // Socket description.
    int sockfd;
    // Address of server.
    struct addrinfo *servinfo, *p;
    // Len of packet received.
    int recvlen;
    // Holds the packet to send.
    char buf[14];
    // Holds the packet received.
    char recvbuf[MAXPACKETSIZE];
    // Hold the subscriber number and tech.
    unsigned long sub_no;
    char tech;
    char *cptr;
    // Hold the response type. a: accepted; p: not paid; e: not exist.
    char rtype;

    if (argc != 4) {
        fprintf(stderr,"usage: program_name hostname subscriber_no tech\n");
        exit(1);
    }

    // Get the subscriber number and tech from arguments.
    sub_no = strtoul(argv[2], &cptr, 10);
    tech = strtoul(argv[3], &cptr, 10);
   
    printf("Subscriber number = %lu; Technology is %d\n", sub_no, tech);
 
    p = GetServerAddressAndReturnP(argv[1], SERVERPORT, &sockfd, &servinfo);
    SetTimeOut(sockfd, 3);

    MarshalRequest(buf, sub_no, tech);
    PrintPacket(buf, 14);

    recvlen = SendPackage(sockfd, buf, 14, p, recvbuf, SENDNOATTEMPTS);
    PrintPacket(recvbuf, recvlen); 
    if (recvlen != 14) {
        fprintf(stderr,"response length not expected\n");
        exit(1);
    }

    rtype = CheckResponse(recvbuf, buf);
    
    // Print a message indicating whether connection request is successful.
    if (rtype == 'a') {
        printf("Connection accepted!\n"); 
    } else if (rtype == 'p') {
        printf("Connection rejected: Service not paid.\n"); 
    } else if (rtype == 'e') {
        printf("Connection rejected: Subscriber number not found.\n");
    } else {
        printf("Unknown response type.\n");
    }
     
    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}
