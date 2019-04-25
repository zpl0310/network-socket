/*
** server2.c - server to verify subscriber connection
*/

// The header include shared functions and defines like server port.
#include "shared.h"

// Maximum number of lines to read from database file.
#define MAXDBSIZE 100

// Intepret the received request, populate data if correct format, set error if not.
void UnmarshalRequest(const char* packet,
                      int* sub_no,
                      char* tech) {
    // Check non-data parts of packet.
    if (packet[0] != (char)0xFF ||
        packet[1] != (char)0xFF ||
        packet[3] != (char)0xFF ||
        packet[4] != (char)0xF8 ||
        packet[6] != 5 ||
        packet[12] != (char)0xFF ||
        packet[13] != (char)0xFF) {
        fprintf(stderr, "Wrong request packet format\n");
        exit(1);
    }

    // Convert char array into int by bitshift.
    *sub_no = ((unsigned char)packet[8]<<24) +
              ((unsigned char)packet[9]<<16) +
              ((unsigned char)packet[10]<<8) +
              ((unsigned char)packet[11]);
    *tech = packet[7];
}

// Check with database and set return buffer.
void CheckAndSetResponse(const char* packet,
                         char* buf,
                         int sub_no,
                         int tech,
                         const int sub_list[],
                         const char tech_list[],
                         const char paid_list[],
                         int db_len) {
    // copy most of the packet.
    for (int i = 0; i < 14; i++) {
        buf[i] = packet[i];
    }
    // initially set return type to not exist.
    buf[4] = 0xFA;
    // check database and set return type accordingly.
    for (int i = 0; i < db_len; i++) {
        if (sub_list[i] == sub_no) {
            if (tech_list[i] == tech && paid_list[i] == 1) {
                buf[4] = 0xFB;  // accept message;
                return;
            }
            buf[4] = 0xF9;  // subscriber number found.
        }
    }
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo *p;
    int rv;
    int numbytes, bytestosend;
    struct sockaddr_storage their_addr;
    char recvbuf[MAXPACKETSIZE];  // Incoming packet buffer;
    char retbuf[14];  // Return packet buffer;
    socklen_t addr_len;
    // Hold subscriber number from request.
    int sub_no;
    char tech;

    // Hold the test database.
    int sub_list[3] = {4085546805, 4086668821, 4086808821};
    char tech_list[3] = {4, 3, 2};
    char paid_list[3] = {1, 0, 1};
    // Hold the database load from file.
    int fsub_list[MAXDBSIZE];
    char ftech_list[MAXDBSIZE];
    char fpaid_list[MAXDBSIZE];
    int fdb_size = 0;  // if 0, use test database.

    if (argc != 2) {
        fprintf(stderr, "usage: program_name [db_file_name|\"test\"]\n");
        fprintf(stderr, "  \"test\" to use test database\n");
        exit(1);
    }

    if (strcmp(argv[1], "test") == 0) {
        printf("Using test database: 4085546805-4(paid) / 4086668821-3(unpaid), -2(paid)\n");
    } else {  // read the database file.
        FILE *fp;
        char line[60];
        char *cptr;
        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            fprintf(stderr, "database file does not exist!");
            exit(1);
        }
        while (fgets(line, 60, fp) != NULL) {
            if (fdb_size >= MAXDBSIZE) {
                printf("Maximum database read limit reached, ignoring rest of file.\n");
                break;
            }
            fsub_list[fdb_size] = strtoul(line, &cptr, 10);
            ftech_list[fdb_size] = strtoul(cptr, &cptr, 10);
            fpaid_list[fdb_size] = strtoul(cptr, &cptr, 10);
            printf("%s", line);
            ++fdb_size;
        }
        fclose(fp);
    }

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

        // Exit if incoming packet size incorrect.
        if (numbytes != 14) {
            fprintf(stderr, "incoming request packet size unexpected.\n");
            exit(1);
        }

        UnmarshalRequest(recvbuf, &sub_no, &tech);

        printf("received request, with subscriber no. %u, tech %d", sub_no, tech);

        if (fdb_size == 0) {
            CheckAndSetResponse(recvbuf, retbuf, sub_no, tech,
                                sub_list, tech_list, paid_list, 3);
        } else {
            CheckAndSetResponse(recvbuf, retbuf, sub_no, tech,
                                fsub_list, ftech_list, fpaid_list, fdb_size);
        }

        // Return message.
        if ((numbytes = sendto(sockfd, retbuf, 14, 0,
                 (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("server: sendto");
            exit(1);
        }

        // Empty for better spacing.
        printf("\n");
    }

    close(sockfd);

    return 0;
}
