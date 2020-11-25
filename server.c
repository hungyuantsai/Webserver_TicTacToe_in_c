#include "server.h"

#define BUFSIZE 1024

typedef struct Player Player;
typedef struct Gameboard Gameboard;
struct Player
{
    int sockfd, win_amt, lose_amt;
    char *account, *password;
    Player *next;
};

struct Gameboard
{
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
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);


    printf("Creating socket...\n");
    SOCKET socket_listen,socket_source;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);


    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;
    SOCKET maxi = -1;
    int i, nready;
    ssize_t recvlen;
    char buf[BUFSIZE];

    printf("Waiting for connections...\n");
    
    SOCKET client[FD_SETSIZE];
    for (i=0; i<FD_SETSIZE; i++){
        client[i] = -1;
    }

    while(1) {
        fd_set reads;
        reads = master;
        if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }
        // new client connection
        if (FD_ISSET(socket_listen, &reads)) 
        {
            struct sockaddr_storage client_address;
            socklen_t client_len = sizeof(client_address);
            SOCKET socket_client = accept(socket_listen,
            (struct sockaddr*) &client_address,&client_len);
            
            if (!ISVALIDSOCKET(socket_client)) {
                fprintf(stderr, "accept() failed. (%d)\n",GETSOCKETERRNO());
                exit(1);
            }

            for (i=0; i<FD_SETSIZE; i++){
                if (client[i] < 0){
                    client[i] = socket_client; // save descriptor 
                    break;
                }
            }

            if (i == FD_SETSIZE)
            {
                perror("too many connection.\n");
                exit(1);
            }

            FD_SET(socket_client, &master);
            if (socket_client > max_socket)
                max_socket = socket_client;
            if (i > maxi)
                maxi = i;
            if (--nready <= 0)
                continue;
        } //if

        for (i=0; i<=maxi; i++)
        {
            if ((socket_source = client[i]) < 0)
                continue;
            if (FD_ISSET(socket_source, &reads))
            { // deal with clients
                

                if ((recvlen = recv(socket_source, buf, BUFSIZE, 0)) > 0)
                {
                    buf[recvlen] = '\0';
                    char arg1[200] = {'\0'}, arg2[200] ={'\0'}, arg3[200] = {'\0'};
                    sscanf(buf, "%s %s %s", arg1, arg2, arg3);
                    memset(buf, '\0', BUFSIZE);
                    // create account
                    if (strcmp(arg1, "create") == 0){
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
                        else{
                            while (cur != NULL){
                                pre = cur;
                                cur = cur->next;
                            }
                            cur = new_player;
                            pre->next = new_player;
                        }
                        
                        printf("Success create %s.\n",new_player->account);
                        strcpy(buf, "Hello, ");
                        strcat(buf, arg2);
                        strcat(buf, "!");
                        strcat(buf, " We are happy for your join!");
                        send(socket_source, buf, sizeof(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }
                    // login
                    else if (strcmp(arg1, "login") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        Player *temp;
                        temp = players;
                        while (temp != NULL)
                        {
                            if (strcmp(temp->account, arg2) == 0)
                            {
                                if (strcmp(temp->password, arg3) == 0 && temp->sockfd == -1)
                                {
                                    printf("%s has just logged in!\n", temp->account);
                                    strcpy(buf, "Hello, ");
                                    strcat(buf, arg2);
                                    strcat(buf, "!");
                                    temp->sockfd = socket_source;                                    
                                    send(socket_source, buf, sizeof(buf), 0);
                                }
                                else if (temp->sockfd != -1)
                                {
                                    strcpy(buf, "the account has been logged!");
                                    send(socket_source, buf, sizeof(buf), 0);
                                }
                                else
                                {
                                    strcpy(buf, "Wrong password !\nPlease login again.");
                                    send(socket_source, buf, sizeof(buf), 0);
                                }
                                break;
                            } // if
                            temp = temp->next;
                        } // while
                        if (temp == NULL)
                        {
                            strcpy(buf, "account is not existed.");
                            send(socket_source, buf, sizeof(buf), 0);
                        }
                        memset(buf, '\0', BUFSIZE);
                    } // if
                    
                    // list players
                    else if (strcmp(arg1, "list") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        Player *temp = players;
                        while (temp != NULL)
                        {
                            if (temp->sockfd != -1)
                            {
                                strcat(buf, temp->account);
                                strcat(buf, ", ");
                            }
                            temp = temp->next;
                        }
                        send(socket_source, buf, sizeof(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }

                    // invite player
                    else if (strcmp(arg1, "invite") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        int dest_fd;
                        Player *src, *dest;
                        src = players;
                        dest = players;
                        Gameboard *srcb, *destb;
                        // find the destination name
                        while (dest != NULL)
                        {
                            if ((strcmp(dest->account, arg2) != 0) || (dest->sockfd == -1))
                            {
                                dest = dest->next;
                            }
                            else
                            {
                                // find the source name
                                while (src != NULL)
                                {
                                    if (src->sockfd != socket_source)
                                    {
                                        src = src->next;
                                    }
                                    else
                                    {
                                        strcpy(buf, src->account);
                                        strcat(buf, " has challenged you.\n\nAccept challenge with "
                                                        "usage : accept {challenger}");
                                        send(dest->sockfd, buf, sizeof(buf), 0);
                                        break;
                                    }
                                } // while
                                if (src == NULL)
                                {
                                    fprintf(stderr, "failed to find the source sockfd.\n");
                                    break;
                                }
                                break;
                            } // else
                        } // while
                        if (dest == NULL)
                        {
                            sprintf(buf, "player %s is not existed.", arg2);
                            send(socket_source, buf, sizeof(buf), 0);
                        }
                        memset(buf, '\0', BUFSIZE);
                    } // else if invite

                    // accept game invitation
                    else if (strcmp(arg1, "accept") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        Player *src, *dest;
                        src = players;
                        dest = players;
                        while (dest != NULL)
                        {
                            if (strcmp(dest->account, arg2) != 0)
                            {
                                dest = dest->next;
                            }
                            else
                            {
                                // get the source account
                                while (src != NULL)
                                {
                                    if (socket_source != src->sockfd)
                                    {
                                        src = src->next;
                                    }
                                    else
                                    {
                                        // set the game
                                        Gameboard *new_board, *cur, *pre;
                                        cur = boards;
                                        pre = boards;
                                        new_board = (Gameboard *)malloc(sizeof(Gameboard));
                                        if (boards == NULL)
                                        {
                                            boards = new_board;
                                        }
                                        else
                                        {
                                            while(cur != NULL)
                                            {
                                                pre = cur;
                                                cur = cur->next;
                                            }
                                            cur = new_board;
                                            pre->next = new_board;
                                        } // else
                                        if (socket_source < dest->sockfd)
                                        {
                                            new_board->src_fd = socket_source;
                                            new_board->dest_fd = dest->sockfd;
                                        }
                                        else
                                        {
                                            new_board->src_fd = dest->sockfd;
                                            new_board->dest_fd = socket_source;
                                        }
                                        
                                        for (char x='0'; x<'9'; x++)
                                        {
                                            new_board->board[x-'0'] = x;
                                        }
                                        new_board->next = NULL;

                                        printf("%s and %s start to play a game.\n",src->account,dest->account);
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
                                } // while
                                if (src == NULL)
                                {
                                    fprintf(stderr, "failed to find the source sockfd.\n");
                                    break;
                                }
                                break;
                            } // else
                        } // while
                        if (dest == NULL)
                        {
                            sprintf(buf, "account %s is not existed.", arg2);
                            send(socket_source, buf, sizeof(buf), 0);
                            memset(buf, '\0', BUFSIZE);
                        }
                        memset(buf, '\0', BUFSIZE);
                    } // else if accept

                    // set the position
                    else if (strcmp(arg1, "set") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        Player *src, *dest;
                        src = players; dest = players;
                        Gameboard *board, *pre_board;
                        board = boards;
                        pre_board = boards;
                        // find out the destination sockfd
                        while (dest != NULL)
                        {
                            if (strcmp(dest->account, arg3) != 0)
                            {
                                dest = dest->next;
                            }
                            else
                            {
                                while (((board->src_fd != socket_source) || (board->dest_fd != dest->sockfd)) 
                                        && ((board->src_fd != dest->sockfd) || (board->dest_fd != socket_source)) 
                                        && board != NULL)
                                {
                                    pre_board = board;
                                    board = board->next;
                                }
                                // check for the existence of the game
                                if (board == NULL)
                                {
                                    sprintf(buf, "No game is existed between you and %s\n\n", arg3);
                                    strcat(buf, "try usage : invite {player} to invite the player!");
                                    send(socket_source, buf, sizeof(buf), 0);
                                    memset(buf, '\0', BUFSIZE);
                                }
                                else
                                {
                                    // check if the cell has been set
                                    if(board->board[atoi(arg2)] == 'O' || board->board[atoi(arg2)] == 'X')
                                    {
                                        for (int j=0; j<9; j++)
                                        {
                                            printf("board->board[%d] is %c\n", j, board->board[j]);
                                        }
                                        strcpy(buf, "the position you select has been set!");
                                        send(socket_source, buf, sizeof(buf), 0);
                                        memset(buf, '\0', BUFSIZE);
                                    }
                                    else
                                    {
                                        // the smaller sockfd will be O, else be X
                                        if (board->src_fd == socket_source)
                                        {
                                            board->board[atoi(arg2)] = 'O';
                                        }
                                        else
                                        {
                                            board->board[atoi(arg2)] = 'X';
                                        }
                                        // get the source name
                                        while (src != NULL)
                                        {
                                            if (socket_source != src->sockfd)
                                            {
                                                src = src->next;
                                            }
                                            else
                                            {
                                                break;
                                            }
                                        }
                                        if (src == NULL)
                                        {
                                            fprintf(stderr, "failed to get the src name while setting\n");
                                            strcpy(buf, "system error\ntry to type instruction again!");
                                            send(socket_source, buf, sizeof(buf), 0);
                                            memset(buf, '\0', BUFSIZE);
                                        }
                                        else
                                        {
                                            int winner = -1; // 0 if src win, else 1
                                            // check for diagonal winning lines
                                            if (((board->board[0] == board->board[4]) && (board->board[0] == board->board[8])) ||
                                                ((board->board[2] == board->board[4]) && (board->board[2] == board->board[6])))
                                            {
                                                if ( board->board[0] == 'O')
                                                {
                                                    winner = 0;
                                                } // if
                                                else
                                                {
                                                    winner = 1;
                                                } // else
                                            } // check for rows and columns winning lines
                                            for (int idx=0; idx<3; idx++)
                                            {
                                                // rows
                                                if ((board->board[0+idx] == board->board[3+idx]) && (board->board[0+idx] == board->board[6+idx]))
                                                {
                                                    if (board->board[0+idx] == 'O')
                                                    {
                                                        winner = 0;
                                                    }
                                                    else
                                                    {
                                                        winner = 1;
                                                    }
                                                    break;
                                                }
                                                // columns
                                                else if (((board->board[0+3*idx] == board->board[1+3*idx]) && (board->board[0+3*idx] == board->board[2+3*idx])))
                                                {
                                                    if (board->board[0+3*idx] == 'O')
                                                    {
                                                        winner = 0;
                                                    }
                                                    else
                                                    {
                                                        winner = 1;
                                                    }
                                                    break;
                                                }
                                            }

                                            if (winner == -1)
                                            {
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
                                            else
                                            {
                                                if (winner = 0)
                                                {
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
                                                else
                                                {
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
                                                strcat(buf, "\ninvite someone to play another game!\n\n");
                                                strcat(buf, " Usage: \n (1) create {account} {password} -- Create the account \n");
                                                strcat(buf, " (2) login {account} {password}  -- User login\n");
                                                strcat(buf, " (3) list  -- list online user\n");
                                                strcat(buf, " (4) invite {username} --Invite someone to play with you\n" );
                                                strcat(buf, " (5) watch {username} --Watch other's game \n");
                                                strcat(buf, " (6) send {username} {message} --Send a message to other player \n");
                                                strcat(buf, " (7) game list --view player who is in the game\n");
                                                strcat(buf, " (8) performance --look your performance\n");
                                                strcat(buf, " (9) logout \n\n");
                                                
                                                send(src->sockfd, buf, sizeof(buf), 0);
                                                send(dest->sockfd, buf, sizeof(buf), 0);
                                                memset(buf, '\0', BUFSIZE);
                                                pre_board->next = board->next;
                                                free(board);
                                            }
                                        } // else (src != NULL)
                                    } // else (board->board[atoi(arg2)] != 'O' || 'X')
                                } // else (board != NULL)
                                break;
                            } // else (strcmp(dest->account, arg3))
                        } // while (dest != NULL)
                        if (dest == NULL)
                        {
                            sprintf(buf, "NO player %s exists.", arg3);
                            send(socket_source, buf, sizeof(buf), 0);
                            memset(buf, '\0', BUFSIZE);
                        }
                        memset(buf, '\0', BUFSIZE);
                    } // else if (set)
                    
                    // logout 
                    else if (strcmp(arg1, "logout") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        Player *temp = players;
                        while (temp != NULL)
                        {
                            if (temp->sockfd == socket_source)
                            {
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
                        {
                            strcpy(buf, "You should login first!");
                        }
                        send(socket_source, buf, strlen(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }

                    
                    // view the list of gaming players
                    else if (strcmp(arg1, "game") == 0 && strcmp(arg2, "list") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        Gameboard *board = boards;
                        strcpy(buf, "Gaming list :");
                        // traverse the boards
                        while (board != NULL)
                        {
                            Player *p1, *p2;
                            p1 = players; p2 = players;
                            // get first player's name
                            while (p1 != NULL)
                            {
                                if (p1->sockfd == board->src_fd)
                                {
                                    break;
                                }
                                p1 = p1->next;
                            }
                            // get second player's name
                            while (p2 != NULL)
                            {
                                if (p2->sockfd == board->dest_fd)
                                {
                                    break;
                                }
                                p2 = p2->next;
                            }
                            char temp[128] = {'\0'};
                            sprintf(temp, "\n {%s, %s}", p1->account, p2->account);
                            strcat(buf, temp);
                            board = board->next;
                        }
                        if (boards == NULL)
                        {
                            strcpy(buf, "No players are gaming!");
                        }
                        send(socket_source, buf, strlen(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    } // else if view gaming list

                    // Watch the specific gameboard position
                    else if (strcmp(arg1, "watch") == 0)
                    {
                        memset(buf, '\0', BUFSIZE);
                        Player *p1, *p2, *tra_p;
                        tra_p = players;
                        Gameboard *board = boards;
                        // get the sockfd of players
                        while (tra_p != NULL)
                        {
                            if (strcmp(tra_p->account, arg2) == 0)
                            {
                                p1 = tra_p;
                            }
                            if (strcmp(arg3, tra_p->account) == 0)
                            {
                                p2 = tra_p;
                            }
                            tra_p = tra_p->next;
                        }
                        // get the board
                        while (board != NULL)
                        {
                            if (((board->src_fd == p1->sockfd) && (board->dest_fd == p2->sockfd)) || 
                                ((board->src_fd == p2->sockfd) && (board->dest_fd == p1->sockfd)))
                            {
                                sprintf(buf, " \nthe gameboard between %s and %s now :\n\n"
                                            " %c | %c | %c \n"
                                            "---+---+---\n"
                                            " %c | %c | %c \n"
                                            "---+---+---\n"
                                            " %c | %c | %c \n"
                                            , p1->account, p2->account
                                            , board->board[0], board->board[1], board->board[2]
                                            , board->board[3], board->board[4], board->board[5]
                                            , board->board[6], board->board[7], board->board[8]);
                                send(socket_source, buf, strlen(buf), 0);
                                memset(buf, '\0', BUFSIZE);
                                break;
                            }
                            board = board->next;
                        }
                        if (board == NULL)
                        {
                            strcpy(buf, "No gaming existed!");
                            send(socket_source, buf, strlen(buf), 0);
                            memset(buf, '\0', BUFSIZE);
                        }
                    }

                    // show the performance
                    else if (strcmp(arg1, "performance") == 0){
                        memset(buf, '\0', BUFSIZE);
                        Player *player = players;
                        while (player != NULL)
                        {
                            if (player->sockfd == socket_source)
                            {
                                sprintf(buf, "Hello %s, here's your performance!\n"
                                            " win :  %d\n lose : %d"
                                            , player->account, player->win_amt, player->lose_amt);
                                break;
                            }
                            player = player->next;
                        }
                        if (player == NULL)
                        {
                            fprintf(stderr, "match the player failed with instuction performance!\n");
                            sprintf(buf, "server failed to match the player!");
                        }
                        send(socket_source, buf, strlen(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }

                    else if(strcmp(arg1,"send")==0)
                    {
                        
                        memset(buf, '\0', BUFSIZE);
                        Player *src, *dest;
                        src = players;
                        dest = players;
                        
                        if(arg2 == NULL){
                             strcpy(buf,"Usage:\n send {username} {message}\n");
                             strcat(buf,"You miss the person who you want to send message!\n ");
                             send(socket_source,buf,strlen(buf),0);
                             memset(buf, '\0', BUFSIZE);
                        }
                        else{
                            while (dest != NULL)
                            {
                                if (strcmp(dest->account, arg2) != 0)
                                    dest = dest->next;
                            
                                else{
                                    // get the source account
                                    while (src != NULL){
                                        if (socket_source != src->sockfd)
                                            src = src->next;
                                    
                                        else{
                                            if(arg3 == NULL){
                                                strcpy(buf,"You don't type in any messages!!\n");
                                                send(src->sockfd,buf,strlen(buf),0);
                                                memset(buf, '\0', BUFSIZE);
                                            }
                                            else{
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
                                                
                                            }
                                            break;
                                        }
                                    }
                                    break;
                                }
                            
                            }
                        }
                    }
                    // exception response
                    else
                    {
                        strcpy(buf, " Usage: \n (1) create {account} {password} -- Create the account \n");
                        strcat(buf, " (2) login {account} {password}  -- User login\n");
                        strcat(buf, " (3) list  -- list online user\n");
                        strcat(buf, " (4) invite {username} --Invite someone to play with you\n" );
                        strcat(buf, " (5) watch {username} --Watch other's game \n");
                        strcat(buf, " (6) send {username} {message} --Send a message to other player \n");
                        strcat(buf, " (7) game list --view player who is in the game\n");
                        strcat(buf, " (8) performance --look your performance\n");
                        strcat(buf, " (9) logout \n\n");
                        send(socket_source, buf, sizeof(buf), 0);
                        memset(buf, '\0', BUFSIZE);
                    }
                    bzero(buf, sizeof(buf));
                } // if recv()
                else
                {
                    printf("close the sock %d\n", socket_source);
                    Player *temp = players;
                    while (temp != NULL)
                    {
                        if (temp->sockfd == socket_source)
                        {
                            temp->sockfd = -1;
                            break;
                        }
                        temp = temp->next;
                    }
                    if (temp == NULL)
                    {
                        fprintf(stderr, "socket %d logged out failed!\n", socket_source);
                    }
                    close(socket_source);
                    FD_CLR(socket_source, &master);
                    client[i] = -1;
                }
                if (--nready <= 0)
                    break;
            } // if
        } // for i to max socket
    } // while(1)



    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);



    printf("Finished.\n");

    return 0;
}

