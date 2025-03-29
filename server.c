#include "server.h"

#define BUFSIZE 1024

typedef struct Player Player;
typedef struct Gameboard Gameboard;

struct Player {
    int sockfd, win_amt, lose_amt;
    char *account, *password;
    Player *next;
};

struct Gameboard {
    int src_fd, dest_fd;
    char board[9];
    struct Gameboard *next;
};

Player *players;
Gameboard *boards;

int check( const char board[3][3] );
void init( int fd );

int main() {

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listener, socket_source;
    socket_listener = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listener)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /* 確保 socket 與指定的 IP/Port 綁定 */
    printf("Binding socket to local address...\n");
    if (bind(socket_listener, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listener, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    fd_set master;      // master file descriptor 清單
    fd_set reads;       // 給 select() 用的暫時 file descriptor 清單
    FD_ZERO(&master);   // 清除 master 
    FD_ZERO(&reads);    // 清除 temp sets

    FD_SET(socket_listener, &master);    // 將 socket_listener 新增到 master set
    SOCKET max_socket = socket_listener; // 持續追蹤最大的 file descriptor
    
    int i = 0;
    SOCKET max_client_fd = -1;
    ssize_t recvlen;
    char buf[BUFSIZE];

    printf("Waiting for connections...\n");
    
    SOCKET client[FD_SETSIZE];
    for (i = 0; i < FD_SETSIZE; i++) {
        client[i] = -1;
    }

    while(1) {
        reads = master;
        // https://beej-zhtw-gitbook.netdpi.net/jin_jie_ji_shu/selectff1a_tong_bu_i__o_duo_gong
        // (max_socket + 1): 它必定大於 standard input（0）
        if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }
        // new client connection
        if (FD_ISSET(socket_listener, &reads)) {
            struct sockaddr_storage client_address;
            socklen_t client_len = sizeof(client_address);
            SOCKET socket_client = accept(socket_listener, (struct sockaddr*) &client_address, &client_len);
            
            if (!ISVALIDSOCKET(socket_client)) {
                fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
                exit(1);
            }

            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0) {
                    client[i] = socket_client; // save descriptor 
                    break;
                }
            }

            if (i == FD_SETSIZE) {
                perror("Too many connections!\nServer crashed.\n");
                exit(1);
            }

            FD_SET(socket_client, &master); // 新增 new client fd 到 master set
            if (socket_client > max_socket) // 持續追蹤最大的 fd
                max_socket = socket_client;
            if (i > max_client_fd)
                max_client_fd = i;
        }

        for (i = 0; i <= max_client_fd; i++) {
            if ((socket_source = client[i]) < 0)
                continue;
            if (FD_ISSET(socket_source, &reads)) {
                if ((recvlen = recv(socket_source, buf, BUFSIZE, 0)) > 0) {
                    buf[recvlen] = '\0';
                    char arg1[200] = {'\0'}, arg2[200] = {'\0'}, arg3[200] = {'\0'};
                    sscanf(buf, "%s %s %s", arg1, arg2, arg3);
                    memset(buf, '\0', BUFSIZE);

                    // create account
                    if (strcmp(arg1, "create") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *temp;
                        temp = players;
                        Player *new_player = (Player *)malloc(sizeof(Player));
                        new_player->account = (char *)malloc(sizeof(arg2));
                        new_player->password = (char *)malloc(sizeof(arg3));
                        strcpy(new_player -> account , arg2);
                        strcpy(new_player -> password , arg3);
                        new_player->win_amt = 0;
                        new_player->lose_amt = 0;
                        new_player->sockfd = -1;
                        new_player->next = NULL;
                        
                        Player *cur, *pre;
                        cur = players;
                        if (cur == NULL)
                            players = new_player;
                        else {
                            while (cur != NULL) {
                                pre = cur;
                                cur = cur->next;
                            }
                            cur = new_player;
                            pre->next = new_player;
                        }
                        
                        printf("Success create %s.\n", new_player->account);
                        strcpy(buf, "Hello, ");
                        strcat(buf, arg2);
                        strcat(buf, "!\n");
                        strcat(buf, "Account create successfully.\n");
                        send(socket_source, buf, sizeof(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }

                    // login
                    else if (strcmp(arg1, "login") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *temp;
                        temp = players;
                        while (temp != NULL) {
                            if (strcmp(temp->account, arg2) == 0) {
                                if (strcmp(temp->password, arg3) == 0 && temp->sockfd == -1) {
                                    printf("%s has just logged in!\n", temp->account);
                                    strcpy(buf, "Hello, ");
                                    strcat(buf, arg2);
                                    strcat(buf, "!");
                                    temp->sockfd = socket_source;                                    
                                    send(socket_source, buf, sizeof(buf), 0);
                                }
                                else if (temp->sockfd != -1) {
                                    strcpy(buf, "the account has been logged!\n");
                                    send(socket_source, buf, sizeof(buf), 0);
                                }
                                else {
                                    strcpy(buf, "Wrong password !\nPlease login again.\n");
                                    send(socket_source, buf, sizeof(buf), 0);
                                }
                                break;
                            }
                            temp = temp->next;
                        }
                        if (temp == NULL) {
                            strcpy(buf, "account is not existed.\n");
                            send(socket_source, buf, sizeof(buf), 0);
                        }
                        memset(buf, '\0', BUFSIZE);
                    }

                    // list players
                    else if (strcmp(arg1, "list") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *temp = players;
                        while (temp != NULL) {
                            if (temp->sockfd != -1) {
                                strcat(buf, temp->account);
                                strcat(buf, ", ");
                            }
                            temp = temp->next;
                        }
                        send(socket_source, buf, sizeof(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }

                    // invite player
                    else if (strcmp(arg1, "invite") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        int dest_fd;
                        Player *src, *dest;
                        src = players;
                        dest = players;
                        Gameboard *srcb, *destb;
                        // find the destination name
                        while (dest != NULL) {
                            if ((strcmp(dest->account, arg2) != 0) || (dest->sockfd == -1))
                                dest = dest->next;
                            else {
                                // find the source name
                                while (src != NULL) {
                                    if (src->sockfd != socket_source)
                                        src = src->next;
                                    else {
                                        strcpy(buf, src->account);
                                        strcat(buf, " is challenging you.\n\nAccept challenge with "
                                                        "usage : accept {challenger}");
                                        send(dest->sockfd, buf, sizeof(buf), 0);
                                        break;
                                    }
                                }
                                if (src == NULL) {
                                    fprintf(stderr, "failed to find the source sockfd.\n");
                                    break;
                                }
                                break;
                            }
                        }
                        if (dest == NULL) {
                            sprintf(buf, "player %s is not existed.", arg2);
                            send(socket_source, buf, sizeof(buf), 0);
                        }
                        memset(buf, '\0', BUFSIZE);
                    }

                    // accept game invitation
                    else if (strcmp(arg1, "accept") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *src, *dest;
                        src = players;
                        dest = players;
                        while (dest != NULL) {
                            if (strcmp(dest->account, arg2) != 0)
                                dest = dest->next;
                            else {
                                // get the source account
                                while (src != NULL) {
                                    if (socket_source != src->sockfd)
                                        src = src->next;
                                    else {
                                        // set the game board
                                        Gameboard *new_board, *cur, *pre;
                                        cur = boards;
                                        pre = boards;
                                        new_board = (Gameboard *)malloc(sizeof(Gameboard));
                                        if (boards == NULL)
                                            boards = new_board;
                                        else {
                                            while(cur != NULL) {
                                                pre = cur;
                                                cur = cur->next;
                                            }
                                            cur = new_board;
                                            pre->next = new_board;
                                        }
                                        if (socket_source < dest->sockfd) {
                                            new_board->src_fd = socket_source;
                                            new_board->dest_fd = dest->sockfd;
                                        }
                                        else {
                                            new_board->src_fd = dest->sockfd;
                                            new_board->dest_fd = socket_source;
                                        }

                                        for (char x = '0'; x < '9'; x++)
                                            new_board->board[x-'0'] = x;
                                        new_board->next = NULL;

                                        printf("%s and %s start to play a game.\n", src->account, dest->account);
                                        sprintf(buf, "%s has accept your challenge!\n\n"
                                                    "Play the game with the usage : set {position} {opponent}\n\n"
                                                    " %c | %c | %c \n"
                                                    "---+---+---\n"
                                                    " %c | %c | %c \n"
                                                    "---+---+---\n"
                                                    " %c | %c | %c "
                                                    , src->account
                                                    , new_board->board[0], new_board->board[1], new_board->board[2]
                                                    , new_board->board[3], new_board->board[4], new_board->board[5]
                                                    , new_board->board[6], new_board->board[7], new_board->board[8]);
                                        send(dest->sockfd, buf, sizeof(buf), 0);
                                        memset(buf, '\0', BUFSIZE);
                                        sprintf(buf, 
                                                    "\n\n"
                                                    " %c | %c | %c \n"
                                                    "---+---+---\n"
                                                    " %c | %c | %c \n"
                                                    "---+---+---\n"
                                                    " %c | %c | %c "
                                                    , new_board->board[0], new_board->board[1], new_board->board[2]
                                                    , new_board->board[3], new_board->board[4], new_board->board[5]
                                                    , new_board->board[6], new_board->board[7], new_board->board[8]);
                                        send(src->sockfd, buf, sizeof(buf), 0);
                                        memset(buf, '\0', BUFSIZE);
                                        break;
                                    }
                                }
                                if (src == NULL) {
                                    fprintf(stderr, "failed to find the source sockfd.\n");
                                    break;
                                }
                                break;
                            }
                        }
                        if (dest == NULL) {
                            sprintf(buf, "account %s is not existed.", arg2);
                            send(socket_source, buf, sizeof(buf), 0);
                            memset(buf, '\0', BUFSIZE);
                        }
                        memset(buf, '\0', BUFSIZE);
                    }

                    // set the position
                    else if (strcmp(arg1, "set") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *src, *dest;
                        src = players; dest = players;
                        Gameboard *board, *pre_board;
                        board = boards;
                        pre_board = boards;

                        // find out the destination sockfd
                        while (dest != NULL) {
                            if (strcmp(dest->account, arg3) != 0)
                                dest = dest->next;
                            else  {
                                while (((board->src_fd != socket_source) || (board->dest_fd != dest->sockfd)) 
                                        && ((board->src_fd != dest->sockfd) || (board->dest_fd != socket_source)) 
                                        && board != NULL) {
                                    pre_board = board;
                                    board = board->next;
                                }

                                // check for the existence of the game
                                if (board == NULL) {
                                    sprintf(buf, "No game is existed between you and %s\n\n", arg3);
                                    strcat(buf, "try usage : invite {player} to invite the player!\n");
                                    send(socket_source, buf, sizeof(buf), 0);
                                    memset(buf, '\0', BUFSIZE);
                                }
                                else {
                                    // check if the cell has been set
                                    if(board->board[atoi(arg2)] == 'O' || board->board[atoi(arg2)] == 'X') {
                                        for (int j = 0; j < 9; j++)
                                            printf("board->board[%d] is %c\n", j, board->board[j]);
                                        strcpy(buf, "the position you select has been set!\n");
                                        send(socket_source, buf, sizeof(buf), 0);
                                        memset(buf, '\0', BUFSIZE);
                                    }
                                    else {
                                        // the smaller sockfd will be O, else be X
                                        if (board->src_fd == socket_source)
                                            board->board[atoi(arg2)] = 'O';
                                        else
                                            board->board[atoi(arg2)] = 'X';
                                        // get the source name
                                        while (src != NULL) {
                                            if (socket_source != src->sockfd)
                                                src = src->next;
                                            else
                                                break;
                                        }
                                        if (src == NULL) {
                                            fprintf(stderr, "failed to get the src name while setting\n");
                                            strcpy(buf, "system error\ntry to type instruction again!\n");
                                            send(socket_source, buf, sizeof(buf), 0);
                                            memset(buf, '\0', BUFSIZE);
                                        }
                                        else {
                                            int winner = -1; // 0 if src win, else 1
                                            // check for diagonal winning lines
                                            if (((board->board[0] == board->board[4]) && (board->board[0] == board->board[8])) ||
                                                ((board->board[2] == board->board[4]) && (board->board[2] == board->board[6]))) {
                                                if ( board->board[0] == 'O')
                                                    winner = 0;
                                                else
                                                    winner = 1;
                                            }
                                            for (int idx=0; idx<3; idx++) {
                                                if ((board->board[0+idx] == board->board[3+idx]) && (board->board[0+idx] == board->board[6+idx])) {
                                                    if (board->board[0+idx] == 'O')
                                                        winner = 0;
                                                    else
                                                        winner = 1;
                                                    break;
                                                }
                                                // columns
                                                else if (((board->board[0+3*idx] == board->board[1+3*idx]) && (board->board[0+3*idx] == board->board[2+3*idx]))) {
                                                    if (board->board[0+3*idx] == 'O')
                                                        winner = 0;
                                                    else
                                                        winner = 1;
                                                    break;
                                                }
                                            }
                                            if (winner == -1) {
                                                sprintf(buf, "\n%s choose %d\n"
                                                            "\nusage : set {position} {opponent}\n\n"
                                                            " %c | %c | %c \n"
                                                            "---+---+---\n"
                                                            " %c | %c | %c \n"
                                                            "---+---+---\n"
                                                            " %c | %c | %c"
                                                            , src->account, atoi(arg2)
                                                            , board->board[0], board->board[1], board->board[2]
                                                            , board->board[3], board->board[4], board->board[5]
                                                            , board->board[6], board->board[7], board->board[8]);
                                                send(dest->sockfd, buf, sizeof(buf), 0);
                                                memset(buf, '\0', BUFSIZE);
                                                sprintf(buf, "\nYou choose %d\n"
                                                            "\n\n"
                                                            " %c | %c | %c \n"
                                                            "---+---+---\n"
                                                            " %c | %c | %c \n"
                                                            "---+---+---\n"
                                                            " %c | %c | %c"
                                                            , atoi(arg2)
                                                            , board->board[0], board->board[1], board->board[2]
                                                            , board->board[3], board->board[4], board->board[5]
                                                            , board->board[6], board->board[7], board->board[8]);
                                                send(src->sockfd, buf, sizeof(buf), 0);
                                                memset(buf, '\0', BUFSIZE);
                                            }
                                            else {
                                                if (winner == 1) {
                                                    dest->win_amt++;
                                                    src->lose_amt++;
                                                    sprintf(buf, "%s win the game!\n"
                                                                "the result is below\n\n"
                                                                " %c | %c | %c \n"
                                                                "---+---+---\n"
                                                                " %c | %c | %c \n"
                                                                "---+---+---\n"
                                                                " %c | %c | %c \n"
                                                                , dest->account
                                                                , board->board[0], board->board[1], board->board[2]
                                                                , board->board[3], board->board[4], board->board[5]
                                                                , board->board[6], board->board[7], board->board[8]);
                                                }
                                                else {
                                                    dest->lose_amt++;
                                                    src->win_amt++;
                                                    sprintf(buf, "%s win the game!\n"
                                                                "the result is below\n\n"
                                                                " %c | %c | %c \n"
                                                                "---+---+---\n"
                                                                " %c | %c | %c \n"
                                                                "---+---+---\n"
                                                                " %c | %c | %c \n"
                                                                , src->account
                                                                , board->board[0], board->board[1], board->board[2]
                                                                , board->board[3], board->board[4], board->board[5]
                                                                , board->board[6], board->board[7], board->board[8]);
                                                }
                                                strcat(buf, "\nInvite someone else to play new game!\n\n");
                                                strcat(buf, " Usage: \n (1) create {account} {password} --Create an account.\n");
                                                strcat(buf, " (2) login {account} {password}  --User login.\n");
                                                strcat(buf, " (3) list  --List all online users.\n");
                                                strcat(buf, " (4) invite {username} --Invite someone to play with you.\n");
                                                strcat(buf, " (5) send {username} {message} --Send a message to a player.\n");
                                                strcat(buf, " (6) record --Take a look at your game records.\n");
                                                strcat(buf, " (7) User logout.\n\n");
                                                
                                                send(src->sockfd, buf, sizeof(buf), 0);
                                                send(dest->sockfd, buf, sizeof(buf), 0);
                                                memset(buf, '\0', BUFSIZE);
                                                pre_board->next = board->next;
                                                free(board);
                                            }
                                        } 
                                    } 
                                }
                                break;
                            }
                        }
                        if (dest == NULL) {
                            sprintf(buf, "NO player %s exists.", arg3);
                            send(socket_source, buf, sizeof(buf), 0);
                            memset(buf, '\0', BUFSIZE);
                        }
                        memset(buf, '\0', BUFSIZE);
                    }

                    else if (strcmp(arg1, "record") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *player = players;
                        while (player != NULL) {
                            if (player->sockfd == socket_source) {
                                sprintf(buf, "Hello %s, here's your record!\n"
                                            " win :  %d\n lose : %d\n"
                                            , player->account, player->win_amt, player->lose_amt);
                                break;
                            }
                            player = player->next;
                        }
                        if (player == NULL) {
                            fprintf(stderr, "match the player failed with instuction record!\n");
                            sprintf(buf, "server failed to match the player!\n");
                        }
                        send(socket_source, buf, strlen(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }
                    
                    // logout 
                    else if (strcmp(arg1, "logout") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *temp = players;
                        while (temp != NULL) {
                            if (temp->sockfd == socket_source) {
                                temp->sockfd = -1;
                                temp->win_amt = 0;
                                temp->lose_amt = 0;
                                sprintf(buf, "Goobye, %s!\n", temp->account);
                                send(socket_source, buf, strlen(buf), 0);
                                printf("%s has just logged out!\n", temp->account);
                                memset(buf, '\0', BUFSIZE);
                                break;
                            }
                            
                            temp = temp->next;
                        }
                        if (temp == NULL)
                            strcpy(buf, "You should login first!\n");
                        send(socket_source, buf, strlen(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }
                    
                    // send messege
                    else if(strcmp(arg1, "send") == 0) {
                        memset(buf, '\0', BUFSIZE);
                        Player *src, *dest;
                        src = players;
                        dest = players;
                        
                        while (dest != NULL) {
                            if (strcmp(dest->account, arg2) != 0)
                                dest = dest->next;
                            else{
                                // get the source account
                                while (src != NULL) {
                                    if (socket_source != src->sockfd)
                                        src = src->next;
                                    else {
                                        strcpy(buf,src->account);
                                        strcat(buf," : ");
                                        strcat(buf,arg3);
                                        strcat(buf,"\n");
                                        send(dest->sockfd,buf,strlen(buf),0);
                                        memset(buf,0,BUFSIZE);
                                        strcpy(buf,"You : ");
                                        strcat(buf,arg3);
                                        strcat(buf,"\n");
                                        send(src->sockfd,buf,strlen(buf),0);
                                        memset(buf, '\0', BUFSIZE);
                                        break;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    else {
                        strcpy(buf, " Usage:\n (1) create {account} {password} -- Create an account.\n");
                        strcat(buf, " (2) login {account} {password}  --User login.\n");
                        strcat(buf, " (3) list  --List all online users.\n");
                        strcat(buf, " (4) invite {username} --Invite someone to play with you.\n");
                        strcat(buf, " (5) send {username} {message} --Send a message to a player.\n");
                        strcat(buf, " (6) record --Take a look at your game records.\n");
                        strcat(buf, " (7) User logout.\n\n");
                        send(socket_source, buf, sizeof(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }
                    bzero(buf, sizeof(buf));
                }
                else {
                    printf("close the sock %d\n", socket_source);
                    Player *temp = players;
                    while (temp != NULL) {
                        if (temp->sockfd == socket_source) {
                            temp->sockfd = -1;
                            break;
                        }
                        temp = temp->next;
                    }
                    if (temp == NULL) {
                        fprintf(stderr, "socket %d logged out failed!\n", socket_source);
                    }
                    close(socket_source);
                    FD_CLR(socket_source, &master);
                    client[i] = -1;
                }
            }
        }
    }

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listener);

    printf("Finished.\n");

    return 0;
}

