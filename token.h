#ifndef PENQUIN_TOKEN_H
#define PENQUIN_TOKEN_H

#include "list.h"

typedef enum {
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COLON,
    TOKEN_DOUBLE_COLON,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,

    TOKEN_EQUAL,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_DOUBLE_EQUAL,
    TOKEN_LESS_THAN,
    TOKEN_GREATER_THAN,
    TOKEN_LESS_THAN_OR_EQUAL,
    TOKEN_GREATER_THAN_OR_EQUAL,

    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    TOKEN_ERROR,
    TOKEN_EOF,

	TOKEN_IMPORT,

    TOKEN_FUN,
    TOKEN_WHILE,
	TOKEN_IF,
	TOKEN_ELSE,
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
