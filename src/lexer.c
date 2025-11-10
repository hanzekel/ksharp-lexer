#include "lexer.h"
#include "charclass.h"
#include <ctype.h>

int peek(Lexer* L) {
    return L->current_char;
}

int advance(Lexer* L) {
    int current = L->current_char;
    
    if (current == EOF) return EOF;
    
    if (current == '\n') {
        L->line++;
        L->column = 1;
    } else {
        L->column++;
    }
    
    L->current_char = fgetc(L->input);
    return current;
}

int match(Lexer* L, int expected) {
    if (L->current_char == expected) {
        advance(L);
        return 1;
    }
    return 0;
}

void skip_ws(Lexer* L) {
    while (L->current_char == ' ' || L->current_char == '\t' || 
           L->current_char == '\n' || L->current_char == '\r') {
        advance(L);
    }
}
