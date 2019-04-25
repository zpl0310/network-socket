/*
** client1.c - client that send packet with data and wait for response.
*/

// The header include shared functions and defines like server port.
#include "shared.h"

// Encapsulate data and simulate bad packet.
// Returns: length of packet.
int MarshalClientMessage(char* buf,
                         const char* data,
                         unsigned char data_len,
                         unsigned char client_id,
                         unsigned char segn,
                         char simulate_error) {
    // Start of packet id.
    buf[0] = 0xFF;
    buf[1] = 0xFF;

    // Client id.
    buf[2] = client_id;

    // Package type.
    buf[3] = 0xFF;
    buf[4] = 0xF1;

    // Segment number.
    if (simulate_error == 's') {  // simulate not in sequence error.
        buf[5] = segn + 1;
    } else if (simulate_error == 'd' && segn > 1) {  // simulate duplicate error.
        buf[5] = segn - 1;
    } else {
        buf[5] = segn;
    }

    // Set data length.
    if (simulate_error == 'l' && data_len > 1) {  // simulate length discrepancy.
        buf[6] = data_len - 1;
    } else {
        buf[6] = data_len;
    }

    // Set data.
    for (int i = 0; i < data_len; i++) {
        buf[7+i] = data[i];
    }

    // Set end of packet id.
    if (simulate_error == 'e') {  // simulate end of packet missing.
        return data_len + 7;
    }
    buf[data_len + 7] = 0xFF;
    buf[data_len + 8] = 0xFF;
 
    return data_len + 9;
}

// Intepret the server response, print an error message if receives reject packet.
// (also print additional error messages if packet format is not within expectation)
void UnmarshalResponse(const char* response, char client_id, char segn) {
    // Check beginning of message.
    if (response[0] != (char)0xFF ||
        response[1] != (char)0xFF ||
        response[2] != client_id ||
        response[3] != (char)0xFF ||
        (response[4] != (char)0xF2 && response[4] != (char)0xF3)) {
        fprintf(stderr, "Invalid response packet.\n");
        exit(1);
    }
    // Handles reject packet, check for error code.
    if (response[4] == (char)0xF3) {
        if (response[5] != (char)0xFF) {
            fprintf(stderr, "Invalid response packet.\n");
            exit(1);
        }
        switch (response[6]) {
            case (char)0xF4:
                fprintf(stderr, "Received packet out of sequence.\n");
                exit(1);
            case (char)0xF5:
                fprintf(stderr, "Packet length mismatch.\n");
                exit(1);
            case (char)0xF6:
                fprintf(stderr, "End of packet missing.\n");
                exit(1);
            case (char)0xF7:
                fprintf(stderr, "Duplicate packet.\n");
                exit(1);
            default:
                fprintf(stderr, "Invalid response packet.\n");
                exit(1);
        }
    }
    // Handles rest of packet for ACK.
    if (response[5] != segn) {
        fprintf(stderr, "Acknowledge packet out of order.\n");
        exit(1);
    }
    if (response[6] != (char)0xFF || response[7] != (char)0xFF) {
        fprintf(stderr, "Invalid response packet.\n");
        exit(1);
    }
    // No error!
    printf("Acknowledgement received!\n");
}

int main(int argc, char *argv[])
{
    // Socket description.
    int sockfd;
    // Address of server.
    struct addrinfo *servinfo, *p;
    // Len of packet to send/received.
    int packetlen, recvlen;
    // Holds the packet to send.
    char buf[MAXPACKETSIZE];
    // Holds the packet received.
    char recvbuf[MAXPACKETSIZE];

    if (argc != 4) {
        fprintf(stderr,"usage: program_name hostname message error_to_simulate\n");
        exit(1);
    }

    p = GetServerAddressAndReturnP(argv[1], SERVERPORT, &sockfd, &servinfo);
    SetTimeOut(sockfd, 3);

    for (unsigned char segn = 1; segn < 6; segn++) {
        if (segn == 3) {  // Simulate Error for packet 3.
            packetlen = MarshalClientMessage(buf, argv[2], strlen(argv[2]), CLIENTID, segn, argv[3][0]);
        } else {
            packetlen = MarshalClientMessage(buf, argv[2], strlen(argv[2]), CLIENTID, segn, 'n');
        }
        PrintPacket(buf, packetlen);

        recvlen = SendPackage(sockfd, buf, packetlen, p, recvbuf, SENDNOATTEMPTS);
        PrintPacket(recvbuf, recvlen); 
        UnmarshalResponse(recvbuf, CLIENTID, segn);

        // Empty for better spacing.
        printf("\n");    
    }
    
    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}
