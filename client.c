#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAXLINE 4096

int main (int argc, char *argv[]) {
    int sockfd, listenfd;
    struct sockaddr_in servaddr;

    if (argc != 2) {
        fprintf(stderr, "usage:tcpcli <IPaddress>\n");
        exit(1);
    }
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "socket error\n");
        exit(1);
    }

    /* set the servaddr */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton error for %s\n", argv[1]);
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "connect error\n");
        exit(1);
    }

    printf( " Usage:\n (1) create {account} {password} -- Create an account.\n");
    printf( " (2) login {account} {password}  --User login.\n");
    printf( " (3) list  --List all online users.\n");
    printf( " (4) invite {username} --Invite someone to play with you.\n");
    printf( " (5) send {username} {message} --Send a message to a player.\n");
    printf( " (6) record --Take a look at your game records.\n");
    printf( " (7) User logout.\n\n");

    /* deal with the message */
    while (1) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);
        FD_SET(0, &rset);

        if (select(sockfd+1, &rset, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "select() failed\n");
            exit(1);
        }
        if (FD_ISSET(sockfd, &rset)) {
            char recvline[MAXLINE];
            memset(recvline, '\0', MAXLINE);
            int bytes_received = recv(sockfd, recvline, MAXLINE, 0);
            if (bytes_received < 1) {
                printf("\nDisonnect!\n");
                break;
            }
            printf("\n%s\n\n", recvline);
        }
        if (FD_ISSET(0, &rset)) {
            char recvline[MAXLINE];
            if (!fgets(recvline, MAXLINE, stdin)) 
                break;
            send(sockfd, recvline, strlen(recvline), 0);
            memset(recvline, '\0', MAXLINE);
        }
    }

    exit(0);
}
