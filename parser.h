#ifndef PENQUIN_PARSER_H
#define PENQUIN_PARSER_H

#include <stdbool.h>
#include "common.h"
#include "list.h"
#include "token.h"
#include "table.h"

typedef struct Scope {
	struct Scope *prev;
	Table *locals;
	Table *definitions;
} Scope;

typedef enum {
	TYPE_VALUE,
	TYPE_ARRAY,
	TYPE_POINTER,
} TypeType;

typedef struct TypeInfo {
	TypeType type;
	union {
		struct {
			struct TypeInfo *of;
			int length;
		} array;
		struct TypeInfo *pointer_to;
		String value_of;
	};
} TypeInfo;

typedef struct {
	List items;
} Array;

typedef struct {
	String name;
	struct AstNode *value;
	struct AstNode *initial;
} Assignment;

typedef struct {
	List statements;
	Scope *scope;
} Block;

typedef struct {
	struct AstNode *left;
	struct AstNode *right;
} Binary;

typedef struct {
	char *path;
	List tokens;
	List nodes;
	Scope *scope;
} File;

typedef struct {
	String name;
	List parameters;
	List statements;
	struct Type *type;
	bool external;
	bool vararg;
	Scope *scope;
} Function;

typedef struct {
	struct AstNode *function;
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
	struct AstNode *file_node;
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
	bool rest;
	TypeInfo type_info;
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
	String name;
	struct AstNode *declaration;
} Variable;

typedef struct {
	struct AstNode *condition;
	struct AstNode *statement;
} While;

typedef enum {
    AST_ACCESSOR,
    AST_ARRAY,
    AST_ASSIGNMENT,
    AST_BOOL,
    AST_BLOCK,
    AST_FILE,
    AST_FUNCTION,
    AST_FUNCTION_CALL,
    AST_IF,
    AST_IMPORT,
    AST_ITEM_ACCESS,
    AST_NUMBER,
    AST_OPERATOR,
    AST_PARAMETER,
    AST_RETURN,
    AST_STRING,
    AST_VARIABLE,
    AST_WHILE,
} AstType;


typedef struct AstNode {
    AstType type;
    TypeInfo *type_info;
    void *backend_ref;
    union {
		Binary       accessor;
		Array        array;
        Assignment   assignment;
		Block        block;
		bool         bool_;
		FunctionCall call;
		File         file;
		Function     fn;
		If           if_;
		Import       import;
		ItemAccess   item_access;
        float        number;
		Operator     operator_;
        Parameter    parameter;
        Return       return_;
        String       string;
		Variable     variable;
		While        while_;
    } as;
} AstNode;


void     parse(List *t, List *nodes);
AstNode *parse_file(char *path, List *t);

#endif
