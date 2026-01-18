#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- Token from SymbolTable.txt ---------------- */

typedef struct {
    char lexeme[64];   /* text in the left column */
    char token[32];    /* text in the right column, e.g. "identifier", "type" */
} STToken;

/* ---------------- Variable type for semantics ---------------- */

typedef enum {
    VT_UNKNOWN = 0,
    VT_INT,
    VT_FLOAT,
    VT_BOOL,
    VT_CHAR
} VarType;

/* A single variable entry in our semantic symbol table */
typedef struct {
    char name[64];   /* variable name */
    VarType type;    /* its type */
} VarEntry;

/* Simple fixed-size storage (OK for demo) */
#define MAX_TOKENS  2000
#define MAX_VARS    256

STToken tokens[MAX_TOKENS];
int token_count = 0;

VarEntry vars[MAX_VARS];
int var_count = 0;

/* ---------------- Utility: trim newline from strings ---------------- */

static void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = '\0';
        n--;
    }
}

/* ---------------- Map type lexeme -> VarType ---------------- */

static VarType type_from_lexeme(const char *lex) {
    if      (strcmp(lex, "int")   == 0) return VT_INT;
    else if (strcmp(lex, "float") == 0) return VT_FLOAT;
    else if (strcmp(lex, "bool")  == 0) return VT_BOOL;
    else if (strcmp(lex, "char")  == 0) return VT_CHAR;
    /* void or others => unknown in this simple checker */
    return VT_UNKNOWN;
}

/* ---------------- Map token name -> VarType for RHS expr ---------------- */

static VarType type_from_token(const char *token, const char *lexeme) {
    /* literal constants */
    if (strcmp(token, "const_int")   == 0) return VT_INT;
    if (strcmp(token, "const_float") == 0) return VT_FLOAT;
    if (strcmp(token, "const_bool")  == 0) return VT_BOOL;
    if (strcmp(token, "const_char")  == 0) return VT_CHAR;

    /* identifiers: look up in our semantic symbol table */
    if (strcmp(token, "identifier") == 0) {
        for (int i = 0; i < var_count; i++) {
            if (strcmp(vars[i].name, lexeme) == 0) {
                return vars[i].type;
            }
        }
        /* use before declare -> unknown (will trigger error later) */
        return VT_UNKNOWN;
    }

    /* default */
    return VT_UNKNOWN;
}

/* ---------------- Symbol table operations ---------------- */

static int find_var(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_var(const char *name, VarType t) {
    if (var_count >= MAX_VARS) return;  /* too many, ignore for demo */
    strncpy(vars[var_count].name, name, sizeof(vars[var_count].name)-1);
    vars[var_count].name[sizeof(vars[var_count].name)-1] = '\0';
    vars[var_count].type = t;
    var_count++;
}

/* ---------------- Step 1: read SymbolTable.txt into tokens[] ---------------- */

static int load_tokens(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", path);
        return 0;
    }

    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);

        /* Skip header/footer lines */
        if (line[0] == '+' || line[0] == '\0') continue;
        if (strncmp(line, "Source:", 7) == 0)  continue;
        if (line[0] != '|') continue; /* not a table row */

        /* Parse: | <lexeme> | <token> | */
        STToken tok;
        memset(&tok, 0, sizeof(tok));

        /* We assume lexeme and token have no spaces except inside long strings.
           For semantics we only care about identifiers/types/literals/operators,
           so %s parsing is OK. */
        char lex[64], type[32];
        /* note: %63s means "up to 63 non-space characters" */
        if (sscanf(line, "| %63s | %31s |", lex, type) == 2) {
            strncpy(tok.lexeme, lex, sizeof(tok.lexeme)-1);
            strncpy(tok.token,  type, sizeof(tok.token)-1);
            tokens[token_count++] = tok;
            if (token_count >= MAX_TOKENS) break;
        }
    }

    fclose(fp);
    return 1;
}

/* ---------------- Step 2: build semantic symbol table ---------------- */

static void build_symbol_table(void) {
    for (int i = 0; i < token_count; i++) {
        /* Look for pattern:   type identifier ;  */
        if (strcmp(tokens[i].token, "type") == 0) {
            if (i + 1 < token_count && strcmp(tokens[i+1].token, "identifier") == 0) {
                const char *type_lex = tokens[i].lexeme;
                const char *name_lex = tokens[i+1].lexeme;

                VarType t = type_from_lexeme(type_lex);
                int idx = find_var(name_lex);

                if (idx != -1) {
                    printf("[Semantic Error] Duplicate declaration of '%s'\n", name_lex);
                } else {
                    add_var(name_lex, t);
                    printf("[Declare] %s %s\n", type_lex, name_lex);
                }
            }
        }
    }
}

/* ---------------- Step 3: check assignments ---------------- */

static void check_assignments(void) {
    for (int i = 0; i < token_count; i++) {
        /* Look for pattern: identifier = <expr> ;   */
        if (strcmp(tokens[i].token, "identifier") == 0) {
            const char *name_lex = tokens[i].lexeme;

            /* ensure there is '=' and another token after it */
            if (i + 2 < token_count &&
                strcmp(tokens[i+1].lexeme, "=") == 0 &&
                strcmp(tokens[i+1].token,  "operator") == 0) {

                /* Find declared type of the variable on the left */
                int idx = find_var(name_lex);
                if (idx == -1) {
                    printf("[Semantic Error] Variable '%s' used before declaration (assignment)\n",
                           name_lex);
                    continue;
                }

                VarType left_type = vars[idx].type;
                VarType right_type = type_from_token(tokens[i+2].token, tokens[i+2].lexeme);

                if (right_type == VT_UNKNOWN) {
                    printf("[Semantic Warning] Cannot determine type of right-hand side for '%s'\n",
                           name_lex);
                } else if (left_type != VT_UNKNOWN && left_type != right_type) {
                    printf("[Semantic Error] Type mismatch in assignment to '%s' (left is %d, right is %d)\n",
                           name_lex, left_type, right_type);
                } else {
                    printf("[OK] Assignment to '%s' is type-safe.\n", name_lex);
                }
            }
        }
    }
}

/* ---------------- main ---------------- */

int main(void) {
    if (!load_tokens("SymbolTable.txt")) {
        fprintf(stderr, "Make sure SymbolTable.txt exists (run the lexer first).\n");
        return 1;
    }

    printf("Loaded %d tokens from SymbolTable.txt\n\n", token_count);

    printf("=== Building semantic symbol table ===\n");
    build_symbol_table();

    printf("\n=== Checking assignments ===\n");
    check_assignments();

    printf("\nDone.\n");
    return 0;
}
