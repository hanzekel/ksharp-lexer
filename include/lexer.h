#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdbool.h>

typedef struct {
    FILE* input;
    int current_char;
    int line;
    int column;
} Lexer;

// Cursor functions
int peek(Lexer* L);
int advance(Lexer* L);
int match(Lexer* L, int expected);
void skip_ws(Lexer* L);

#endif
