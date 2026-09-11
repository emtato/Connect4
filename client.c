#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/select.h>

#include "board.h"
#include "player.h"

#ifndef PORT
#define PORT XXXXX
#endif

#define MAX_BUF 128
#define MAX_QUEUE 2

void read_message_from_server(int soc, char *msg);

int connect_to_server();

void set_block(int socket);

// CLIENT GAME LOOP -----------------------------------------------------------------------------------------------
int main() {
    int soc = connect_to_server();
    char msg[MAX_BUF];

    //game stuff
    char name[17];
    Board board;
    init_board(&board);
    int game_started = 0;

    // name section
    // raed initial message from server
    read_message_from_server(soc, msg);
    // case for if the game is full
    int spectating = 0;
    if (strstr(msg, "Game is full! Rejecting connection") != NULL) {
        printf("%s\n", msg);
        return 0;
    }
    if (strstr(msg, "spectating") != NULL) {
        spectating = 1;
    }

    printf("%s\n", msg);
    if (!spectating) {
        // get name from stdin, with error handling
        if (fgets(name, sizeof(name), stdin) == NULL) {
            perror("client: fgets");
            exit(1);
        }

        name[strcspn(name, "\n")] = '\0';
        char send_name[20];
        snprintf(send_name, sizeof(send_name), "%s\r\n", name);
        if (write(soc, send_name, strlen(send_name)) != (ssize_t) strlen(send_name)) {
            perror("client: write");
            exit(1);
        }
        printf("--- Player name: %s\n", name);
    }

    // client handling (named)
    while (1) {
        // set up socket for client side
        // initialize select
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(soc, &read_fds);
        int sin = fileno(stdin);
        FD_SET(sin, &read_fds);

        // store highest fd
        int max_fd = soc;
        if (sin > soc) {
            max_fd = sin;
        }
        //call select w/handling
        int sel = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        // select sets any fd in read_fds to active (like readable)
        if (sel < 0) {
            perror("server: select");
            exit(1);
        }

        if (!spectating) {
            // if the server has sent a message to read
            if (FD_ISSET(soc, &read_fds)) {
                // check if the game has started or not
                if (game_started == 0) {
                    // read whatever the server has sent --> waiting for other player or match found
                    read_message_from_server(soc, msg);
                    printf("%s\n", msg);

                    // if the msg contains game start, set game_started = 1
                    if (strstr(msg, "Game Start!") != NULL) {
                        game_started = 1;
                        // flush the input, so whatever the client inputted while waiting for player is erased
                        fflush(stdin);
                    }
                }
                // play a round
                else {
                    read_message_from_server(soc, msg);
                    // if the message is a board --> approriate action
                    if ((strstr(msg, ".") != NULL) || (strstr(msg, "x") != NULL && strstr(msg, "o") != NULL)) {
                        board = convert_msg_to_board(msg);
                        print_board(&board);
                    }
                    // else if the message is an instruction, update isturn --> act accordingly
                    else {
                        printf("%s\n", msg);
                    }
                }
            }

            if (FD_ISSET(sin, &read_fds)) {
                // handle it - my turn baby
                char move[MAX_BUF - 2];
                // get move from stdin, with error handling
                if (fgets(move, sizeof(move), stdin) == NULL) {
                    perror("client: fgets");
                    exit(1);
                }
                move[strcspn(move, "\n")] = '\0';
                char send_move[MAX_BUF];
                snprintf(send_move, sizeof(send_move), "%s\r\n", move);
                //send the move to server
                if (write(soc, send_move, strlen(send_move)) != (ssize_t) strlen(send_move)) {
                    perror("client: write");
                    exit(1);
                }
            }
        } 
        else { //we're spectating lil bro
            if(FD_ISSET(soc, &read_fds)){
                read_message_from_server(soc, msg);
                // if the message is a board --> approriate action
                if ((strstr(msg, ".") != NULL) || (strstr(msg, "x") != NULL && strstr(msg, "o") != NULL)) {
                    board = convert_msg_to_board(msg);
                    print_board(&board);
                }
                else if (strstr(msg, "chosen to quit") != NULL){
                    break;
                }   
                else{
                    printf("%s\n", msg);
                }

            }

            // check for 'quit' command + send it over
            if (FD_ISSET(sin, &read_fds)) {
                // handle it - my turn baby
                char move[MAX_BUF - 2];
                // get move from stdin, with error handling
                if (fgets(move, sizeof(move), stdin) == NULL) {
                    perror("client: fgets");
                    exit(1);
                }
                move[strcspn(move, "\n")] = '\0';
                char send_move[MAX_BUF];
                snprintf(send_move, sizeof(send_move), "%s\r\n", move);
                //send the move to server
                if (write(soc, send_move, strlen(send_move)) != (ssize_t) strlen(send_move)) {
                    perror("client: write");
                    exit(1);
                }
            }
        }
    }
    close(soc);
    return 0;
}

// main client side functions ----------------------------------------------------------------------------
void read_message_from_server(int soc, char *msg) {
    int total = 0;

    while (1) {
        ssize_t n = read(soc, &msg[total], 1);
        if (n < 0) {
            perror("read_from_socket: read");
            exit(1);
        }
        if (n == 0) {
            fprintf(stderr, "client: server disconnected\n");
            exit(1);
        }

        total += (int) n;

        if (total >= MAX_BUF - 1) {
            break;
        }

        if (total >= 2 && msg[total - 2] == '\r' && msg[total - 1] == '\n') {
            break;
        }
    }

    msg[total] = '\0';

    if (total >= 2) {
        msg[total - 2] = '\0';
    }
}

int connect_to_server() {
    // create client socket
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    // error check
    if (soc < 0) {
        perror("client: socket");
        exit(1);
    }

    // intialize server address
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    memset(&server.sin_zero, 0, 8);

    struct addrinfo *ai;
    char *hostname = "XXXXX";

    // declares memory and populates ailist
    getaddrinfo(hostname, NULL, NULL, &ai);
    server.sin_addr = ((struct sockaddr_in *) ai->ai_addr)->sin_addr;

    // free the memory that was allocated by getaddrinfo for this list
    freeaddrinfo(ai);

    int ret = connect(soc, (struct sockaddr *) &server, sizeof(struct sockaddr_in));
    if (ret < 0) {
        perror("client: connect");
        exit(1);
    }

    return soc;
}
