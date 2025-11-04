#include "C:\DevC\include\charclass.h"

int is_alpha(int c) {
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

int is_digit(int c) {
    return (c >= '0' && c <= '9');
}

int is_underscore(int c) {
    return (c == '_');
}

int is_whitespace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

int is_symbol_char(int c) {
    switch (c) {
        case '(':
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
        case ';':
        case ',':
        case '.':
        case ':':
            return 1;
        default:
            return 0;
    }
}

int is_operator_char(int c) {
    switch (c) {
        case '+': case '-': case '*': case '/':
        case '%': case '=': case '<': case '>':
        case '!': case '&': case '`':
            return 1;
        default:
            return 0;
    }
}

int is_quote(int c)  { return (c == '"'); }
int is_escape(int c) { return (c == '\\'); }
int is_prefix(int c) { return (c == '$' || c == '@'); }

