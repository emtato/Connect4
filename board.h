//
// Created by Emilia Ma on 2026-03-26.

#ifndef BOARD_H
#define BOARD_H

#define ROWS 6
#define COLS 7
#define EMPTY '.'

typedef struct {
    char cells[ROWS][COLS];
    int maxcol[COLS];
    int numMoves; //if this equals ROW*COL, boar full
    char text[50];
} Board;

void init_board(Board *b);
void print_board(const Board *b);
int is_valid_move(const Board *b, int col);
int play_move(Board *b, int col, char piece);
int check_winner(const Board *b, int row, int col, char piece);
int recursive_check(const Board *b, int col, int row, char piece, int length, int direction);
int check_grid(const Board *b, int row, int col, char piece) ;
Board convert_msg_to_board(char* msg);
char* convert_board_to_msg(Board *b);

#endif