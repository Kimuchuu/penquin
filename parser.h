#ifndef PENQUIN_PARSER_H
#define PENQUIN_PARSER_H

#include <stdbool.h>
#include "common.h"
#include "list.h"
#include "token.h"

typedef struct {
	List items;
} Array;

typedef struct {
	String name;
	struct AstNode *value;
} Assignment;

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
	String name;
	List parameters;
	List statements;
	struct Type *type;
	bool external;
	bool vararg;
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
	struct AstNode *indexable;
	struct AstNode *index;
} ItemAccess;

typedef struct {
	TokenType type;
	struct AstNode *left;
	struct AstNode *right;
} Operator;

typedef struct {
	bool pointer;
	bool rest;
	String type;
	String name;
} Parameter;

typedef struct {
	struct AstNode *expression;
} Return;

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
    AST_ARRAY,
    AST_ASSIGNMENT,
    AST_BLOCK,
    AST_FILE,
    AST_FUNCTION,
    AST_FUNCTION_CALL,
    AST_IF,
    AST_IMPORT,
    AST_ITEM_ACCESS,
    AST_NUMBER,
    AST_OPERATOR,
    AST_RETURN,
    AST_STRING,
    AST_VARIABLE,
    AST_WHILE,
} AstType;


typedef struct AstNode {
    AstType type;
    union {
		Binary       accessor;
		Array        array;
        Assignment   assignment;
		Block        block;
		FunctionCall call;
		File         file;
		Function     fn;
		If           if_;
		Import       import;
		ItemAccess   item_access;
        float        number;
		Operator     operator_;
        Return       return_;
        String       string;
		While        while_;
    } as;
} AstNode;


void     parse(List *t, List *nodes);
AstNode *parse_file(char *path, List *t);

#endif
