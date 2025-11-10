#include "../include/scanner.h"

static const char* KEYWORDS[] = {
  "if","else","elseif","for","while","do",
  "switch","case","default","break","continue","return",
  "print","input","writeln","readln",
  "begin","end","then","of","repeat","until"
};
static const int KEYWORDS_N = 22;

static const char* TYPES[] = {"int","float","char","bool"};
static const int TYPES_N = 4;

static const char* NOISE[] = {"please","kindly","beginwith","endwith","noop"};
static const int NOISE_N = 5;

// character in a string
bool in(char *arr, char target) {
  int n = strlen(arr);

  for (int i = 0; i < n; i++) {
    char arrElem = arr[i];
    if (arrElem == target) return true;
  }

  return false;
}

// string in a list of string
bool inList(char **arr, char *target) {
  int n = sizeof(arr) / sizeof(arr[0]);

  for (int i = 0; i < n; i++) {
    char *arrElem = arr[i];
    int m = strlen(arrElem);
    bool found = true;

    if (m != strlen(target)) continue;

    for (int j = 0; j < m; j++) {
      if (arrElem[j] != target[j]) found = false;
    }

    if (found) return found;
  }

  return false;
}

bool is_bool(char *a) {
  int n = strlen(a);
  char tr[] = "true", fl[] = "false";
  int t = strlen(tr), f = strlen(fl);

  if (t != n && f != n) return false;

  bool is_true = true;
  for (int i = 0; i < t; i++) {
    if (tr[i] != a[i]) {
      is_true = false;
      break;
    }
  }

  bool is_false = true;
  for (int i = 0; i < f; i++) {
    if (fl[i] != a[i]) {
      is_false = false;
      break;
    }
  }

  return is_true || is_false;
}

bool is_arith(char *a) {
  int n = strlen(a);
  char mod[] = "MOD", div[] = "DIV";
  int m = strlen(mod);

  if (m != n) return false;

  bool is_mod = true;
  for (int i = 0; i < m; i++) {
    if (mod[i] != a[i]) {
      is_mod = false;
      break;
    }
  }

  bool is_div = true;
  for (int i = 0; i < m; i++) {
    if (div[i] != a[i]) {
      is_div = false;
      break;
    }
  }

  return is_mod || is_div;
}


Token scan_identifier_or_keyword(Lexer* L) {
  int start = (int)L->pos;
  while (!in(" \n(){}[]\r\t", peek(L))) {
    advance(L);
  } 
  int n = (int)L->pos - start + 1;
  char *keyword = malloc((n + 1) * sizeof(char));

  for (int i = start; i <= (int)L->pos; i++) {
    keyword[i] = L->buf[i];
  }

  keyword[n] = '\0';
  TokenType type;
  if (is_bool(keyword)) type = TOK_CONST_BOOL;
  else if (is_arith(keyword)) type = TOK_OP_ARITH;
  else if (inList(TYPES, keyword)) type = TOK_RESERVED_TYPE;
  else if (inList(KEYWORDS, keyword)) type = TOK_KEYWORD;
  else if (inList(NOISE, keyword)) type = TOK_NOISE;
  else type = TOK_IDENTIFIER;

  return make(L, type, keyword, n, NULL);
}


Token scan_number(Lexer* L) {
  int start = (int)L->pos;
  bool is_float = false;

  while (!in(" \n(){}[]\r\t", peek(L))) {
    advance(L);
    if (match(L, '.')) is_float = true;
  }
  int n = (int)L->pos - start + 1;
  char *number = malloc((n + 1) * sizeof(char));

  for (int i = start; i <= (int)L->pos; i++) {
    number[i] = L->buf[i];
  }
  number[n] = '\0';

  return make(L, is_float ? TOK_CONST_FLOAT : TOK_CONST_INT, number, n, NULL);
}

Token scan_char(Lexer* L) {
  int start = (int)L->col;
  advance(L);
  int curr = advance(L);
  if (curr == '\\') advance(L);

  if (peek(L) == '\'') {
    advance(L);
    Token t = make(L, TOK_CONST_CHAR, "<char>", 6, NULL);
    t.col = start;
    return t;
  }

  return scan_invalid(L, "<unterminated_char>");
}


Token scan_operator(Lexer* L) {
  int start_col = L->col;
  int c = advance(L);
  if(c=='=' && match(L,'=')) { 
    Token t=make(L,TOK_OP_REL,"==",2,"=="); t.col=start_col; return t; 
    }
  if(c=='!' && match(L,'=')) { Token t=make(L,TOK_OP_REL,"!=",2,"!="); t.col=start_col; return t; }
  if(c=='<' && match(L,'=')) { Token t=make(L,TOK_OP_REL,"<=",2,"<="); t.col=start_col; return t; }
  if(c=='>' && match(L,'=')) { Token t=make(L,TOK_OP_REL,">=",2,">="); t.col=start_col; return t; }
  if(c=='&' && match(L,'&')) { Token t=make(L,TOK_OP_LOGIC,"&&",2,"&&"); t.col=start_col; return t; }
  if(c=='|' && match(L,'|')) { Token t=make(L,TOK_OP_LOGIC,"||",2,"||"); t.col=start_col; return t; }
  if(c=='*' && match(L,'*')) { Token t=make(L,TOK_OP_ARITH,"**",2,"**"); t.col=start_col; return t; }
  
  if(c=='='){ Token t=make(L,TOK_ASSIGN,"=",1,"="); t.col=start_col; return t; }
  if(c=='+'||c=='-'||c=='*'||c=='/'||c=='%'){ char s2[2]={(char)c,0};
    Token t=make(L,TOK_OP_ARITH,s2,1,s2); t.col=start_col; return t; }
  if(c=='<'||c=='>'){ char s2[2]={(char)c,0};
    Token t=make(L,TOK_OP_REL,s2,1,s2); t.col=start_col; return t; }
  if(c=='!'){ Token t=make(L,TOK_OP_LOGIC,"!",1,"!"); t.col=start_col; return t; }
  char s2[2]={(char)c,0}; return scan_invalid(L, s2);
}


Token scan_comment(Lexer* L) {
  int sc = L->col;
  if (match(L, '/')) {
    if (match(L, '/')) {
      while (peek(L) != EOF || peek(L) != '\n') advance(L);

      Token t;
      t = make(L, TOK_COMMENT, "//", 2, NULL);
      t.col = sc;

      return t;

    } else if (match(L, '*')) {
      int sec_last = L->pos, last = L->pos;
      
      while (peek(L) != EOF) {
        advance(L);
        sec_last = last;
        last = L->pos;
      }

      if (sec_last != '*' || last != '/') {
        return scan_invalid(L, "<unterminated_comment>");
      } else {
        Token t;
        t = make(L, TOK_COMMENT, "/* */", 5, NULL);
        t.col = sc;

        return t;
      }
    }
  }
}

Token scan_invalid(Lexer* L, char *tag) {
  return make(L, TOK_INVALID, tag, strlen(tag), NULL);
}

