/*
** server1.c -- a server that respond with acknowledgement or rejection
*/

// The header include shared functions and defines like server port.
#include "shared.h"

// Intepret the received packet, populate data if correct format, set error if not.
// Returns: char representing error.
//  s: Case1 - out of sequence
//  l: Case2 - length does not match
//  e: Case3 - no end of packet
//  d: Case4 - duplicated packet
//  o: other
//  n: no error 

//read packet
char UnmarshalPacket(const char* packet,
                     int packetlen,
                     char* buf,
                     unsigned char *segn,
                     unsigned char *client_id) {
    // Check beginning of message.
    if (packet[0] != (char)0xFF ||
        packet[1] != (char)0xFF ||
        packet[3] != (char)0xFF ||
        packet[4] != (char)0xF1) {
        return 'o';
    }
    *client_id = packet[2];

    // Checks segment no.
    if (packet[5] != *segn + 1) {
        if (packet[5] == *segn) {
            return 'd';
        } else {
            *segn = packet[5];
            return 's';
        }
    }
    *segn = packet[5];
    
    // Checks end of message.
    if (packet[packetlen - 1] != (char)0xFF ||
        packet[packetlen - 2] != (char)0xFF) {
        return 'e';
    }  

    // Checks length of payload.
    if (packet[6] != packetlen - 9) {
        return 'l';
    }

    // Otherwise correct, populate buffer.
    for (int i = 0; i < packetlen - 9; i++) {
        buf[i] = packet[7+i];
    } 
    return 'n';
}

void SetAckPacket(char* buf,
                  unsigned char client_id,
                  unsigned char segn) {
    // Start of packet id.
    buf[0] = 0xFF;
    buf[1] = 0xFF;

    // Client id.
    buf[2] = client_id;

    // Package type: ACK.
    buf[3] = 0xFF;
    buf[4] = 0xF2;
    
    // Segment no.
    buf[5] = segn;
    
    // End of packet.
    buf[6] = 0xFF;
    buf[7] = 0xFF;
}

void SetRejectPacket(char* buf,
                     char error_code,
                     unsigned char client_id,
                     unsigned char segn) {
    // Start of packet id.
    buf[0] = 0xFF;
    buf[1] = 0xFF;

    // Client id.
    buf[2] = client_id;

    // Package type: REJECT.
    buf[3] = 0xFF;
    buf[4] = 0xF3;
  
    // Reject reason.
    buf[5] = 0xFF;
    switch (error_code) {
        case 's':
            buf[6] = 0xF4;
            break;
        case 'l':
            buf[6] = 0xF5;
            break;
        case 'e':
            buf[6] = 0xF6;
            break;
        case 'd':
            buf[6] = 0xF7;
            break;
        default:
            buf[6] = 0xF8;
            break;
    }
  
    // Segment no.
    buf[7] = segn;
    
    // End of packet.
    buf[8] = 0xFF;
    buf[9] = 0xFF;
}

int main(void)
{
    int sockfd;
    struct addrinfo *p;
    int numbytes, bytestosend;
    struct sockaddr_storage their_addr;
    char recvbuf[MAXPACKETSIZE];  // Incoming packet buffer;
    char buf[MAXPACKETSIZE];  // Payload buffer;
    char retbuf[10];  // Return packet buffer;
    socklen_t addr_len;
    unsigned char segn, client_id;
    char error_code;

    // Expect the first segment number to be 1;
    segn = 0;

    // Call shared function to establish socket.
    p = SetServerSocketAndReturnP(SERVERPORT, &sockfd);

    // Loop until force quit.
    while (1) {
        printf("server: waiting to recvfrom...\n");
    
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, recvbuf, MAXPACKETSIZE-1 , 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
    
        printf("server: received packet, which is %d bytes long\n", numbytes);
        
        if ((error_code = UnmarshalPacket(recvbuf, numbytes, buf, &segn, &client_id)) == 'n') {
            buf[numbytes - 9] = '\0';
            printf("listener: packet no. %d payload is \"%s\"\n", segn, buf);
            SetAckPacket(retbuf, client_id, segn);
            bytestosend = 8;
        } else {
            printf("listener: packet contains error, sending rejection\n");
            SetRejectPacket(retbuf, error_code, client_id, segn);
            bytestosend = 10;
        }
    
        // Return message.
        if ((numbytes = sendto(sockfd, retbuf, bytestosend, 0,
                 (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("server: sendto");
            exit(1);
        }
        
        // Reset if error encountered, or received 5 packets.
        if (error_code != 'n' || segn == 5) {
            segn = 0;
        }
        
        // Empty for better spacing.
        printf("\n");    
    }

    close(sockfd);

    return 0;
}
