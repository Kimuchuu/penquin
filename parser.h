#ifndef PENQUIN_PARSER_H
#define PENQUIN_PARSER_H

#include <stdbool.h>
#include "common.h"
#include "list.h"

typedef struct {
	bool pointer;
	String type;
	String name;
} Parameter;

typedef struct {
	List parameters;
	List statements;
} Function;

typedef struct {
	List statements;
} Block;

typedef enum {
    AST_NUMBER,
    AST_STRING,
    AST_VARIABLE,
    AST_FUNCTION,
    AST_WHILE,

    AST_OPERATOR,
    AST_ASSIGNMENT,
    AST_FUNCTION_CALL,
} AstType;


typedef struct AstNode {
    AstType type;
    union {
        float number;
        char operator;
        String string;
		List arguments;
		Block block;
		Function fn;
    } as;
    struct AstNode *left;
    struct AstNode *right;
} AstNode;


void parse(List *t, List *nodes);

#endif
