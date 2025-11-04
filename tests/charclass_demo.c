#include <stdio.h>
#include "C:\DevC\include\charclass.h"

int main(void) {
    char test[] = {'A', 'z', '_', '9', '+', ' ', '@', '"', '{', 0};
    for (int i = 0; test[i] != 0; i++) {
        char c = test[i];
        printf("'%c': ", c);
        if (is_alpha(c))        printf("Letter\n");
        else if (is_digit(c))   printf("Digit\n");
        else if (is_underscore(c)) printf("Underscore\n");
        else if (is_whitespace(c)) printf("Whitespace\n");
        else if (is_symbol_char(c)) printf("Symbol\n");
        else if (is_operator_char(c)) printf("Operator\n");
        else if (is_quote(c))   printf("Quote\n");
        else if (is_escape(c))  printf("Escape\n");
        else if (is_prefix(c))  printf("Prefix\n");
        else printf("Unknown\n");
    }
    return 0;
}

