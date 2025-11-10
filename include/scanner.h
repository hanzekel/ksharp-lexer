#ifndef SCANNER_H
#define SCANNER_H

#include "lexer.h"
#include "charclass.h"
#include "cursor.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>


Token scan_identifier_or_keyword(Lexer L);
Token scan_number(Lexer L);
Token scan_char(Lexer L);
Token scan_operator(Lexer L);
Token scan_comment(Lexer L);
Token is_invalid(Lexer L);

#endif