#include <stdio.h>    // for printing and files (printf, fopen, etc.)
#include <stdlib.h>   // for memory (malloc, free)
#include <string.h>   // only for strlen and memcpy (NO strcmp/strncmp)
#include <ctype.h>    // for character tests (isdigit, isalpha, etc.)

/* ---------------- token type labels ----------------
   This enum lists the possible "kinds" of tokens we can produce. */
typedef enum {
  TOK_IDENTIFIER,       // user names: myVar, count_1
  TOK_KEYWORD,          // special words: if, while, return, ...
  TOK_RESERVED_TYPE,    // built-in types: int, float, char, bool, void
  TOK_CONST_INT,        // number without dot: 123
  TOK_CONST_FLOAT,      // number with dot: 12.34
  TOK_CONST_CHAR,       // single char literal: 'a'
  TOK_CONST_BOOL,       // true or false
  TOK_CONST_STRING,     // "hello"
  TOK_OP_ARITH,         // + - * / % ** DIV MOD
  TOK_OP_REL,           // < > <= >= == !=
  TOK_OP_LOGIC,         // && || !
  TOK_ASSIGN,           // =
  TOK_DELIM,            // ; , : .
  TOK_BRACKET,          // () [] {}
  TOK_COMMENT,          // //... or /* ... */
  TOK_NOISE,            // words we treat as "noise": please, then, to, ...
  TOK_UNKNOWN,          // anything not recognized or broken
  TOK_EOF               // end-of-file marker
} TokenType;

/* ---------------- token structure ----------------
   A token = kind + text + where it started + optional label (extra). */
typedef struct {
  TokenType type;    // what kind of token
  char *lexeme;      // exact text of the token (we store a copy)
  int line, col;     // where it started (1-based line and column)
  char *extra;       // small subtype hint like "**", "DIV", "(" for pretty output
} Token;

/* ---------------- small string helpers ---------------- */

/* dup_n:
   Copy exactly n bytes from s into a new null-terminated string. */
static char *dup_n(const char *s, int n){
  char *p = (char*)malloc(n+1);  // allocate n+1 for '\0'
  if(!p) return NULL;            // if allocation fails, return NULL
  memcpy(p, s, n);               // copy n bytes from s
  p[n] = 0;                      // add string terminator
  return p;                      // return the new string pointer
}

/* dup_s:
   Copy a full C-string into a new allocation. */
static char *dup_s(const char *s){
  return dup_n(s, (int)strlen(s));  // reuse dup_n with full length
}

/* lowerc:
   Convert one ASCII letter to lowercase without locale stuff. */
static inline char lowerc(char ch){
  if (ch >= 'A' && ch <= 'Z')      // if uppercase
    return (char)(ch - 'A' + 'a'); // turn into lowercase
  return ch;                       // else return as-is
}

/* ends_with_ksh:
   Check manually if a file name ends with ".ksh". */
static int ends_with_ksh(const char *name){
  size_t n = strlen(name);             // get length
  if (n < 4) return 0;                 // too short to end with ".ksh"
  // compare last 4 chars one-by-one
  return (name[n-4]=='.' && name[n-3]=='k' && name[n-2]=='s' && name[n-1]=='h');
}

/* ---------------- lexer state ----------------
   This struct keeps the whole text and our current position. */
typedef struct {
  const char *buf;   // the whole file text in memory
  size_t len;        // total length of buf
  size_t pos;        // next index to read (0..len)
  int line;          // current line number (starts at 1)
  int col;           // current column number (starts at 1)
} Lexer;

/* peek:
   Look at the next character without consuming it. */
static int peek(Lexer* L){
  return (L->pos < L->len) ? L->buf[L->pos] : EOF; // EOF if at end
}

/* advance:
   Consume one character and move forward, updating line/col. */
static int advance(Lexer* L){
  if (L->pos >= L->len) return EOF;  // nothing left
  int c = L->buf[L->pos++];          // get char and increase pos
  if (c=='\n'){                      // if newline
    L->line++;                       // next line
    L->col = 1;                      // reset column
  } else {
    L->col++;                        // same line: just move column
  }
  return c;                          // return the char we consumed
}

/* match:
   If the next character equals 'ch', consume it and return 1; else 0. */
static int match(Lexer* L, int ch){
  if (peek(L) == ch){ advance(L); return 1; }  // eat it and say yes
  return 0;                                    // otherwise say no
}

/* skip_ws:
   Skip spaces, tabs, and line breaks so next token starts at real text. */
static void skip_ws(Lexer* L){
  for(;;){                          // loop until non-space
    int c = peek(L);                // look at next char
    if (c==' ' || c=='\t' || c=='\r' || c=='\n')
      advance(L);                   // consume whitespace
    else
      break;                        // stop when not whitespace
  }
}

/* make:
   Build a Token object with copies of text and extra (if present). */
static Token make(Lexer* L, TokenType ty, const char* s, int n, const char* extra){
  Token t;                          // create local token
  t.type   = ty;                    // set token kind
  t.lexeme = s ? dup_n(s,n) : NULL; // copy lexeme if provided
  t.line   = L->line;               // record current line
  t.col    = L->col;                // record current column
  t.extra  = extra ? dup_s(extra) : NULL;  // copy subtype text if any
  return t;                         // return token
}

/* ============================================================
   DFA-BASED KEYWORD SCANNER (Option A integration)
   Used ONLY to detect keywords; all other scanning stays the same
   ============================================================ */

/* Keyword list (from your DFA code) */
static const char* KEYWORDS[] = {
  "if","else","elseif","for","while","do",
  "switch","case","default","break","continue","return",
  "print","input","writeln","readln",
  "begin","end","then","of","repeat","until"
};

#define KEYWORD_COUNT (int)(sizeof(KEYWORDS)/sizeof(KEYWORDS[0]))
#define DFA_MAX_ROOTS    64
#define DFA_MAX_CHILDREN 31

typedef struct DFANode {
  struct DFANode **children;  // child pointers
  char val;                   // character for this node
  int isLeaf;                 // 1 if this node ends a keyword
  int cSize;                  // number of used children
} DFANode;

typedef struct {
  DFANode **start;            // array of starting nodes (by first char)
  int sSize;                  // number of used start entries
} DFA;

/* Forward declarations for DFA helpers */
static DFANode *dfa_init_chain(const char *symbol, int index, int len);
static DFANode *dfa_brute_LCA(DFANode* root, const char *symbol, int index, int len);
static void      dfa_add_symbol(DFA *head, const char *symbol);
static int       dfa_match(const DFA *head, const char *symbol, int len);

/* Global keyword DFA + build-once flag */
static DFA kw_dfa;
static int kw_dfa_built = 0;

/* dfa_init_chain:
   Create a chain of nodes for symbol[index..len-1]. */
static DFANode *dfa_init_chain(const char *symbol, int index, int len) {
  if (len == 0) {
    return NULL;
  }
  if (index == len) {
    return NULL;
  }

  DFANode *root = (DFANode*)malloc(sizeof(DFANode));
  if (!root) return NULL;

  root->children = (DFANode **) malloc(DFA_MAX_CHILDREN * sizeof(DFANode *));
  if (!root->children) {
    free(root);
    return NULL;
  }

  for (int i = 0; i < DFA_MAX_CHILDREN; i++)
    root->children[i] = NULL;

  root->val   = symbol[index];
  root->cSize = 0;
  root->isLeaf = (index == len - 1);

  DFANode *child = dfa_init_chain(symbol, index + 1, len);
  if (child != NULL && root->cSize < DFA_MAX_CHILDREN) {
    root->children[root->cSize++] = child;
  }

  return root;
}

/* dfa_brute_LCA:
   Reuse states where possible; add missing suffix via new chains. */
static DFANode *dfa_brute_LCA(DFANode* root, const char *symbol, int index, int len) {
  if (index >= len) {
    root->isLeaf = 1;   // mark that some keyword ends here
    return root;
  }

  /* Try to find existing child matching symbol[index] */
  for (int i = 0; i < root->cSize; i++) {
    if (symbol[index] == root->children[i]->val) {
      root->children[i] = dfa_brute_LCA(root->children[i], symbol, index + 1, len);
      return root;
    }
  }

  /* Not found => create new chain starting at index */
  if (root->cSize < DFA_MAX_CHILDREN) {
    DFANode *child = dfa_init_chain(symbol, index, len);
    if (child != NULL) {
      root->children[root->cSize++] = child;
    }
  }
  return root;
}

/* dfa_add_symbol:
   Insert one keyword into the DFA. */
static void dfa_add_symbol(DFA *head, const char *symbol) {
  int len = (int)strlen(symbol);
  if (len == 0) return;

  int index = 0;
  /* Check if there is already a root for first char */
  for (int i = 0; i < head->sSize; i++) {
    if (symbol[0] == head->start[i]->val) {
      /* Reuse that root, add rest through LCA */
      dfa_brute_LCA(head->start[i], symbol, 1, len);
      return;
    }
  }

  /* No existing root for this first char: create full chain */
  if (head->sSize < DFA_MAX_ROOTS) {
    head->start[head->sSize++] = dfa_init_chain(symbol, 0, len);
  }
}

/* dfa_match:
   Return 1 if symbol[0..len-1] matches a keyword in DFA; else 0. */
static int dfa_match(const DFA *head, const char *symbol, int len) {
  if (len == 0) return 0;
  /* Find matching root */
  DFANode *node = NULL;
  for (int i = 0; i < head->sSize; i++) {
    if (symbol[0] == head->start[i]->val) {
      node = head->start[i];
      break;
    }
  }
  if (!node) return 0;

  int idx = 1;
  while (idx < len) {
    DFANode *next = NULL;
    for (int c = 0; c < node->cSize; c++) {
      if (symbol[idx] == node->children[c]->val) {
        next = node->children[c];
        break;
      }
    }
    if (!next) return 0; // path breaks
    node = next;
    idx++;
  }
  return node->isLeaf ? 1 : 0;
}

/* build_keyword_dfa:
   Build the DFA once from KEYWORDS[]. */
static void build_keyword_dfa(void) {
  if (kw_dfa_built) return;

  kw_dfa.start = (DFANode **) malloc(DFA_MAX_ROOTS * sizeof(DFANode *));
  if (!kw_dfa.start) return;

  for (int i = 0; i < DFA_MAX_ROOTS; i++)
    kw_dfa.start[i] = NULL;

  kw_dfa.sSize = 0;

  for (int i = 0; i < KEYWORD_COUNT; i++) {
    dfa_add_symbol(&kw_dfa, KEYWORDS[i]);
  }

  kw_dfa_built = 1;
}

/* ---------------- word classifiers ----------------
   These functions recognize types, noise words, boolean literals,
   and word-operators (DIV/MOD). Keywords now use DFA above. */

/* is_keyword:
   Return 1 if s (length n) is a keyword; else 0.
   Now implemented using the DFA built from KEYWORDS[]. */
static int is_keyword(const char* s, int n){
  build_keyword_dfa();          // ensure DFA is built
  return dfa_match(&kw_dfa, s, n);
}

/* is_type:
   Return 1 if s is a built-in type; else 0.
   Types: int, float, char, bool, void */
static int is_type(const char* s, int n){
  switch (lowerc(s[0])){
    case 'i': return (n==3 && s[1]=='n'&&s[2]=='t');                            // int
    case 'f': return (n==5 && s[1]=='l'&&s[2]=='o'&&s[3]=='a'&&s[4]=='t');      // float
    case 'c': return (n==4 && s[1]=='h'&&s[2]=='a'&&s[3]=='r');                 // char
    case 'b': return (n==4 && s[1]=='o'&&s[2]=='o'&&s[3]=='l');                 // bool
    case 'v': return (n==4 && s[1]=='o'&&s[2]=='i'&&s[3]=='d');                 // void
  }
  return 0;                                                                     // not a type
}

/* is_noise:
   Return 1 if s is a noise word; else 0.
   Noise: please, then, do, end, begin, of, the, to, and, from */
static int is_noise(const char* s, int n){
  switch (lowerc(s[0])){
    case 'p': return (n==6 && s[1]=='l'&&s[2]=='e'&&s[3]=='a'&&s[4]=='s'&&s[5]=='e'); // please
    case 't':
      if (n==4 && s[1]=='h'&&s[2]=='e'&&s[3]=='n') return 1;                          // then
      if (n==2 && s[1]=='o') return 1;                                                // to
      return 0;
    case 'd': return (n==2 && s[1]=='o');                                             // do
    case 'e': return (n==3 && s[1]=='n'&&s[2]=='d');                                  // end
    case 'b': return (n==5 && s[1]=='e'&&s[2]=='g'&&s[3]=='i'&&s[4]=='n');            // begin
    case 'o': return (n==2 && s[1]=='f');                                             // of
    case 'a': return (n==3 && s[1]=='n'&&s[2]=='d');                                  // and
    case 'f': return (n==4 && s[1]=='r'&&s[2]=='o'&&s[3]=='m');                       // from
  }
  return 0;                                                                           // not noise
}

/* is_bool_lit:
   Return 1 if "true", 0 if "false", else -1 if not a boolean literal. */
static int is_bool_lit(const char* s, int n){
  if (n==4 && lowerc(s[0])=='t' && s[1]=='r'&&s[2]=='u'&&s[3]=='e')  return 1;  // true
  if (n==5 && lowerc(s[0])=='f' && s[1]=='a'&&s[2]=='l'&&s[3]=='s'&&s[4]=='e') return 0; // false
  return -1;                                                                      // not bool
}

/* is_word_op:
   Return 1 if "DIV" or "MOD" (case-insensitive). Set *out_extra to "DIV"/"MOD". */
static int is_word_op(const char* s, int n, const char** out_extra){
  if (n==3){
    char a=lowerc(s[0]), b=lowerc(s[1]), c=lowerc(s[2]); // lowercase chars
    if (a=='d'&&b=='i'&&c=='v'){ if(out_extra)*out_extra="DIV"; return 1; }     // DIV
    if (a=='m'&&b=='o'&&c=='d'){ if(out_extra)*out_extra="MOD"; return 1; }     // MOD
  }
  return 0;                                                                         // not word-op
}

/* ---------------- scanners (build tokens) ---------------- */

/* scan_string:
   Read characters until the closing " is found. Handle escapes like \".
   If newline/EOF appears first, mark as unknown (broken string). */
static Token scan_string(Lexer* L){
  int col0 = L->col;             // remember the starting column
  advance(L);                    // consume opening quote
  size_t p = L->pos;             // start of the string payload
  int c;
  while ((c=peek(L)) != EOF){    // loop until we hit EOF or closing quote
    if (c=='\\'){                // if backslash, skip next char (escape)
      advance(L);
      if (peek(L)!=EOF) advance(L);
      continue;
    }
    if (c=='"'){                 // closing quote
      size_t n = L->pos - p;     // compute string length
      Token t = make(L, TOK_CONST_STRING, L->buf+p, (int)n, NULL); // build token
      advance(L);                // consume the closing quote
      t.col = col0;              // fix column to where it started
      return t;                  // done
    }
    if (c=='\n') break;          // newline before closing quote = broken
    advance(L);                  // consume normal char
  }
  return make(L, TOK_UNKNOWN, "<unterminated_string>", 22, NULL);  // report unknown
}

/* scan_char:
   Read 'x' or escaped like '\n'. Must end with a closing '.
   If not properly closed, return unknown. */
static Token scan_char(Lexer* L){
  int col0 = L->col;             // remember column
  advance(L);                    // consume opening single quote
  int c = advance(L);            // read the character
  if (c=='\\') advance(L);       // allow one escaped char
  if (peek(L)=='\''){            // must see closing single quote
    advance(L);                  // consume closing quote
    Token t = make(L, TOK_CONST_CHAR, "<char>", 6, NULL); // we don't store payload
    t.col = col0;                // fix column
    return t;                    // done
  }
  return make(L, TOK_UNKNOWN, "<unterminated_char>", 20, NULL);    // broken char
}

/* scan_number:
   Read digits, optionally a dot and more digits.
   If a dot is present but not followed by digit, mark unknown (bad float). */
static Token scan_number(Lexer* L){
  int start = (int)L->pos;       // mark start index
  int col0  = L->col;            // mark starting column
  while (isdigit(peek(L)))       // read digits
    advance(L);
  int isf = 0;                   // 0 = int, 1 = float
  if (peek(L)=='.'){             // check for decimal part
    isf = 1;                     // it is a float candidate
    advance(L);                  // consume dot
    if (!isdigit(peek(L)))       // must have at least one digit after dot
      return make(L, TOK_UNKNOWN, "<bad_float>", 10, NULL); // mark unknown
    while (isdigit(peek(L)))     // read digits after dot
      advance(L);
  }
  int n = (int)L->pos - start;   // total length
  Token t = make(L, isf ? TOK_CONST_FLOAT : TOK_CONST_INT, L->buf+start, n, NULL);
  t.col = col0;                  // fix column
  return t;                      // done
}

/* scan_identifier_or_keyword:
   Read a word: first char is letter/_; rest can be letter/digit/_.
   Then classify it: bool, word-op, type, keyword (via DFA), noise, or identifier. */
static Token scan_identifier_or_keyword(Lexer* L){
  int start = (int)L->pos;       // start index
  int col0  = L->col;            // start column
  advance(L);                    // consume first letter or '_'
  while (isalnum(peek(L)) || peek(L)=='_')  // read rest of word
    advance(L);

  int n = (int)L->pos - start;   // length of word
  const char* s = L->buf + start;// pointer to the word

  // 1) boolean literal?
  int b = is_bool_lit(s, n);     // 1=true, 0=false, -1=not bool
  if (b != -1){
    Token t = make(L, TOK_CONST_BOOL, s, n, NULL);
    t.col = col0; return t;
  }

  // 2) word operator (DIV/MOD)?
  const char* extra = NULL;      // will hold "DIV" or "MOD"
  if (is_word_op(s, n, &extra)){
    Token t = make(L, TOK_OP_ARITH, s, n, extra);
    t.col = col0; return t;
  }

  // 3) built-in type?
  if (is_type(s, n)){
    Token t = make(L, TOK_RESERVED_TYPE, s, n, NULL);
    t.col = col0; return t;
  }

  // 4) keyword?  (NOW DFA-BASED)
  if (is_keyword(s, n)){
    Token t = make(L, TOK_KEYWORD, s, n, NULL);
    t.col = col0; return t;
  }

  // 5) noise word?
  if (is_noise(s, n)){
    Token t = make(L, TOK_NOISE, s, n, NULL);
    t.col = col0; return t;
  }

  // 6) otherwise, a plain identifier (user name).
  Token t = make(L, TOK_IDENTIFIER, s, n, NULL);
  t.col = col0; return t;
}

/* scan_slash_comment_or_op:
   Handle '/', which can mean:
   - '//' line comment until newline
   - '/* ... *\/' block comment
   - or plain arithmetic operator '/' */
static Token scan_slash_comment_or_op(Lexer* L){
  int col0 = L->col;             // remember column
  if (match(L,'/')){             // if we see a second '/'
    if (match(L,'/')){           // it's a line comment
      while (peek(L)!=EOF && peek(L)!='\n')  // skip until end of line
        advance(L);
      Token t = make(L, TOK_COMMENT, "//", 2, NULL); // produce comment token
      t.col = col0; return t;
    }
    if (match(L,'*')){           // start of block comment
      int prev=0, cur=0;         // track last two chars to find "*/"
      while ((cur=advance(L)) != EOF){
        if (prev=='*' && cur=='/'){           // found closing
          Token t = make(L, TOK_COMMENT, "/* */", 5, NULL);
          t.col = col0; return t;
        }
        prev = cur;                           // shift window
      }
      return make(L, TOK_UNKNOWN, "<unterminated_comment>", 22, NULL);  // never closed
    }
    // if it was just one '/', it's the arithmetic operator
    Token t = make(L, TOK_OP_ARITH, "/", 1, "/");
    t.col = col0; return t;
  }
  // shouldn't happen here (caller checks), but mark unknown if it does
  int c = advance(L); char s2[2] = {(char)c,0};
  return make(L, TOK_UNKNOWN, s2, 1, NULL);
}

/* scan_operator:
   Handle other operators:
   - two-char: == != <= >= && || **
   - single-char: = + - * % < > !
   Unknown symbol becomes TOK_UNKNOWN. */
static Token scan_operator(Lexer* L){
  int col0 = L->col;              // remember column
  int c = advance(L);             // read current operator char

  // two-character operators first
  if (c=='=' && match(L,'=')){ Token t=make(L,TOK_OP_REL,"==",2,"=="); t.col=col0; return t; }
  if (c=='!' && match(L,'=')){ Token t=make(L,TOK_OP_REL,"!=",2,"!="); t.col=col0; return t; }
  if (c=='<' && match(L,'=')){ Token t=make(L,TOK_OP_REL,"<=",2,"<="); t.col=col0; return t; }
  if (c=='>' && match(L,'=')){ Token t=make(L,TOK_OP_REL,">=",2,">="); t.col=col0; return t; }
  if (c=='&' && match(L,'&')){ Token t=make(L,TOK_OP_LOGIC,"&&",2,"&&"); t.col=col0; return t; }
  if (c=='|' && match(L,'|')){ Token t=make(L,TOK_OP_LOGIC,"||",2,"||"); t.col=col0; return t; }
  if (c=='*' && match(L,'*')){ Token t=make(L,TOK_OP_ARITH,"**",2,"**"); t.col=col0; return t; }

  // single-character operators
  if (c=='='){ Token t=make(L,TOK_ASSIGN,"=",1,"="); t.col=col0; return t; }
  if (c=='+'||c=='-'||c=='*'||c=='/'||c=='%'){
    char s2[2]={(char)c,0}; Token t=make(L,TOK_OP_ARITH,s2,1,s2); t.col=col0; return t;
  }
  if (c=='<'||c=='>'){
    char s2[2]={(char)c,0}; Token t=make(L,TOK_OP_REL,s2,1,s2); t.col=col0; return t;
  }
  if (c=='!'){ Token t=make(L,TOK_OP_LOGIC,"!",1,"!"); t.col=col0; return t; }

  // anything else is unknown
  char s2[2]={(char)c,0};
  return make(L, TOK_UNKNOWN, s2, 1, NULL);
}

/* quick helpers for punctuation classes */
static int is_delim(int c){ return c==';'||c==','||c==':'||c=='.'; }  // delimiters
static int is_bracket(int c){ return c=='('||c==')'||c=='{'||c=='}'||c=='['||c==']'; } // brackets

/* next_token:
   Main scanner step: skip spaces, look at next char, decide what to scan. */
static Token next_token(Lexer* L){
  skip_ws(L);                     // ignore whitespace first
  int c = peek(L);                // look ahead
  if (c==EOF)                    // if no more chars
    return make(L, TOK_EOF, NULL, 0, NULL);   // end-of-file token

  // simple single-char tokens
  if (is_delim(c)){ int ch=advance(L); char s2[2]={ch,0}; return make(L, TOK_DELIM,   s2,1,s2); }
  if (is_bracket(c)){int ch=advance(L); char s2[2]={ch,0}; return make(L, TOK_BRACKET, s2,1,s2); }

  // identifiers, numbers, strings, chars
  if (isalpha(c) || c=='_') return scan_identifier_or_keyword(L);
  if (isdigit(c))           return scan_number(L);
  if (c=='"')               return scan_string(L);
  if (c=='\'')              return scan_char(L);

  // operators
  if (c=='/')               return scan_slash_comment_or_op(L);
  if (c=='*'||c=='+'||c=='-'||c=='%'||c=='='||c=='<'||c=='>'||c=='!'||c=='&'||c=='|')
    return scan_operator(L);

  // totally unknown character
  advance(L);
  char s2[2]={(char)c,0};
  return make(L, TOK_UNKNOWN, s2, 1, NULL);
}

/* ---------------- pretty table output ----------------
   We print a neat two-column table: Lexeme | Token. */

/* tname:
   Convert TokenType to a small label for the table. */
static const char* tname(TokenType t){
  switch (t){
    case TOK_IDENTIFIER:    return "identifier";
    case TOK_KEYWORD:       return "keyword";
    case TOK_RESERVED_TYPE: return "type";
    case TOK_CONST_INT:     return "const_int";
    case TOK_CONST_FLOAT:   return "const_float";
    case TOK_CONST_CHAR:    return "const_char";
    case TOK_CONST_STRING:  return "const_string";
    case TOK_CONST_BOOL:    return "const_bool";
    case TOK_OP_ARITH:      return "operator";
    case TOK_OP_REL:        return "operator";
    case TOK_OP_LOGIC:      return "operator";
    case TOK_ASSIGN:        return "operator";
    case TOK_DELIM:         return "punctuator";
    case TOK_BRACKET:       return "punctuator";
    case TOK_COMMENT:       return "comment";
    case TOK_NOISE:         return "noise";
    case TOK_UNKNOWN:       return "unknown";
    case TOK_EOF:           return "eof";
  }
  return "?";  // fallback, should not happen
}

/* write_head:
   Print the header lines of the table. */
static void write_head(FILE* fp, const char* src){
  fprintf(fp, "Source: %s\n", src);
  fprintf(fp, "+----------------------+------------------+\n");
  fprintf(fp, "| %-20s | %-16s |\n", "Lexeme", "Token");
  fprintf(fp, "+----------------------+------------------+\n");
}

/* write_row:
   Print one row in the table. If lexeme is long, clip for layout. */
static void write_row(FILE* fp, const char* lex, const char* tok){
  char buf[64];
  int n = (int)strlen(lex);
  if (n > 20){ memcpy(buf, lex, 19); buf[19]='.'; buf[20]=0; lex = buf; } // clip long
  fprintf(fp, "| %-20s | %-16s |\n", lex, tok);
}

/* write_foot:
   Print the closing line of the table. */
static void write_foot(FILE* fp){
  fprintf(fp, "+----------------------+------------------+\n");
}

/* ---------------- file loader ----------------
   read_all: read the whole file into memory at once. */
static char* read_all(const char* path, size_t* out_len){
  FILE* f = fopen(path, "rb");         // open for reading binary
  if (!f) return NULL;                  // fail -> NULL
  fseek(f, 0, SEEK_END);                // go to end
  long n = ftell(f);                    // get file size
  fseek(f, 0, SEEK_SET);                // back to start
  char* buf = (char*)malloc(n+1);       // allocate size + 1
  if (!buf){ fclose(f); return NULL; }  // if fail, close and return
  fread(buf, 1, n, f);                  // read all bytes
  fclose(f);                            // close file
  buf[n] = 0;                           // null-terminate
  if (out_len) *out_len = n;            // return length if asked
  return buf;                           // return the buffer
}

/* ---------------- main program ----------------
   Steps:
   1) decide input path (argv or default "sample.ksh")
   2) check it ends with .ksh (manual, no strcmp)
   3) read file into memory
   4) scan tokens and print table to console and SymbolTable.txt
   5) free memory and exit */
int main(int argc, char** argv){
  char path[1024] = {0};                 // buffer to store path

  if (argc >= 2){                        // if user passed a file path
    // manual safe copy (no strcpy risks)
    size_t i=0;
    while (argv[1][i] && i < sizeof(path)-1){
      path[i] = argv[1][i];
      i++;
    }
    path[i] = 0;                         // null-terminate
  } else {
    // default to "sample.ksh"
    path[0]='s'; path[1]='a'; path[2]='m'; path[3]='p'; path[4]='l'; path[5]='e';
    path[6]='.'; path[7]='k'; path[8]='s'; path[9]='h'; path[10]=0;
  }

  if (!ends_with_ksh(path)){             // manual ".ksh" checker
    fprintf(stderr, "Error: need a .ksh source file (got: %s)\n", path);
    return 1;                            // stop if wrong extension
  }

  size_t len = 0;                        // will hold size of file
  char* buf = read_all(path, &len);      // load whole file
  if (!buf){                             // if failed to read
    fprintf(stderr, "Error: cannot read file: %s\n", path);
    return 1;                            // stop
  }

  Lexer L = {0};                         // create lexer state
  L.buf = buf; L.len = len;              // set buffer and length
  L.pos = 0; L.line = 1; L.col = 1;      // start at line 1, col 1

  FILE* out = fopen("SymbolTable.txt","wb"); // open output file
  if (!out){
    fprintf(stderr, "Error: cannot create SymbolTable.txt\n");
    free(buf);
    return 1;
  }

  write_head(stdout, path);              // print table header to console
  write_head(out,    path);              // and to file

  for(;;){                               // main scanning loop
    Token t = next_token(&L);            // get next token
    const char* shown = (t.extra && *t.extra) ? t.extra : (t.lexeme ? t.lexeme : "");
    write_row(stdout, shown, tname(t.type)); // print row to console
    write_row(out,    shown, tname(t.type)); // print row to file
    if (t.lexeme) free(t.lexeme);        // free token text copy
    if (t.extra)  free(t.extra);         // free extra text copy
    if (t.type == TOK_EOF) break;        // stop at end-of-file
  }

  write_foot(stdout);                    // close the table (console)
  write_foot(out);                       // close the table (file)
  fclose(out);                           // close SymbolTable.txt
  free(buf);                             // free file buffer
  return 0;                              // exit OK
}
