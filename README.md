K# Lexical Analyzer (K# Lexer) — Group 2 

The K# Lexer is a custom-built lexical analyzer created in the C programming language as part of our Principles of Programming Languages project. 

It reads a source file written in our proposed language K#, scans characters using Deterministic Finite Automata (DFAs), classifies code into tokens, and outputs a clean SymbolTable.txt containing every token with its corresponding classification.  

What the K# Lexer Does The lexer performs the following major functions: 

1. Reads a .ksh Source File It only accepts files ending in .ksh. Any other file type results in an error. 
2. Converts Raw Text Into Tokens
 
 It identifies and classifies lexemes into categories: 
 
 Keywords 
 Identifiers 
 Reserved Types 
 Numeric Constants (int, float) 
 Boolean Constants (true/false) 
 Character Literals 
 String Literals 
 Noise Words (ignored words like please, of, then…) 
 Operators (arithmetic, logical, relational, assignment) 
 Delimiters (; , . :) Brackets (() [] {}) 
 Comments (//, /* */) Unknown / Invalid symbols 
 
 3. Ignores Whitespace and Comments Whitespace and comments are treated as boundaries—not tokens. 
 4. Writes a Formatted Symbol Table
 
 
-------------------------------------------------------------------------------------------------------------------------------------------
 
 
 K# SEMANTIC ANALYZER (Additional Points)
 
  - This file is just a demo of simple semantic checks.
 
  How it works:
  1. Reads SymbolTable.txt (output of the lexer).
  2. Rebuilds a list of tokens (lexeme + token-type string).
  3. Builds a symbol table of variables from declarations:  <type> <identifier> ;
  4. Checks:
       - duplicate declarations
       - type mismatch in simple assignments:  identifier = value ;
 
  Limitations:
  - Very small grammar: only handles straight declarations and assignments.
  - No full parsing, no scopes, no functions, no arrays.
  - This is for demonstration, not for your project grade.
 