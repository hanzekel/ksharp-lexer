#include "category_checks.h"

int is_delim(int c) {
    // Delimiters in K#: ; , : .
    return (c == ';' || c == ',' || c == ':' || c == '.');
}

int is_bracket(int c) {
    // Brackets and braces in K#: () {} []
    return (c == '(' || c == ')' ||
            c == '{' || c == '}' ||
            c == '[' || c == ']');
}=