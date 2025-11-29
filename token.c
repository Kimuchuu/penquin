#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include "token.h"

#define CASE_TOKEN(token) case TOKEN_##token: return #token


typedef struct {
    char *source;
    int start;
    int current;
    int line;
    int col;
} Scanner;


Scanner scanner;
List *list;


const char *token_type_to_string(enum TokenType type) {
    switch (type) {
        CASE_TOKEN(LEFT_BRACE);
        CASE_TOKEN(RIGHT_BRACE);
        CASE_TOKEN(LEFT_PAREN);
        CASE_TOKEN(RIGHT_PAREN);
        CASE_TOKEN(COLON);
        CASE_TOKEN(SEMICOLON);
        CASE_TOKEN(COMMA);

        CASE_TOKEN(EQUAL);
        CASE_TOKEN(PLUS);
        CASE_TOKEN(MINUS);
        CASE_TOKEN(STAR);
        CASE_TOKEN(SLASH);
        CASE_TOKEN(PERCENT);
        CASE_TOKEN(LESS_THAN);
        CASE_TOKEN(GREATER_THAN);

        CASE_TOKEN(NUMBER);
        CASE_TOKEN(STRING);
        CASE_TOKEN(IDENTIFIER);
    	CASE_TOKEN(FUN);
    	CASE_TOKEN(WHILE);

        CASE_TOKEN(ERROR);
        CASE_TOKEN(EOF);
        default: return "INVALID";
    }
}

static void skip_whitespace() {
    char c;
    while (isspace(c = scanner.source[scanner.current])) {
        if (c == '\n') {
            scanner.line++;
            scanner.col = 0;
        }
        scanner.col++;
        scanner.current++;
    }
}

static void scan_identifier() {
    char c;
    while (isalnum(c = scanner.source[scanner.current]) || c == '_') {
        scanner.current++;
    }
}

static void scan_number() {
    while (isdigit(scanner.source[scanner.current])) {
        scanner.current++;
    }
    if (scanner.source[scanner.current] == '.') {
        scanner.current++;
        while (isdigit(scanner.source[scanner.current])) {
            scanner.current++;
        }
    }
}

static bool scan_string() {
    char c;
    while ((c = scanner.source[scanner.current]) != '\0' && c != '"') {
        scanner.current++;
    }
    if (c == '"') {
        scanner.current++;
        return true;
    }
    return false;
}


static void scan_token() {
    skip_whitespace();
    int start = scanner.current++;
    char c = scanner.source[start];

    Token token;
    token.line = scanner.line;
    token.col = scanner.col++;
    token.raw = scanner.source + start;

    switch (c) {
        case '{':
            token.type = TOKEN_LEFT_BRACE;
            break;
        case '}':
            token.type = TOKEN_RIGHT_BRACE;
            break;
        case '(':
            token.type = TOKEN_LEFT_PAREN;
            break;
        case ')':
            token.type = TOKEN_RIGHT_PAREN;
            break;
        case ':':
            token.type = TOKEN_COLON;
            break;
        case ';':
            token.type = TOKEN_SEMICOLON;
            break;
        case ',':
            token.type = TOKEN_COMMA;
            break;
        case '=':
            token.type = TOKEN_EQUAL;
            break;
        case '+':
            token.type = TOKEN_PLUS;
            break;
        case '-':
            token.type = TOKEN_MINUS;
            break;
        case '*':
            token.type = TOKEN_STAR;
            break;
        case '/':
            token.type = TOKEN_SLASH;
            break;
        case '%':
            token.type = TOKEN_PERCENT;
            break;
        case '<':
            token.type = TOKEN_LESS_THAN;
            break;
        case '>':
            token.type = TOKEN_GREATER_THAN;
            break;
        case '"':
            token.type = scan_string() ? TOKEN_STRING : TOKEN_ERROR;
            break;
        case '\0':
            token.type = TOKEN_EOF;
            break;
        default:
            if (isalpha(c) || c == '_') {
                scan_identifier();
				int len = scanner.current - start;
				if (len == 3 && strncmp(token.raw, "fun", 3) == 0) {
					token.type = TOKEN_FUN;
				} else if (len == 5 && strncmp(token.raw, "while", 5) == 0) {
					token.type = TOKEN_WHILE;
				} else {
					token.type = TOKEN_IDENTIFIER;
				}
            } else if (isdigit(c)) {
                scan_number();
                token.type = TOKEN_NUMBER;
            } else {
                token.type = TOKEN_ERROR;
            }
            break;
    }
    token.length = scanner.current - start;
    list_add(list, &token);
}


void scan(char *source, List *tokens) {
    scanner.source = source;
    scanner.start = 0;
    scanner.current = 0;
    scanner.line = 1;
    scanner.col = 1;
    list = tokens;

    do {
        scan_token();
    } while (LIST_GET(Token, list, list->length - 1).type != TOKEN_EOF);
}
