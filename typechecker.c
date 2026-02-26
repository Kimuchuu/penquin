#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include "common.h"

static void parse_node(AstNode *node);


static void parse_accessor(AstNode *node) {
	
}

static void parse_array(AstNode *node) {
	
}


static void parse_assignment(AstNode *node) {
	
}


static void parse_block(AstNode *node) {
	
}


static void parse_file_node(AstNode *node) {
	
}


static void parse_function(AstNode *node) {
	
}


static void parse_function_call(AstNode *node) {

}


static void parse_if(AstNode *node) {
	
}


static void parse_import(AstNode *node) {
	
}


static void parse_item_access(AstNode *node) {
	
}


static TypeInfo *create_type_info(char *name, bool pointer) {
	TypeInfo *type_info = malloc(sizeof(TypeInfo));
	type_info->type.pointer = pointer;
	type_info->type.name = STRING(name);
	return type_info;
}

static void parse_number(AstNode *node) {
	node->type_info = create_type_info("S4", false);
}

static void parse_operator(AstNode *node) {
	parse_node(node->as.operator_.left);
	parse_node(node->as.operator_.right);

	switch (node->as.operator_.type) {
		case TOKEN_PLUS:
		case TOKEN_MINUS:
		case TOKEN_STAR:
		case TOKEN_SLASH:
			node->type_info = create_type_info("S4", false);
			break;
		case TOKEN_DOUBLE_EQUAL:
		case TOKEN_LESS_THAN:
		case TOKEN_LESS_THAN_OR_EQUAL:
		case TOKEN_GREATER_THAN:
		case TOKEN_GREATER_THAN_OR_EQUAL:
			node->type_info = create_type_info("S2", false);
			break;;
		default:
			break;
	}
}

static void parse_return(AstNode *node) {
	parse_node(node->as.return_.expression);
}

static void parse_string(AstNode *node) {
	node->type_info = create_type_info("S1", true);
}

static void parse_variable(AstNode *node) {
}

static void parse_while(AstNode *node) {
	parse_node(node->as.while_.condition);
	parse_node(node->as.while_.statement);
}

static void parse_node(AstNode *node) {
	switch (node->type) {
		case AST_ACCESSOR:
			parse_accessor(node);
			break;
		case AST_ARRAY:
			parse_array(node);
			break;
		case AST_ASSIGNMENT:
			parse_assignment(node);
			break;
		case AST_BLOCK:
			parse_block(node);
			break;
		case AST_FILE:
			parse_file_node(node);
			break;
		case AST_FUNCTION:
			parse_function(node);
			break;
		case AST_FUNCTION_CALL:
			parse_function_call(node);
			break;
		case AST_IF:
			parse_if(node);
			break;
		case AST_IMPORT:
			parse_import(node);
			break;
		case AST_ITEM_ACCESS:
			parse_item_access(node);
			break;
		case AST_NUMBER:
			parse_number(node);
			break;
			parse_number(node);
		case AST_OPERATOR:
			parse_operator(node);
			break;
		case AST_RETURN:
			parse_return(node);
			break;
		case AST_STRING:
			parse_string(node);
			break;
		case AST_VARIABLE:
			parse_variable(node);
			break;
		case AST_WHILE:
			parse_while(node);
			break;
		default:
			printf("wft, no bitches");
			exit(1);
    }
}

static void type_check(AstNode *node) {
	parse_node(node);
}
