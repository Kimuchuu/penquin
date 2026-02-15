#ifndef PENQUIN_TOKEN_H
#define PENQUIN_TOKEN_H

#include "list.h"

typedef enum {
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_DOUBLE_COLON,
    TOKEN_DOUBLE_EQUAL,
	TOKEN_ELSE,
    TOKEN_EOF,
    TOKEN_EQUAL,
    TOKEN_ERROR,
    TOKEN_EXTERN,
    TOKEN_FUN,
    TOKEN_GREATER_THAN,
    TOKEN_GREATER_THAN_OR_EQUAL,
    TOKEN_IDENTIFIER,
	TOKEN_IF,
	TOKEN_IMPORT,
    TOKEN_LEFT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_LEFT_PAREN,
    TOKEN_LESS_THAN,
    TOKEN_LESS_THAN_OR_EQUAL,
    TOKEN_MINUS,
    TOKEN_NUMBER,
    TOKEN_PERCENT,
    TOKEN_PLUS,
	TOKEN_RETURN,
    TOKEN_RIGHT_BRACE,
    TOKEN_RIGHT_BRACKET,
    TOKEN_RIGHT_PAREN,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,
    TOKEN_STRING,
    TOKEN_TRIPLE_DOT,
    TOKEN_WHILE,
} TokenType;

typedef struct {
    int line;
    int col;
    TokenType type;
    int length;
    char *raw;
} Token;

const char *token_type_to_string(TokenType type);
void scan(char *source, List *tokens);

#endif
