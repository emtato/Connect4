//
// Created by Emilia Ma on 2026-03-25.
//
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#include "board.h"
#include "player.h"

#ifndef PORT
#define PORT XXXX
#endif

#define MAX_BUF 128
#define MAX_QUEUE 3

#define HAS_SPEC (connections[2] >= 0 ? 1 : 0)

// SERVER SOCKET FUNCTIONS ===================================================
/*
    Write msg to players with socket descriptors p1, p2, p3
    if spec is 1, that means we have a spectator and we should also write to p3. otherwise, we can call func with spec, p3 = 0
*/
void write_to_all(char *msg, int p1, int p2, int p3, int spec) {
    if (write(p1, msg, strlen(msg)) != (ssize_t) strlen(msg)) {
        perror("main: write_to_all");
//printf("35\n");
        exit(1);
    }
    if (write(p2, msg, strlen(msg)) != (ssize_t) strlen(msg)) {
        perror("main: write_to_all");
//printf("40\n");
        exit(1);
    }
    if (spec) {
        if (write(p3, msg, strlen(msg)) != (ssize_t) strlen(msg)) {
            perror("main: write_to_all");
//printf("46\n");
            exit(1);
        }
    }
}

void write_to_one(char *msg, int p) {
    if (write(p, msg, strlen(msg)) != (ssize_t) strlen(msg)) {
        perror("main: write to one");
//printf("55\n");
        exit(1);
    }
}

/*
    Read from given socket, and return a pointer to a string that stores what's read
*/
void read_from_socket(int socket, char *msg, int connections[]) {
    // ensure the client's ENTIRE message is read
    int bytes_read;
    // error handling w/reading
    if ((bytes_read = read(socket, msg, MAX_BUF - 1)) < 0) {
        perror("read_from_socket: read");
        exit(1);
    }
    if (bytes_read == 0) {
        // if our spectator died
        if(connections[2] == socket){
            close(socket);
            connections[2] = -1;
            return;
        }
        else if(connections[1] == socket){
            // remove the dead client
            close(socket);
            connections[1] = -1;
            return;
            
        }
        else if(connections[0] == socket){
             // remove the dead client
            close(socket);
            connections[0] = -1;
            return;
        }
    }
    msg[bytes_read] = '\0';

    // read data until the it contains \r\n = end of message
    while (strstr(msg, "\r\n") == NULL) {
        int num = read(socket, &msg[bytes_read], MAX_BUF - bytes_read);
        if ((bytes_read += num) < 0) {
            perror("read_from_socket: read");
            exit(1);
        }
        if (num == 0) {
            // if our spectator died
        if(connections[2] == socket){
            close(socket);
            connections[2] = -1;
            return;
        }
        else if(connections[1] == socket){
            // remove the dead client
            close(socket);
            connections[1] = -1;
            return;
            
        }
        else if(connections[0] == socket){
             // remove the dead client
            close(socket);
            connections[0] = -1;
            return;
        }
        }
        msg[bytes_read] = '\0';
    }
    msg[bytes_read - 2] = '\0';
}

/*
    Accept a new player and store the socket into connections
*/
void accept_player(int listen_soc, int connections[]) {
    // set up the client side ------------------------------------------
    struct sockaddr_in client_addr;
    unsigned int client_len = sizeof(struct sockaddr_in);
    client_addr.sin_family = AF_INET;

    int client_socket = accept(listen_soc, (struct sockaddr *) &client_addr, &client_len);
    if (client_socket < 0) {
        perror("accept_player: accept");
        exit(1);
    }

    //connect connection to an empty conn
    if (connections[0] < 0) {
        connections[0] = client_socket;
    } else if (connections[1] < 0) {
        connections[1] = client_socket;
    } else if (connections[2] < 0) {
        connections[2] = client_socket;
        char msg[MAX_BUF];
        sprintf(msg, "You are spectating. Waiting for game to start. If you wish to leave, type 'quit'\r\n");
        if (write(connections[2], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
            perror("main: write");
            exit(1);
        }
        return;
    }

    // Ask the player to input their player name
    char msg[MAX_BUF];
    sprintf(msg, "Welcome to Connect 4! Please input your player name.\r\n");
    // with error handling

    if (write(client_socket, msg, strlen(msg)) != (ssize_t) strlen(msg)) {
        perror("main: write");
        exit(1);
    }
}

/*
    reject a new connection and send a message to the client
*/
void reject_full(int listen_soc) {
    // set up the client side ------------------------------------------
    struct sockaddr_in client_addr;
    unsigned int client_len = sizeof(struct sockaddr_in);
    client_addr.sin_family = AF_INET;

    int client_socket = accept(listen_soc, (struct sockaddr *) &client_addr, &client_len);
    if (client_socket < 0) {
        perror("accept_player: accept");
        exit(1);
    }

    char msg[MAX_BUF];
    sprintf(msg, "Game is full! Rejecting connection\r\n");
    // with error handling
    if (write(client_socket, msg, strlen(msg)) != (ssize_t) strlen(msg)) {
        perror("main: write");
        exit(1);
    }
}

// main game loop ================================================================
int main() {
    // set up server socket --------------------------------
    int listen_soc = socket(AF_INET, SOCK_STREAM, 0);
    // error checking for socket
    if (listen_soc < 0) {
        perror("server: socket");
        exit(1);
    }

    // intialize server address
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&server.sin_zero, 0, 8);

    // let us (coders) reuse the same port --> TAKEN FROM CLASS SOCKET WORKSHEET
    int n = 1;
    int status = setsockopt(listen_soc, SOL_SOCKET, SO_REUSEADDR, (const char *) &n, sizeof(n));
    if (status < 0) {
        perror("setsockopt: REUSEADDR");
    }

    // bind the socket to an address + check for error
    if (bind(listen_soc, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) < 0) {
        perror("server: bind");
        // close the socket
        close(listen_soc);
        exit(1);
    }

    // set up a queue in the kernel to hold pending connections (2) + check for error
    if (listen(listen_soc, MAX_QUEUE) < 0) {
        perror("listen");
        exit(1);
    }

    int connections[3] = {-1, -1, -1};

    //intialize game board
    char msg[MAX_BUF];
    Board board;
    init_board(&board);
    int alternating = 0;

    Player players[3];
    int connected_players = 0;
    int named[2] = {-1, -1};
    int move_made = 0;

    Player p1;
    Player p2;
    //Player stalker;
    char pName1[MAX_BUF];
    char pName2[MAX_BUF];
    int ask_to_rejoin = 0;

    int break_win = 0;
    while (1) {
        //server is running
        // initialize select
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_soc, &read_fds);

        // store highest fd
        int max_fd = listen_soc;
        // add the clients to the fd_set
        FD_SET(connections[0], &read_fds);
        // find new highest file desc
        if (connections[0] > max_fd) {
            max_fd = connections[0];
        }
        FD_SET(connections[1], &read_fds);
        if (connections[1] > max_fd) {
            max_fd = connections[1];
        }

        // only add to read_fds if the connection exists
        if(connections[2] != -1){
            FD_SET(connections[2], &read_fds);
            if (connections[2] > max_fd) {
                max_fd = connections[2];
            }
        }
        

        //call select w/handling
        int sel = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        // select sets any fd in read_fds to active (like readable)
        if (sel < 0) {
            perror("server: select");
            exit(1);
        }

        // add a player if there is a connection waiting -----------------------------------
        if (FD_ISSET(listen_soc, &read_fds)) {
            if ((connections[2] >= 0) && connected_players == 2) {
                //modified max connected players check
                printf("rejecting connection, game full");
                // will send a reject message to client, client should quit on their side
                reject_full(listen_soc);
            } else {
                accept_player(listen_soc, connections);
            }
        }

        // Client check
        for (int i = 0; i < 2; i++) {
            //printf("checkinf for connection with %d\n", i);
            if (FD_ISSET(connections[i], &read_fds)) {
                //printf("response recorded from socket %d\n", i);
                // if connections[i] is connected, and has not been named, name it
                if (named[i] == -1) {
                    if (i == 0) {
                        read_from_socket(connections[i], pName1, connections);
                        p1.name = pName1;
                        p1.symbol = 'x';
                        players[0] = p1;
                        named[0] = 1;
                        connected_players++;
                    } else if (i == 1) {
                        read_from_socket(connections[i], pName2, connections);
                        p2.name = pName2;
                        p2.symbol = 'o';
                        players[1] = p2;
                        named[1] = 1;
                        connected_players++;
                    }

                    // if we have 2 connected players:
                    if (connected_players == 2) {
                        sprintf(msg, "Match found\r\n");
                        if (write(connections[1], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                            perror("server: write");
                            exit(1);
                        }
                        // Announce to 'game start' to all players w'intiial turn
                        sprintf(msg, "----- Game Start! %s's turn -----\r\n", players[(alternating %2)].name);
                        //if/else to write to 3 people or 2 depending on the presence of "it"
                        write_to_all(msg, connections[0], connections[1], connections[2], HAS_SPEC);

                        //send the board to all
                        char *board_text = convert_board_to_msg(&board);
                        write_to_all(board_text, connections[0], connections[1], connections[2], HAS_SPEC);

                        // write to each client - their instruction
                        snprintf(msg, MAX_BUF, "Please wait for your turn\r\n");
                        if (write(connections[((alternating+1) %2)], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                            perror("server: write");
                            exit(1);
                        }
                        snprintf(msg, MAX_BUF, "Play a move\r\n");
                        if (write(connections[(alternating %2)], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                            perror("server: write");
                            exit(1);
                        }
                    }
                    // if not
                    else {
                        sprintf(msg, "Thank you, Please wait for another player to join\r\n");
                        if (write(connections[i], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                            perror("main: write");
                            exit(1);
                        }
                    }
                }
                // connections[i] is connected, been named, and is ready to be read (by FD_ISSET)
                else if (named[i] != -1) {
//printf("attempting to read\n");
                    read_from_socket(connections[i], msg, connections);

                    // check if this guy has disconnected + updated number of connected players
                    if(connections[i] == -1){
                        connected_players--;
                        if((i == 0) && (connected_players == 1)){
                            write_to_one("Would you like to wait for someone to join? type YES to wait, anything else to quit\r\n", connections[1]);
                            ask_to_rejoin = 1;
                            named[0] = -1;
                        }
                        else if((i == 1) && (connected_players == 1)){
                            write_to_one("Would you like to wait for someone to join? type YES to wait, anything else to quit\r\n", connections[0]);
                            ask_to_rejoin = 1;
                            named[1] = -1;
                        }
                        break;
                    }

//printf("read: %s from socket %d\n", msg, i);
//printf("length of msg = %d\n", strlen(msg));
//printf("number of connected = %d\n", connected_players);
                    if (connected_players != 2) {
                        if(ask_to_rejoin == 0){
                            sprintf(msg, "Please wait for another player to join norm\r\n");
                            if (write(connections[i], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                                perror("main: write");
                                exit(1);
                            }
                        }
                        else{
                            if(strstr(msg, "YES") != NULL){
                                sprintf(msg, "Please wait for another player to join\r\n");
                                if (write(connections[i], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                                    perror("main: write");
                                    exit(1);
                                }
                                ask_to_rejoin = 0;
                            }
                            else{
                                break_win = 2;
                                break;
                            }
                        }
                    } else {
                        // main gameplay ----------------------------- (handling client responses)
                        // first check if it is client's turn (alternating % 2) + act accordingy
                        if ((alternating % 2) == 0) {
                            if (connections[i] >= 0) {
                                // if the turn = to whoever sent info
                                if (i == 0) {
                                    // check if the game is in a win state: will have recieved a response
                                    if (break_win == 1) {
                                        // play again logic
                                        if (strcmp(msg, "play again") == 0) {
                                            init_board(&board);
                                            move_made = 1;
                                            break_win = 0;
                                        } else {
                                            break_win = 2;
                                            break;
                                        }
                                    }
                                    // check if the move is a valid integer >2 to account for \n
                                    if (!isdigit((unsigned char) msg[0]) || strlen(msg) != 1) {
                                        char *response = "Please input a valid integer!\r\n";
                                        write_to_one(response, connections[0]);
                                        snprintf(msg, MAX_BUF, "Play a move\r\n");
                                        write_to_one(msg, connections[0]); //bwa
                                    }
                                    // otherwise check if valid integer range
                                    else if (!is_valid_move(&board, (strtol(msg, NULL, 10) - 1))) {
                                        char *response = "Please input a valid integer!\r\n";
                                        write_to_one(response, connections[0]);
                                        snprintf(msg, MAX_BUF, "Play a move\r\n");
                                        write_to_one(msg, connections[0]);
                                    }
                                    //  otherwise valid, update the board
                                    else {
                                        int win = play_move(&board, strtol(msg, NULL, 10) - 1, players[i].symbol);
                                        //printf("%d\n", win);
                                        // char *board_text = convert_board_to_msg(&board);
                                        // write_to_all(board_text, connections[0], connections[1], connections[2],
                                        //              HAS_SPEC);

                                        // if someone won
                                        if (win == 1) {
                                            break_win = 1;
                                            snprintf(msg, MAX_BUF, "%s has won!\r\n", players[i].name);
                                            write_to_all(msg, connections[0], connections[1], connections[2], HAS_SPEC);

                                            // send the updated board to all clients
                                            char *board_text = convert_board_to_msg(&board);
                                            write_to_all(board_text, connections[0], connections[1], connections[2], HAS_SPEC);


                                            char *response =
                                                    "Type: play again, to play another round, or anything to end game\r\n";
                                            write_to_all(response, connections[0], connections[1], connections[2], 0);
                                        } else if (win == 2) {
                                            break_win = 1;
                                            snprintf(msg, MAX_BUF, "TIED up freak!\r\n");
                                            write_to_all(msg, connections[0], connections[1], connections[2], HAS_SPEC);

                                            // send the updated board to all clients
                                            char *board_text = convert_board_to_msg(&board);
                                            write_to_all(board_text, connections[0], connections[1], connections[2], HAS_SPEC);


                                            char *response =
                                                    "Type: play again, to play another round, or anything to end game\r\n";
                                            write_to_all(response, connections[0], connections[1], connections[2], 0);
                                        }
                                        move_made = 1;
                                    }
                                }
                                // otherwise turn != who sent info, read it and send back warning
                                else if (i == 1) {
                                    if (write(connections[1], "Please wait for your turn\r\n", 28) != (ssize_t) 28) {
                                        perror("server: write");
                                        exit(1);
                                    }
                                }
                            }
                        } else if ((alternating % 2) == 1) {
                            if (connections[i] >= 0) {
                                // if the turn = to whoever sent info
                                if (i == 1) {
                                    // check if the game is in a win state: will have recieved a response
                                    if (break_win == 1) {
                                        // play again logic
                                        if (strcmp(msg, "play again") == 0) {
                                            init_board(&board);
                                            move_made = 1;
                                            break_win = 0;
                                        } else {
                                            break_win = 2;
                                            break;
                                        }
                                    }
                                    if (!isdigit((unsigned char) msg[0]) || strlen(msg) != 1) {
                                        char *response = "Please input a valid integer!\r\n";
                                        write_to_one(response, connections[1]);
                                        snprintf(msg, MAX_BUF, "Play a move\r\n");
                                        write_to_one(msg, connections[1]);
                                    }
                                    // otherwise check if valid integer range
                                    else if (!is_valid_move(&board, (strtol(msg, NULL, 10) - 1))) {
                                        char *response = "Please input a valid integer!\r\n";
                                        write_to_one(response, connections[1]);
                                        snprintf(msg, MAX_BUF, "Play a move\r\n");
                                        write_to_one(msg, connections[1]);
                                    }
                                    //  otherwise valid, update the board
                                    else {
                                        int win = play_move(&board, strtol(msg, NULL, 10) - 1, players[i].symbol);
                                        //printf("%d\n", win);
                                        // if someone won

                                        // send the updated board to all clients
                                        // char *board_text = convert_board_to_msg(&board);
                                        // write_to_all(board_text, connections[0], connections[1], connections[2],
                                        //              HAS_SPEC);
                                        if (win == 1) {
                                            break_win = 1;
                                            snprintf(msg, MAX_BUF, "%s has won!\r\n", players[i].name);
                                            write_to_all(msg, connections[0], connections[1], connections[2], HAS_SPEC);

                                            // send the updated board to all clients
                                            char *board_text = convert_board_to_msg(&board);
                                            write_to_all(board_text, connections[0], connections[1], connections[2], HAS_SPEC);


                                            char *response =
                                                    "Type: play again, to play another round, or anything to end game\r\n";
                                            write_to_all(response, connections[0], connections[1], connections[2], 0);
                                        } else if (win == 2) {
                                            break_win = 1;
                                            snprintf(msg, MAX_BUF, "tie died!\r\n");
                                            write_to_all(msg, connections[0], connections[1], connections[2], HAS_SPEC);

                                            // send the updated board to all clients
                                            char *board_text = convert_board_to_msg(&board);
                                            write_to_all(board_text, connections[0], connections[1], connections[2], HAS_SPEC);


                                            char *response =
                                                    "Type: play again, to play another round, or anything to end game\r\n";
                                            write_to_all(response, connections[0], connections[1], connections[2], 0);
                                        }
                                        move_made = 1;
                                    }
                                }

                                // otherwise turn != who sent info, read it and send back warning
                                else if (i == 0) {
                                    if (write(connections[0], "Please wait for your turn\r\n", 28) != (ssize_t) 28) {
                                        perror("server: write");
                                        exit(1);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (break_win == 1) {
                break;
            }
        }

        if (break_win == 2) {
            break;
        }

        if(FD_ISSET(connections[2], &read_fds)){
            char stalker_msg[MAX_BUF];
            read_from_socket(connections[2], stalker_msg, connections);
            if(strstr(stalker_msg, "quit")){
                write_to_one("chosen to quit\r\n", connections[2]);
                FD_CLR(connections[2], &read_fds);
                close(connections[2]);
                connections[2] = -1;
            }
            else if(connections[2] != -1){
                write_to_one("Enter a valid command!\r\n", connections[2]);
            }
            // if the read, closed the socket :3
            else if(connections[2] == -1){ 
                FD_CLR(connections[2], &read_fds);
                close(connections[2]);
                connections[2] = -1;
            }
                
            }

        // if a valid move has been made, switch to next turn
        if (move_made == 1 && break_win != 1) {
            alternating++;
            // tell who's turn it is now
            snprintf(msg, MAX_BUF, "----- %s's turn -----\r\n", players[((alternating+1) % 2)].name);
            write_to_all(msg, connections[0], connections[1], connections[2], HAS_SPEC);

            // send the updated board to all clients
            char *board_text = convert_board_to_msg(&board);
            write_to_all(board_text, connections[0], connections[1], connections[2], HAS_SPEC);

            // send correct instruction
            snprintf(msg, MAX_BUF, "Please wait for your turn\r\n");
            if (write(connections[((alternating + 1) % 2)], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                perror("server: write");
                exit(1);
            }
            snprintf(msg, MAX_BUF, "Play a move\r\n");
            if (write(connections[(alternating % 2)], msg, strlen(msg)) != (ssize_t) strlen(msg)) {
                perror("server: write");
                exit(1);
            }
            move_made = 0;
        }

        
    }

    close(listen_soc);

    return 0;
}