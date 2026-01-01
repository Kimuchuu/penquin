#ifndef PENQUIN_PARSER_H
#define PENQUIN_PARSER_H

#include <stdbool.h>
#include "common.h"
#include "list.h"
#include "token.h"

typedef struct {
	List statements;
} Block;

typedef struct {
	struct AstNode *left;
	struct AstNode *right;
} Binary;

typedef struct {
	char *path;
	List tokens;
	List nodes;
} File;

typedef struct {
	struct AstNode *variable;
	List parameters;
	List statements;
	struct Type *type;
} Function;

typedef struct {
	struct AstNode *variable;
	List arguments;
} FunctionCall;

typedef struct {
	struct AstNode *condition;
	struct AstNode *statement;
	struct AstNode *else_statement;
} If;

typedef struct {
	String path;
} Import;

typedef struct {
	TokenType type;
	struct AstNode *left;
	struct AstNode *right;
} Operator;

typedef struct {
	bool pointer;
	String type;
	String name;
} Parameter;

typedef struct Type {
	bool pointer;
	String name;
} Type;

typedef struct {
	struct AstNode *condition;
	struct AstNode *statement;
} While;

typedef enum {
    AST_ACCESSOR,
    AST_ASSIGNMENT,
    AST_BLOCK,
    AST_FILE,
    AST_FUNCTION,
    AST_FUNCTION_CALL,
    AST_IF,
    AST_IMPORT,
    AST_NUMBER,
    AST_OPERATOR,
    AST_STRING,
    AST_VARIABLE,
    AST_WHILE,
} AstType;


typedef struct AstNode {
    AstType type;
    union {
		Binary       accessor;
        Binary       assignment;
		Block        block;
		FunctionCall call;
		File         file;
		Function     fn;
		If           if_;
		Import       import;
        float        number;
		Operator     operator_;
        String       string;
		While        while_;
    } as;
} AstNode;


void     parse(List *t, List *nodes);
AstNode *parse_file(char *path, List *t);

#endif
