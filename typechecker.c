#include <assert.h>
#include <stdlib.h>
#include "typechecker.h"

static TypeInfo *value_of(char *name) {
	TypeInfo *type = malloc(sizeof(TypeInfo));
	type->type = TYPE_VALUE;
	type->value_of = STRING(name);
	return type;
}

static TypeInfo *pointer_to(TypeInfo *to) {
	TypeInfo *type = malloc(sizeof(TypeInfo));
	type->type = TYPE_POINTER;
	type->pointer_to = to;
	return type;
}

static TypeInfo *array_of(TypeInfo *of, int length) {
	TypeInfo *type = malloc(sizeof(TypeInfo));
	type->type = TYPE_ARRAY;
	type->array.of = of;
	type->array.length = length;
	return type;
}

static void parse_node(AstNode *node);

static void parse_accessor(AstNode *node) {
	parse_node(node->as.accessor.left);
	parse_node(node->as.accessor.right);
	node->type_info = node->as.accessor.right->type_info;
}

static void parse_array(AstNode *node) {
	AstNode *i_node;
	for (int i = 0; i < node->as.array.items.length; i++) {
		i_node = LIST_GET(AstNode *, &node->as.array.items, i);
		parse_node(i_node);
	}
	node->type_info = array_of(i_node->type_info, node->as.array.items.length);
}

static void parse_assignment(AstNode *node) {
	parse_node(node->as.assignment.value);
	node->type_info = node->as.assignment.value->type_info;
	// TODO: fail if reassigning with another, incompatible type
}

static void parse_block(AstNode *node) {
	for (int i = 0; i < node->as.block.statements.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &node->as.block.statements, i);
		parse_node(i_node);
	}
}


static void parse_file_node(AstNode *node) {
	for (int i = 0; i < node->as.file.nodes.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &node->as.file.nodes, i);
		parse_node(i_node);
	}
}

static void parse_function(AstNode *node) {
	// TODO: fix/refactor type structs
	if (node->as.fn.type != NULL) {
		char *type_name = String_to_cstring(node->as.fn.type->name);
		if (node->as.fn.type->pointer) {
			node->type_info = pointer_to(value_of(type_name));
		} else {
			node->type_info = value_of(type_name);
		}
	}

	for (int i = 0; i < node->as.fn.parameters.length; i++) {
		AstNode *parameter_node = LIST_GET(AstNode *, &node->as.fn.parameters, i);
		parse_node(parameter_node);
	}

	for (int i = 0; i < node->as.fn.statements.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &node->as.fn.statements, i);
		parse_node(i_node);
	}
}

static void parse_function_call(AstNode *node) {
	parse_node(node->as.call.variable);
	node->type_info = node->as.call.variable->type_info;
	for (int i = 0; i < node->as.call.arguments.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &node->as.call.arguments, i);
		parse_node(i_node);
	}
}


static void parse_if(AstNode *node) {
	parse_node(node->as.if_.condition);
	parse_node(node->as.if_.statement);
	if (node->as.if_.else_statement != NULL) {
		parse_node(node->as.if_.else_statement);
	}
}

static void parse_import(AstNode *node) {
}

static void parse_item_access(AstNode *node) {
	parse_node(node->as.item_access.indexable);
	parse_node(node->as.item_access.index);
	node->type_info = node->as.item_access.indexable->type_info->pointer_to;
}

static void parse_number(AstNode *node) {
	node->type_info = value_of("s4");
}

static void parse_operator(AstNode *node) {
	parse_node(node->as.operator_.left);
	parse_node(node->as.operator_.right);

	switch (node->as.operator_.type) {
		case TOKEN_PLUS:
		case TOKEN_MINUS:
		case TOKEN_STAR:
		case TOKEN_SLASH:
			node->type_info = value_of("s4");
			break;
		case TOKEN_DOUBLE_EQUAL:
		case TOKEN_LESS_THAN:
		case TOKEN_LESS_THAN_OR_EQUAL:
		case TOKEN_GREATER_THAN:
		case TOKEN_GREATER_THAN_OR_EQUAL:
			node->type_info = value_of("bool");
			break;
		default:
			break;
	}
}

static void parse_parameter(AstNode *node) {
	node->type_info = &node->as.parameter.type_info;
}

static void parse_return(AstNode *node) {
	parse_node(node->as.return_.expression);
}

static void parse_string(AstNode *node) {
	node->type_info = pointer_to(value_of("s1"));
}

static void parse_variable(AstNode *node) {
	node->type_info = node->as.variable.declaration->type_info;
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
		case AST_PARAMETER:
			parse_parameter(node);
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
    }
}

void resolve_types(AstNode *node) {
	parse_node(node);
}
