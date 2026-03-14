#include <assert.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include "resolver.h"
#include "common.h"
#include "list.h"
#include "parser.h"
#include "table.h"

static char *module_path;
static char *module_dir;
static Scope *current_scope;
static Scope global_scope;
static Table *modules;

char *resolve_module_path(char *dir, String module_name) {
	if (String_starts_with(module_name, "std:")) {
		int size = 2 + module_name.length - 4 + 3 + 1;
		char *full_path = malloc(size);
		memcpy(full_path, "./std/", 6);
		memcpy(full_path + 6, module_name.p + 4, module_name.length - 4);
		memcpy(full_path + 6 + module_name.length - 4, ".pq", 4);
		return full_path;
	}

	int dirlen = strlen(dir);
	int size = dirlen + 1 + module_name.length + 3 + 1;
	char *full_path = malloc(size);
	memcpy(full_path, dir, dirlen);
	full_path[dirlen] = '/';
	memcpy(full_path + dirlen + 1, module_name.p, module_name.length);
	memcpy(full_path + dirlen + 1 + module_name.length, ".pq", 4);
	return full_path;
}

static char *resolve_identifier_name(char *path, String name) {
	int plen = strlen(path);
	char prefix[plen + 2];
	memcpy(prefix, path, plen);
	prefix[plen] = '@';
	prefix[plen + 1] = '\0';
	return cstring_concat_String(prefix, name);
}

static char *resolve_identifier(String name, bool external) {
	if (external || String_cmp_cstring(name, "main") == 0) {
		return String_to_cstring(name);
	} else {
		return resolve_identifier_name(module_path, name);
	}
}

static AstNode *lookup_identifier(String name) {
	char *global_name = resolve_identifier(name, false);

	AstNode *declaration_node = NULL;
	Scope *scope = current_scope;
	while (scope != NULL && declaration_node == NULL) {
		String lookup_name = scope->locals == global_scope.locals ? STRING(global_name) : name;
		declaration_node = (AstNode *)table_get(scope->locals, lookup_name);
		scope = scope->prev;
	}

	if (declaration_node == NULL) {
		// External declarations, could be moved to separate private place
		declaration_node = (AstNode *)table_get(global_scope.locals, name);
	}

	free(global_name);
	return declaration_node;
}

static Scope *create_scope() {
	Table *locals = malloc(sizeof(Table));
    table_init(locals);

    Scope *block_scope = malloc(sizeof(Scope));
    block_scope->prev = current_scope;
    block_scope->locals = locals;
    current_scope = block_scope;
    return block_scope;
}

static void parse_node(AstNode *node);

static void parse_accessor(AstNode *node) {
	AstNode *file_node = lookup_identifier(node->as.accessor.left->as.variable.name);
	File *file = &file_node->as.file;
	char *current_module_path = module_path;
	module_path = file->path;

	// Looks up in current file
	parse_node(node->as.accessor.left);

	Scope *scope = current_scope;
	current_scope = file->scope;

	// Looks up in current file
	parse_node(node->as.accessor.right);

	current_scope = scope;
	module_path = current_module_path;
}

static void parse_array(AstNode *node) {
	for (int i = 0; i < node->as.array.items.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &node->as.array.items, i);
		parse_node(i_node);
	}
}

static void parse_assignment(AstNode *node) {
	AstNode *declaration_node = lookup_identifier(node->as.assignment.name);
	if (declaration_node == NULL) {
		char *name = resolve_identifier(node->as.assignment.name, current_scope->locals != global_scope.locals);
		table_put(current_scope->locals, STRING(name), node);
		declaration_node = node;
	}
	node->as.assignment.initial = declaration_node;
	parse_node(node->as.assignment.value);
}

static void parse_block(AstNode *node) {
    node->as.block.scope = create_scope();
	for (int i = 0; i < node->as.block.statements.length; i++) {
		AstNode *stmnt = LIST_GET(AstNode *, &node->as.block.statements, i);
		parse_node(stmnt);
	}
	current_scope = current_scope->prev;
}

static void parse_file_node(AstNode *node) {
	module_path = node->as.file.path;
	node->as.file.scope = create_scope();
	for (int i = 0; i < node->as.file.nodes.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &node->as.file.nodes, i);
		parse_node(i_node);
	}
	current_scope = current_scope->prev;
	module_path = NULL;
}

static void parse_function(AstNode *node) {
	char *name = resolve_identifier(node->as.fn.name, node->as.fn.external);

	// TODO: put private functions in file scope
	table_put(global_scope.locals, STRING(name), node);

	node->as.fn.scope = create_scope();
	if (node->as.fn.statements.elements != NULL) {
		for (int i = 0; i < node->as.fn.parameters.length; i++) {
			AstNode *parameter = LIST_GET(AstNode *, &node->as.fn.parameters, i);
			parse_node(parameter);
		}
		
		for (int i = 0; i < node->as.fn.statements.length; i++) {
			parse_node(LIST_GET(AstNode *, &node->as.fn.statements, i));
		}
	}
	current_scope = current_scope->prev;
}

static AstNode *get_declaration(AstNode *node) {
	if (node->type == AST_ACCESSOR) {
		return get_declaration(node->as.accessor.right);
	}
	return node->as.variable.declaration;
}

static void parse_function_call(AstNode *node) {
	parse_node(node->as.call.variable);
	node->as.call.function = get_declaration(node->as.call.variable);
	for (int i = 0; i < node->as.call.arguments.length; i++) {
		parse_node(LIST_GET(AstNode *, &node->as.call.arguments, i));
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
	char *import_path = resolve_module_path(module_dir, node->as.import.path);
	char *identifier = path_to_name(import_path);
	AstNode *file_node = table_get(modules, STRING(import_path));
	assert(file_node != NULL);
	node->as.import.file_node = file_node;
	table_put(current_scope->locals, STRING(identifier), file_node);
}

static void parse_item_access(AstNode *node) {
	parse_node(node->as.item_access.index);
	parse_node(node->as.item_access.indexable);
}

static void parse_number(AstNode *node) {}

static void parse_operator(AstNode *node) {
	parse_node(node->as.operator_.left);
	parse_node(node->as.operator_.right);
}

static void parse_parameter(AstNode *node) {
	table_put(current_scope->locals, node->as.parameter.name, node);
}

static void parse_return(AstNode *node) {
	parse_node(node->as.return_.expression);
}

static void parse_string(AstNode *node) {}

static void parse_variable(AstNode *node) {
	AstNode *declaration = lookup_identifier(node->as.variable.name);
	DEFINE_CSTRING(variable, node->as.variable.name);
	assert(declaration != NULL);
	node->as.variable.declaration = declaration;
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

void resolve(AstNode *node, char *dir) {
	module_dir = dir;
	parse_node(node);
}

void resolver_initialize(Table *modules_) {
	modules = modules_;
	global_scope.prev = NULL;
	global_scope.locals = malloc(sizeof(Table));
	current_scope = &global_scope;
    table_init(global_scope.locals);
}
