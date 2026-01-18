#include <stdio.h>    /* for FILE, printf, fgets, sscanf, etc.          */
#include <stdlib.h>   /* for general utilities                          */
#include <string.h>   /* for strlen (only), no strcmp/strncmp/strstr    */
#include <ctype.h>    /* for isspace, etc.                              */

/* --------------------------------------------------------------------
   LIMITS: maximum sizes for arrays and strings
   -------------------------------------------------------------------- */
#define MAX_LEXEME  128     /* maximum length of a lexeme string        */
#define MAX_TOKENS  2000    /* maximum number of tokens in stream       */
#define MAX_LINE    512     /* maximum length of a line from the file   */

/* --------------------------------------------------------------------
   Custom string helpers (NO strcmp / strncmp / strstr)
   -------------------------------------------------------------------- */

/* str_eq:
   Returns 1 if strings a and b are exactly the same, 0 otherwise.      */
static int str_eq(const char *a, const char *b) {
    int i = 0;                              /* index for both strings    */
    while (a[i] != '\0' && b[i] != '\0') {  /* loop while both not end   */
        if (a[i] != b[i])                   /* if any char differs       */
            return 0;                       /* => not equal              */
        i++;                                /* move to next character    */
    }
    return (a[i] == '\0' && b[i] == '\0');  /* both must end together    */
}

/* str_starts_with:
   Returns 1 if s starts with prefix, 0 otherwise.                       */
static int str_starts_with(const char *s, const char *prefix) {
    int i = 0;                              /* index for prefix          */
    while (prefix[i] != '\0') {             /* check each char in prefix */
        if (s[i] == '\0')                   /* if s ends early           */
            return 0;                       /* => cannot start with it   */
        if (s[i] != prefix[i])             /* mismatch found            */
            return 0;                       /* => not starting with it   */
        i++;                                /* proceed                   */
    }
    return 1;                               /* all prefix chars matched  */
}

/* str_contains:
   Returns 1 if s contains substring sub, 0 otherwise.                   */
static int str_contains(const char *s, const char *sub) {
    if (*sub == '\0')                       /* empty substring           */
        return 1;                           /* treat as contained        */

    int i = 0;                              /* index in s                */
    while (s[i] != '\0') {                  /* scan through s            */
        int j = 0;                          /* index in sub              */
        while (s[i + j] != '\0' &&          /* compare s[i+j]..          */
               sub[j] != '\0' &&
               s[i + j] == sub[j]) {        /* while chars match         */
            j++;                            /* move to next char         */
        }
        if (sub[j] == '\0')                 /* reached end of sub        */
            return 1;                       /* => sub found in s         */
        i++;                                /* shift start in s          */
    }
    return 0;                               /* no match found            */
}

/* --------------------------------------------------------------------
   Parser token kinds that the syntax analyzer understands
   -------------------------------------------------------------------- */
typedef enum {
    PT_KEYWORD,       /* language keywords: if, while, for, print, etc.  */
    PT_IDENTIFIER,    /* user-defined names: x, total, sum1              */
    PT_TYPE,          /* data types: int, float, bool, char, void        */
    PT_INTCONST,      /* integer constants: 1, 23, 100                   */
    PT_FLOATCONST,    /* floating constants: 3.14, 0.5                   */
    PT_CHARCONST,     /* char constants: 'a', 'x'                         */
    PT_BOOLCONST,     /* boolean constants: true, false                  */
    PT_OPERATOR,      /* operators: + - * / % == && || etc.              */
    PT_SYMBOL,        /* punctuators: ; , ( ) { }                        */
    PT_COMMENT,       /* comments (ignored by syntax rules)              */
    PT_NOISE,         /* noise words (if lexer passes them)              */
    PT_EOF,           /* end-of-file marker                              */
    PT_UNKNOWN        /* anything not recognized                         */
} ParserTokKind;

/* --------------------------------------------------------------------
   One token for the parser: kind + lexeme text
   -------------------------------------------------------------------- */
typedef struct {
    ParserTokKind kind;                 /* category of the token        */
    char lexeme[MAX_LEXEME];           /* text of the token            */
} ParserToken;

/* --------------------------------------------------------------------
   Global token stream (simple array)
   -------------------------------------------------------------------- */
static ParserToken g_tokens[MAX_TOKENS];  /* array of tokens             */
static int g_tok_count = 0;               /* how many tokens are loaded  */
static int g_tok_index = 0;               /* index of current token      */

/* --------------------------------------------------------------------
   Pretty-printer helpers (for XML-like parse tree)
   -------------------------------------------------------------------- */
static int g_indent = 0;                  /* current indentation depth   */

/* print_indent:
   Prints two spaces for each indent level.                              */
static void print_indent(void) {
    for (int i = 0; i < g_indent; ++i) {  /* repeat for each indent     */
        printf("  ");                     /* two spaces                 */
    }
}

/* open_tag:
   Prints an opening XML-like tag and increases indent.                  */
static void open_tag(const char *name) {
    printf("\n");              /* NEW: visual separator before a new construct */
    print_indent();            
    printf("<%s>\n", name);    
    g_indent++;                
}

/* close_tag:
   Prints a closing XML-like tag and decreases indent.                   */
static void close_tag(const char *name) {
    g_indent--;                           
    print_indent();                       
    printf("</%s>\n", name);              
    printf("\n");              /* NEW: blank line after closing a construct */
}


/* leaf_tag:
   Prints <tag> text </tag> on one line (no nested children).            */
static void leaf_tag(const char *name, const char *text) {
    print_indent();                       /* indent                     */
    printf("<%s> %s </%s>\n", name, text, name); /* one-line element     */
}

/* --------------------------------------------------------------------
   Current token helpers
   -------------------------------------------------------------------- */

/* cur_tok:
   Returns pointer to current token.                                     */
static ParserToken *cur_tok(void) {
    if (g_tok_index >= g_tok_count) {     /* if past last token         */
        return &g_tokens[g_tok_count - 1];/* return last (EOF) token    */
    }
    return &g_tokens[g_tok_index];        /* otherwise current token    */
}

/* next_tok:
   Moves to the next token, if not already at the end.                   */
static void next_tok(void) {
    if (g_tok_index < g_tok_count - 1) {  /* ensure not beyond last     */
        g_tok_index++;                    /* advance index              */
    }
}

/* --------------------------------------------------------------------
   trim:
   Removes leading and trailing whitespace from string s in-place.
   -------------------------------------------------------------------- */
static void trim(char *s) {
    int len = (int)strlen(s);             /* total length               */
    int start = 0;                        /* index of first non-space   */

    /* trim right side: remove newline, carriage return, spaces          */
    while (len > 0 &&
           (s[len - 1] == '\n' ||
            s[len - 1] == '\r' ||
            isspace((unsigned char)s[len - 1]))) {
        s[--len] = '\0';                  /* shorten string             */
    }

    /* find first non-space from the left                                */
    while (s[start] && isspace((unsigned char)s[start])) {
        start++;                          /* skip spaces                */
    }

    /* if there were leading spaces, shift whole string left             */
    if (start > 0) {
        int i = 0;                        /* index for new position     */
        while (s[start + i] != '\0') {    /* copy until end             */
            s[i] = s[start + i];          /* move characters            */
            i++;
        }
        s[i] = '\0';                      /* terminate new string       */
    }
}

/* --------------------------------------------------------------------
   map_kind:
   Converts the lexer token-name string to a ParserTokKind.
   Uses str_eq instead of strcmp.
   -------------------------------------------------------------------- */
static ParserTokKind map_kind(const char *kind) {
    if (str_eq(kind, "keyword"))      return PT_KEYWORD;
    if (str_eq(kind, "identifier"))   return PT_IDENTIFIER;
    if (str_eq(kind, "type"))         return PT_TYPE;
    if (str_eq(kind, "const_int"))    return PT_INTCONST;
    if (str_eq(kind, "const_float"))  return PT_FLOATCONST;
    if (str_eq(kind, "const_char"))   return PT_CHARCONST;
    if (str_eq(kind, "const_bool"))   return PT_BOOLCONST;
    if (str_eq(kind, "operator"))     return PT_OPERATOR;
    if (str_eq(kind, "punctuator"))   return PT_SYMBOL;
    if (str_eq(kind, "comment"))      return PT_COMMENT;
    if (str_eq(kind, "noise"))        return PT_NOISE;
    if (str_eq(kind, "eof"))          return PT_EOF;
    return PT_UNKNOWN;                /* everything else => unknown   */
}

/* --------------------------------------------------------------------
   load_tokens_from_symbol_table:
   Reads tokens from SymbolTable.txt.
   - Skips header lines and borders.
   - Extracts lexeme and token kind from table rows.
   -------------------------------------------------------------------- */
static int load_tokens_from_symbol_table(const char *path) {
    FILE *fp = fopen(path, "r");      /* open file for reading        */
    char line[MAX_LINE];              /* buffer for each text line    */

    if (!fp) {                        /* if file could not be opened  */
        fprintf(stderr,
                "[Syntax] Cannot open SymbolTable file: %s\n", path);
        return 0;                     /* no tokens loaded             */
    }

    g_tok_count = 0;                  /* reset token counter          */

    /* read file line by line                                          */
    while (fgets(line, sizeof(line), fp)) {
        trim(line);                   /* remove extra spaces          */

        if (line[0] == '\0')          /* skip empty lines             */
            continue;

        /* skip "Source: ..." line using prefix check                  */
        if (str_starts_with(line, "Source:"))
            continue;

        /* skip table borders that start with '+' or '-'               */
        if (line[0] == '+' || line[0] == '-')
            continue;

        /* skip header row that contains both "Lexeme" and "Token"     */
        if (str_contains(line, "Lexeme") && str_contains(line, "Token"))
            continue;

        /* only process rows that start with '|' (table data rows)     */
        if (line[0] != '|')
            continue;

        /* extract lexeme and token kind using sscanf                  */
        char rawLex[128]  = {0};      /* buffer for lexeme field      */
        char rawKind[128] = {0};      /* buffer for token kind field  */

        /* pattern matches: | <lexeme> | <tokenKind> |                 */
        if (sscanf(line, "| %127[^|]| %127[^|]|", rawLex, rawKind) != 2)
            continue;                 /* malformed line => skip       */

        trim(rawLex);                 /* clean up spaces              */
        trim(rawKind);                /* clean up spaces              */

        ParserTokKind pk = map_kind(rawKind); /* map kind string       */

        if (g_tok_count < MAX_TOKENS) {       /* prevent overflow       */
            ParserToken *pt = &g_tokens[g_tok_count++]; /* next slot    */
            pt->kind = pk;                     /* store token kind      */

            /* copy lexeme into token struct safely                    */
            int i = 0;
            while (rawLex[i] != '\0' && i < MAX_LEXEME - 1) {
                pt->lexeme[i] = rawLex[i];
                i++;
            }
            pt->lexeme[i] = '\0';             /* terminate string       */
        }
    }

    fclose(fp);                        /* close the file              */

    /* If last token is not EOF, append EOF token at the end.          */
    if (g_tok_count == 0 || g_tokens[g_tok_count - 1].kind != PT_EOF) {
        ParserToken *pt = &g_tokens[g_tok_count++]; /* new token        */
        pt->kind = PT_EOF;                      /* mark as EOF          */
        pt->lexeme[0] = 'E';                   /* store "EOF" text      */
        pt->lexeme[1] = 'O';
        pt->lexeme[2] = 'F';
        pt->lexeme[3] = '\0';
    }

    return g_tok_count;                /* return number of tokens     */
}

/* --------------------------------------------------------------------
   Error handling and panic recovery
   -------------------------------------------------------------------- */
static int g_error = 0;                /* flag: set if any error      */

/* syntax_error:
   Prints a message and sets g_error to 1.                              */
static void syntax_error(const char *msg) {
    ParserToken *t = cur_tok();        /* current token               */
    fprintf(stderr,
            "[Syntax Error] %s. Near: %s\n",
            msg,
            t->lexeme[0] ? t->lexeme : "(EOF)");
    g_error = 1;                       /* remember there was an error */
}

/* panic_recover:
   Skips tokens until a good "statement boundary" is found:
   - semicolon ';'
   - right brace '}'
   - end-of-file
   This allows the parser to continue after an error.                   */
static void panic_recover(void) {
    while (cur_tok()->kind != PT_EOF) {  /* loop until EOF            */
        if (cur_tok()->kind == PT_SYMBOL) { /* if symbol token        */
            if (str_eq(cur_tok()->lexeme, ";") ||
                str_eq(cur_tok()->lexeme, "}")) {
                next_tok();              /* consume boundary          */
                break;                   /* exit panic mode           */
            }
        }
        next_tok();                      /* skip this token           */
    }
}

/* --------------------------------------------------------------------
   Helpers for matching specific symbols like ";" or ")"
   -------------------------------------------------------------------- */

/* accept_symbol:
   If current token is the given symbol, consume it and print leaf tag.
   Returns 1 on success, 0 otherwise.                                   */
static int accept_symbol(const char *sym) {
    if (cur_tok()->kind == PT_SYMBOL &&
        str_eq(cur_tok()->lexeme, sym)) {
        leaf_tag("symbol", cur_tok()->lexeme); /* print symbol node   */
        next_tok();                             /* move to next token  */
        return 1;                               /* success             */
    }
    return 0;                                   /* not matched         */
}

/* expect_symbol:
   Calls accept_symbol; if it fails, reports an error and recovers.     */
static void expect_symbol(const char *sym, const char *errmsg) {
    if (!accept_symbol(sym)) {          /* if symbol not present       */
        syntax_error(errmsg);           /* report specific error       */
        panic_recover();                /* skip ahead to safe point    */
    }
}

/* --------------------------------------------------------------------
   Forward declarations of all parsing functions (recursive descent)
   -------------------------------------------------------------------- */
static void parse_program(void);
static void parse_stmt_list(void);
static void parse_statement(void);
static void parse_decl_stmt(void);
static void parse_input_stmt(void);
static void parse_print_stmt(void);
static void parse_assign_stmt(void);
static void parse_assign_no_semicolon(void);
static void parse_if_stmt(void);
static void parse_while_stmt(void);
static void parse_for_stmt(void);
static void parse_block(void);
static void parse_expression(void);
static void parse_simple_expr(void);
static void parse_term(void);
static void parse_factor(void);

/* --------------------------------------------------------------------
   Grammar: program → stmt_list EOF
   -------------------------------------------------------------------- */

/* parse_program:
   Entry point of the parser.                                           */
static void parse_program(void) {
    open_tag("program");            /* <program>                      */
    parse_stmt_list();              /* parse list of statements       */
    close_tag("program");           /* </program>                     */
}

/* parse_stmt_list:
   stmt_list → { statement }                                           */
static void parse_stmt_list(void) {
    while (cur_tok()->kind != PT_EOF) {    /* until EOF token         */
        parse_statement();                 /* parse one statement      */
        printf("\n");   /* visual separator between statements */

    }
}

/* parse_statement:
   Decides which kind of statement to parse based on current token.     */
static void parse_statement(void) {
    ParserToken *t = cur_tok();           /* inspect current token     */

    /* Declaration: starts with type token                             */
    if (t->kind == PT_TYPE) {
        parse_decl_stmt();
        return;
    }

    /* Keyword-based statements                                        */
    if (t->kind == PT_KEYWORD) {
        if (str_eq(t->lexeme, "input")) {
            parse_input_stmt();
            return;
        }
        if (str_eq(t->lexeme, "print") ||
            str_eq(t->lexeme, "writeln")) {
            parse_print_stmt();
            return;
        }
        if (str_eq(t->lexeme, "if")) {
            parse_if_stmt();
            return;
        }
        if (str_eq(t->lexeme, "while")) {
            parse_while_stmt();
            return;
        }
        if (str_eq(t->lexeme, "for")) {
            parse_for_stmt();
            return;
        }
    }

    /* Assignment: begins with identifier                              */
    if (t->kind == PT_IDENTIFIER) {
        parse_assign_stmt();
        return;
    }

    /* If none of the above matched, it is an unexpected token.        */
    syntax_error("Unexpected token at start of statement");
    panic_recover();
}

/* --------------------------------------------------------------------
   Declaration statement:  type identifier ;
   Example:                int x;
   -------------------------------------------------------------------- */
static void parse_decl_stmt(void) {
    open_tag("declStatement");          /* <declStatement>            */

    /* type token already verified by caller                           */
    leaf_tag("type", cur_tok()->lexeme);/* tag for the type           */
    next_tok();                         /* consume type               */

    /* expect identifier name                                          */
    if (cur_tok()->kind == PT_IDENTIFIER) {
        leaf_tag("identifier", cur_tok()->lexeme);
        next_tok();
    } else {
        syntax_error("Expected identifier after type");
    }

    /* expect semicolon to end declaration                             */
    expect_symbol(";", "Missing ';' after declaration");

    close_tag("declStatement");         /* </declStatement>           */
}

/* --------------------------------------------------------------------
   Input statement: input identifier ;
   -------------------------------------------------------------------- */
static void parse_input_stmt(void) {
    open_tag("inputStatement");         /* <inputStatement>           */

    /* keyword 'input'                                                 */
    leaf_tag("keyword", cur_tok()->lexeme);
    next_tok();

    /* identifier to store input                                       */
    if (cur_tok()->kind == PT_IDENTIFIER) {
        leaf_tag("identifier", cur_tok()->lexeme);
        next_tok();
    } else {
        syntax_error("Expected identifier after 'input'");
    }

    /* semicolon terminator                                            */
    expect_symbol(";", "Missing ';' after input statement");

    close_tag("inputStatement");        /* </inputStatement>          */
}

/* --------------------------------------------------------------------
   Print / writeln statement:  print expression ;
   -------------------------------------------------------------------- */
static void parse_print_stmt(void) {
    open_tag("printStatement");         /* <printStatement>           */

    /* keyword print or writeln                                        */
    leaf_tag("keyword", cur_tok()->lexeme);
    next_tok();

    /* parse expression to be printed                                  */
    open_tag("expression");
    parse_expression();
    close_tag("expression");

    /* ending semicolon                                                */
    expect_symbol(";", "Missing ';' after print statement");

    close_tag("printStatement");        /* </printStatement>          */
}

/* --------------------------------------------------------------------
   Assignment statement (full form with semicolon):
     identifier = expression ;
   Used for normal statements and for-init in for-loop.
   -------------------------------------------------------------------- */
static void parse_assign_stmt(void) {
    open_tag("assignStatement");        /* <assignStatement>          */

    /* left-hand side identifier                                      */
    if (cur_tok()->kind == PT_IDENTIFIER) {
        leaf_tag("identifier", cur_tok()->lexeme);
        next_tok();
    } else {
        syntax_error("Expected identifier at start of assignment");
    }

    /* assignment operator '='                                         */
    if (cur_tok()->kind == PT_OPERATOR &&
        str_eq(cur_tok()->lexeme, "=")) {
        leaf_tag("symbol", "=");
        next_tok();
    } else {
        syntax_error("Expected '=' in assignment");
    }

    /* right-hand side expression                                      */
    open_tag("expression");
    parse_expression();
    close_tag("expression");

    /* semicolon required                                              */
    expect_symbol(";", "Missing ';' after assignment");

    close_tag("assignStatement");       /* </assignStatement>         */
}

/* --------------------------------------------------------------------
   parse_assign_no_semicolon:
   Assignment used in the UPDATE part of 'for' header:
     identifier = expression
   (NO semicolon here; ')' terminates the header)
   -------------------------------------------------------------------- */
static void parse_assign_no_semicolon(void) {
    open_tag("assignUpdate");           /* <assignUpdate>             */

    /* left-hand side identifier                                      */
    if (cur_tok()->kind == PT_IDENTIFIER) {
        leaf_tag("identifier", cur_tok()->lexeme);
        next_tok();
    } else {
        syntax_error("Expected identifier in for-update");
        close_tag("assignUpdate");
        return;
    }

    /* '=' operator                                                    */
    if (cur_tok()->kind == PT_OPERATOR &&
        str_eq(cur_tok()->lexeme, "=")) {
        leaf_tag("symbol", "=");
        next_tok();
    } else {
        syntax_error("Expected '=' in for-update");
        close_tag("assignUpdate");
        return;
    }

    /* expression on right side                                       */
    open_tag("expression");
    parse_expression();
    close_tag("expression");

    close_tag("assignUpdate");          /* </assignUpdate>            */
}

/* --------------------------------------------------------------------
   Block: either a single statement or { stmt_list }
   -------------------------------------------------------------------- */
static void parse_block(void) {
    /* if block starts with '{', parse multiple statements             */
    if (cur_tok()->kind == PT_SYMBOL &&
        str_eq(cur_tok()->lexeme, "{")) {

        leaf_tag("symbol", "{");       /* print opening brace         */
        next_tok();                    /* consume '{'                 */

        open_tag("statements");        /* <statements>                */
        while (cur_tok()->kind != PT_EOF &&
               !(cur_tok()->kind == PT_SYMBOL &&
                 str_eq(cur_tok()->lexeme, "}"))) {
            parse_statement();         /* parse each inner statement  */
            printf("\n");   /* visual separator between statements */
        }
        close_tag("statements");       /* </statements>               */

        /* require closing '}'                                         */
        expect_symbol("}", "Missing '}' at end of block");
    } else {
        /* otherwise a single statement acts as the block              */
        parse_statement();
        printf("\n");   /* visual separator between statements */
    }
}

/* --------------------------------------------------------------------
   If statement:
      if ( expression ) block [ else block ]
   -------------------------------------------------------------------- */
static void parse_if_stmt(void) {
    open_tag("ifStatement");          /* <ifStatement>               */

    /* 'if' keyword                                                   */
    leaf_tag("keyword", cur_tok()->lexeme);
    next_tok();

    /* opening '(' for condition                                      */
    expect_symbol("(", "Expected '(' after 'if'");

    /* condition expression                                           */
    open_tag("expression");
    parse_expression();
    close_tag("expression");

    /* closing ')'                                                    */
    expect_symbol(")", "Expected ')' after condition");

    /* parse then-block                                               */
    parse_block();

    /* optional else part                                             */
    if (cur_tok()->kind == PT_KEYWORD &&
        str_eq(cur_tok()->lexeme, "else")) {
        leaf_tag("keyword", "else");
        next_tok();
        parse_block();
    }

    close_tag("ifStatement");         /* </ifStatement>              */
}

/* --------------------------------------------------------------------
   While statement:
      while ( expression ) block
   -------------------------------------------------------------------- */
static void parse_while_stmt(void) {
    open_tag("whileStatement");       /* <whileStatement>            */

    /* 'while' keyword                                                */
    leaf_tag("keyword", cur_tok()->lexeme);
    next_tok();

    /* '(' for condition                                              */
    expect_symbol("(", "Expected '(' after 'while'");

    /* expression for condition                                       */
    open_tag("expression");
    parse_expression();
    close_tag("expression");

    /* ')' after condition                                            */
    expect_symbol(")", "Expected ')' after while condition");

    /* loop body block                                                */
    parse_block();

    close_tag("whileStatement");      /* </whileStatement>           */
}

/* --------------------------------------------------------------------
   For statement ():
      for ( assign_stmt ; expression ; assign_no_semicolon ) block
   Example:
      for (i = 0; i < 5; i = i + 1) { ... }
   -------------------------------------------------------------------- */
static void parse_for_stmt(void) {
    open_tag("forStatement");         /* <forStatement>              */

    /* 'for' keyword                                                  */
    leaf_tag("keyword", cur_tok()->lexeme);
    next_tok();

    /* opening '('                                                    */
    expect_symbol("(", "Expected '(' after 'for'");

    /* --- for-init: full assignment with semicolon ----------------- */
    open_tag("forInit");
    parse_assign_stmt();              /* consumes trailing ';'       */
    close_tag("forInit");

    /* --- for-condition --------------------------------------------- */
    open_tag("forCondition");
    parse_expression();               /* reads condition expression  */
    close_tag("forCondition");

    /* semicolon after condition                                     */
    expect_symbol(";", "Missing ';' in for condition");

    /* --- for-update: assignment without semicolon ------------------ */
    open_tag("forUpdate");
    parse_assign_no_semicolon();      /* no ';' inside header        */
    close_tag("forUpdate");

    /* closing ')' of for header                                     */
    expect_symbol(")", "Expected ')' after for header");

    /* loop body block                                               */
    parse_block();

    close_tag("forStatement");        /* </forStatement>             */
}

/* --------------------------------------------------------------------
   Expression grammar with basic precedence:

   expression   → simple_expr [relop simple_expr]
   simple_expr  → term { (+ | - | ||) term }
   term         → factor { (* | / | % | &&) factor }
   factor       → identifier | constant | ( expression )
   -------------------------------------------------------------------- */

/* is_relop:
   Returns 1 if op is a relational operator, 0 otherwise.               */
static int is_relop(const char *op) {
    return (str_eq(op, "==") ||
            str_eq(op, "!=") ||
            str_eq(op, "<")  ||
            str_eq(op, "<=") ||
            str_eq(op, ">")  ||
            str_eq(op, ">="));
}

/* parse_expression:
   Handles optional relational operator on top of simple_expr.          */
static void parse_expression(void) {
    open_tag("relExpression");        /* <relExpression>             */

    parse_simple_expr();              /* parse left side             */

    /* if current token is a relational operator, parse right side     */
    if (cur_tok()->kind == PT_OPERATOR &&
        is_relop(cur_tok()->lexeme)) {
        leaf_tag("symbol", cur_tok()->lexeme);
        next_tok();
        parse_simple_expr();
    }

    close_tag("relExpression");       /* </relExpression>            */
}

/* parse_simple_expr:
   Handles +, -, || chains.                                             */
static void parse_simple_expr(void) {
    open_tag("simpleExpression");     /* <simpleExpression>          */

    parse_term();                     /* first term                  */

    /* additional (+ | - | ||) term segments                          */
    while (cur_tok()->kind == PT_OPERATOR &&
          (str_eq(cur_tok()->lexeme, "+")  ||
           str_eq(cur_tok()->lexeme, "-")  ||
           str_eq(cur_tok()->lexeme, "||"))) {

        leaf_tag("symbol", cur_tok()->lexeme);
        next_tok();
        parse_term();
    }

    close_tag("simpleExpression");   
}

/* parse_term:
   Handles *, /, %, && chains.                                         */
static void parse_term(void) {
    open_tag("term");                 /* <term>                      */

    parse_factor();                   /* first factor                */

    /* additional (* | / | % | &&) factor segments                     */
    while (cur_tok()->kind == PT_OPERATOR &&
          (str_eq(cur_tok()->lexeme, "*")  ||
           str_eq(cur_tok()->lexeme, "/")  ||
           str_eq(cur_tok()->lexeme, "%")  ||
           str_eq(cur_tok()->lexeme, "&&"))) {

        leaf_tag("symbol", cur_tok()->lexeme);
        next_tok();
        parse_factor();
    }

    close_tag("term");                /* </term>                     */
}

/* parse_factor:
   factor → identifier | constant | ( expression )
   -------------------------------------------------------------------- */
static void parse_factor(void) {
    ParserToken *t = cur_tok();       /* look at current token       */

    /* Parenthesized expression: ( expression )                        */
    if (t->kind == PT_SYMBOL && str_eq(t->lexeme, "(")) {
        leaf_tag("symbol", "(");
        next_tok();                   /* consume '('                 */

        open_tag("expression");
        parse_expression();           /* parse inner expression      */
        close_tag("expression");

        if (cur_tok()->kind == PT_SYMBOL &&
            str_eq(cur_tok()->lexeme, ")")) {
            leaf_tag("symbol", ")");
            next_tok();               /* consume ')'                 */
        } else {
            syntax_error("Missing ')' after grouped expression");
        }
        return;
    }

    /* Identifier as a factor                                          */
    if (t->kind == PT_IDENTIFIER) {
        leaf_tag("identifier", t->lexeme);
        next_tok();
        return;
    }

    /* Constant literal as factor                                      */
    if (t->kind == PT_INTCONST  ||
        t->kind == PT_FLOATCONST||
        t->kind == PT_CHARCONST ||
        t->kind == PT_BOOLCONST) {

        leaf_tag("literal", t->lexeme);
        next_tok();
        return;
    }

    /* If none of the valid options match, it is an error.             */
    syntax_error("Expected identifier, literal, or '(' in expression");
    panic_recover();
}


int main(void) {
    int n = load_tokens_from_symbol_table("SymbolTable.txt");
    if (n == 0) {                         /* if no tokens were loaded  */
        fprintf(stderr, "[Syntax] No tokens loaded.\n");
        return 1;                         /* stop with error code      */
    }

    /* initialize global state                                         */
    g_tok_index = 0;                      /* start at first token      */
    g_error = 0;                          /* clear error flag          */
    g_indent = 0;                         /* reset indentation         */

    /* start parsing from program rule                                 */
    parse_program();

    /* after parse, we expect only EOF                                 */
    if (cur_tok()->kind != PT_EOF) {
        syntax_error("Unexpected extra code after program");
    }

   
    if (g_error) {
        printf("\n[Syntax] Program has syntax errors.\n");
    } else {
        printf("\n[Syntax] Program is syntactically correct.\n");
    }

    return 0;                            
}
