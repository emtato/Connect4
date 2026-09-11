//
// Created by Emilia Ma on 2026-03-26.
//
#include "board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_board(Board *b) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            b->cells[r][c] = '.';
        }
    }
    for (int c = 0; c < COLS; c++) {
        b->maxcol[c] = 0;
    }
}

void print_board(const Board *b) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            printf("%c ", b->cells[r][c]);
        }
        printf("\n");
    }
    for (int c = 0; c < COLS; c++) {
        printf("%d ", c + 1);
    }
    printf("\n");
}

// returns 1 if its a valid move, 0 else
int is_valid_move(const Board *b, int col) {
    if (col < 0 || col >= COLS) return 0;
    if (b->maxcol[col] >= ROWS) {
        return 0;
    }
    return 1;
}

int play_move(Board *b, int col, char piece) {
    int row = ROWS - 1 - b->maxcol[col];
    //calculatnif positin to place piece according to how many are already in this column alr
    b->cells[row][col] = piece;
    b->maxcol[col]++;
    return check_winner(b, row, col, piece);
}

//check winner is called after the move is played
//return 0 if no win, 1 if win, 2 if tie
int check_winner(const Board *b, int row, int col, char piece) {
    //directions:
    /*
     *  0 1 2
     *  3   4
     *  5 6 7
     **/

    //check for tie
    int allmax= 1;
    for (int r = 0; r < COLS; r++) {
        if (b->maxcol[r] < ROWS) allmax = 0;
    }
    if (allmax) {
        return 2;
    }
    int max = 0;
    for (int i = 0; i < 4; i++) {
        int result = recursive_check(b, col, row, piece, 0, i);
        result += recursive_check(b, col, row, piece, 0, 7 - i);
        result++;
        if (result > max) max = result;
        if (max > 3) return 1;
    }

    return 0;
}

int check_grid(const Board *b, int row, int col, char piece) {
    if (row < 0 || col < 0 || row >= ROWS || col >= COLS) return 0;
    if (b->cells[row][col] == piece) return 1; // simplified
    return 0;
}


char *convert_board_to_msg(Board *b) {
    char *msg = malloc(sizeof(char) * 110);
    if (msg == NULL) {
        perror("malloc");
        exit(1);
    }
    msg[0] = '\0';

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            int len = strlen(msg);
            msg[len] = b->cells[i][j];
            msg[len + 1] = ' ';
            msg[len + 2] = '\0';
        }
    }
    strcat(msg, "\r\n");
    return msg;
}

Board convert_msg_to_board(char *msg) {
    Board board;
    init_board(&board);
    char *p = msg;
    //
    // char *p2 = strstr(msg, "\n");
    // int len = p2 - msg;
    // strncpy(board.text, msg, len);


    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            // skip spaces or newlines
            while (*p == ' ' || *p == '\n' || *p == '\r') p++;

            board.cells[i][j] = *p;
            p++;

            // skip spaces after char
            while (*p == ' ') p++;
        }
    }

    return board;
}


//returns 0 if theres no 4 in a row, 1 if there is
int recursive_check(const Board *b, int col, int row, char piece, int length, int direction) {
    int new_row = row;
    int new_col = col;

    if (direction == 0) {
        new_row = row - 1;
        new_col = col - 1;
    } else if (direction == 1) {
        new_row = row - 1;
        new_col = col;
    } else if (direction == 2) {
        new_row = row - 1;
        new_col = col + 1;
    } else if (direction == 3) {
        new_row = row;
        new_col = col - 1;
    } else if (direction == 4) {
        new_row = row;
        new_col = col + 1;
    } else if (direction == 5) {
        new_row = row + 1;
        new_col = col - 1;
    } else if (direction == 6) {
        new_row = row + 1;
        new_col = col;
    } else if (direction == 7) {
        new_row = row + 1;
        new_col = col + 1;
    }

    if (check_grid(b, new_row, new_col, piece) > 0) {
        return recursive_check(b, new_col, new_row, piece, length + 1, direction);
    }

    return length;
}
