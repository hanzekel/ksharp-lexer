#ifndef CHARCLASS_H
#define CHARCLASS_H

int is_alpha(int c);
int is_digit(int c);
int is_underscore(int c);
int is_whitespace(int c);
int is_symbol_char(int c);
int is_operator_char(int c);
int is_quote(int c);
int is_escape(int c);
int is_prefix(int c);

#endif

