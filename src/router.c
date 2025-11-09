#include<stdio.h>
#include <stddef.h>
#include "charclass.h"
#include "lexer.h"

Token next_token(struct Lexer* L) {
    skip_ws(L);
    int c = peek (L);
    if (c== EOF) 
        return make (L, TOK_EOF, NULL, 0, NULL);

    if (c== '"') { 
        advance(L); 
        return make(L, TOK_INVALID, "<string_not_alloved>", 20, NULL);}

    if (is_delim (c)) {int ch=advance(L): char s2[2] = {ch, o}; return make(L, TOK_DELIM, s2, 1, s2);}

    if (is_alpha(c)|| c == '_')
        return scan_identifier_or_keyword  (L);

    if (is_digit(c))
        return scan_number(L);

    if (c == '\'')
        return scan_char(L);

    if (c == '/')
        return scan_slash_comment_or_op(L);

    if (c == '*' || c =='+' || c == '-' || c == '%' || c == '=' || c == '<' || c == '>' || c == '!' || c == '&' || c == '|')
            return scan_operator(L);

    advance(L); char s2[2] = {(char)c, 0}; return make(L, TOK_INVALID, s2, 1, NULL);
}
