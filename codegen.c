#include <assert.h>
#include <llvm-c/Types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include "codegen.h"
#include "list.h"
#include "parser.h"
#include "table.h"

#define DEFINE_CSTRING(name, string) char name[string.length + 1];\
									 memcpy(name, string.p, string.length);\
								  	 name[string.length] = '\0';
#define MALLOC_CSTRING(name, string) char *name = malloc(string.length + 1);\
									 memcpy(name, string.p, string.length);\
								  	 name[string.length] = '\0';

static LLVMContextRef context;
static LLVMBuilderRef builder;
static LLVMModuleRef module;
static LLVMValueRef *current_function;
Table *locals;
Table globals;
Table types;

static LLVMValueRef parse_node(AstNode *node);

static void report_invalid_node(const char *message) {
    fprintf(stderr, "[CODEGEN] %s", message);
    exit(1);
}

static LLVMValueRef parse_number(AstNode *node) {
    if (node->type != AST_NUMBER) {
        report_invalid_node("Expected number");
    }
    LLVMTypeRef int_type = LLVMInt32TypeInContext(context);
    return LLVMConstInt(int_type, node->as.number, 1);
}

static LLVMValueRef parse_string(AstNode *node) {
    if (node->type != AST_STRING) {
        report_invalid_node("Expected string");
    }

	LLVMValueRef str = LLVMConstStringInContext2(context,
												 node->as.string.p,
											  	 node->as.string.length,
											  	 0);
	LLVMValueRef pointer = LLVMBuildAlloca(builder, LLVMTypeOf(str), "");
	LLVMBuildStore(builder, str, pointer);
	return pointer;
}

static LLVMValueRef parse_operator(AstNode *node) {
	LLVMValueRef left = parse_node(node->left);
	LLVMValueRef right = parse_node(node->right);

	switch (node->as.operator) {
		case '+':
			return LLVMBuildAdd(builder, left, right, "");
		case '-':
			return LLVMBuildSub(builder, left, right, "");
		case '*':
			return LLVMBuildMul(builder, left, right, "");
		case '/':
			return LLVMBuildSDiv(builder, left, right, "");
		default:
			report_invalid_node("Unexpected identifier in operator node");
	}
	return NULL; // Will never hit, but just to appease the warning gods
}

static LLVMValueRef parse_variable(AstNode *node) {
	DEFINE_CSTRING(name, node->as.string)

	LLVMValueRef value_pointer = NULL;
	if (locals != NULL) {
		value_pointer = (LLVMValueRef)table_get(locals, name);
	}
	if (value_pointer == NULL) {
		value_pointer = (LLVMValueRef)table_get(&globals, name);
	}
	return LLVMBuildLoad2(
		builder,
		LLVMGetAllocatedType(value_pointer),
		value_pointer,
		""
	);
}

static LLVMValueRef parse_assignment(AstNode *node) {
	if (node->left->type != AST_VARIABLE) {
		report_invalid_node("Expected left of assignment to be variable");
	}

	LLVMValueRef value_node = parse_node(node->right);

	MALLOC_CSTRING(name, node->left->as.string)

	LLVMValueRef pointer = NULL;
	if (locals != NULL) {
		pointer = (LLVMValueRef)table_get(locals, name);
	}
	if (pointer == NULL) {
		pointer = (LLVMValueRef)table_get(&globals, name);
	}
	if (pointer == NULL) {
		pointer = LLVMBuildAlloca(builder, LLVMTypeOf(value_node), name);
	}
	LLVMBuildStore(builder, value_node, pointer);
	if (locals != NULL && table_get(locals, name) != NULL || table_get(&globals, name) == NULL) {
		table_put(locals, name, pointer);
	} else {
		table_put(&globals, name, pointer);
	}

	return value_node;
}

static LLVMValueRef parse_function_call(AstNode *node) {
	DEFINE_CSTRING(name, node->left->as.string)

	LLVMValueRef fn = table_get(&globals, name);
	LLVMTypeRef fn_type = LLVMGlobalGetValueType(fn);

	LLVMValueRef args[node->as.arguments.length];
	for (int i = 0; i < node->as.arguments.length; i++) {
		args[i] = parse_node(LIST_GET(AstNode *, &node->as.arguments, i));
	}

	return LLVMBuildCall2(builder, fn_type, fn, args, node->as.arguments.length, "");
}

static LLVMValueRef parse_function(AstNode *node) {
	MALLOC_CSTRING(name, node->left->as.string)

	LLVMTypeRef return_type = NULL;
	if (node->right == NULL) {
		return_type = LLVMVoidTypeInContext(context);
	} else {
		DEFINE_CSTRING(return_name, node->right->as.string)
		return_type = table_get(&types, return_name);
	}

	Table *tmp_locals = locals;
	Table fn_locals;
	locals = &fn_locals;
    table_init(locals);

	LLVMTypeRef *parameters = NULL;
	if (node->as.fn.parameters.length != 0) {
		parameters = malloc(sizeof(LLVMTypeRef) * node->as.fn.parameters.length);
		for (int i = 0; i < node->as.fn.parameters.length; i++) {
			Parameter parameter = LIST_GET(Parameter, &node->as.fn.parameters, i);
			DEFINE_CSTRING(type_name, parameter.type)
			parameters[i] = table_get(&types, type_name);
			if (parameter.pointer) {
				parameters[i] = LLVMPointerType(parameters[i], 0);
			}
		}
	}

	LLVMTypeRef fn_type = LLVMFunctionType(
		return_type,
		parameters,
		node->as.fn.parameters.length,
		0
	);
    LLVMValueRef fn = LLVMAddFunction(module, name, fn_type);
	current_function = &fn;
	table_put(&globals, name, fn);

	if (node->as.fn.statements.elements != NULL) {
		LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(context, fn, "");
		LLVMPositionBuilderAtEnd(builder, block);


		for (int i = 0; i < node->as.fn.parameters.length; i++) {
			Parameter parameter = LIST_GET(Parameter, &node->as.fn.parameters, i);
			MALLOC_CSTRING(param_name, parameter.name)
			LLVMValueRef param_value = LLVMGetParam(fn, i);
			LLVMValueRef param_pointer = LLVMBuildAlloca(builder, LLVMTypeOf(param_value), name);
			LLVMBuildStore(builder, param_value, param_pointer);
			table_put(locals, param_name, param_pointer);
		}
		

		LLVMValueRef exit_code = NULL;
		for (int i = 0; i < node->as.fn.statements.length; i++) {
			exit_code = parse_node(LIST_GET(AstNode *, &node->as.fn.statements, i));
		}
		LLVMBuildRet(builder, exit_code);
		LLVMPositionBuilderAtEnd(builder, block);
	}

	locals = tmp_locals;
	return fn;
}

static LLVMValueRef parse_while(AstNode *node) {
	LLVMBasicBlockRef start_block = LLVMAppendBasicBlockInContext(context, *current_function, "");
	LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(context, *current_function, "");
	LLVMBasicBlockRef end_block = LLVMAppendBasicBlockInContext(context, *current_function, "");

	LLVMBuildBr(builder, start_block);
	LLVMPositionBuilderAtEnd(builder, start_block);
	LLVMValueRef expr = parse_node(node->left);
	LLVMValueRef null = LLVMConstNull(LLVMTypeOf(expr));
	LLVMValueRef cond = LLVMBuildICmp(builder, LLVMIntNE, expr, null, "");
	LLVMBuildCondBr(builder, cond, body_block, end_block);

	LLVMPositionBuilderAtEnd(builder, body_block);
	for (int i = 0; i < node->as.block.statements.length; i++) {
		AstNode *stmnt = LIST_GET(AstNode *, &node->as.block.statements, i);
		parse_node(stmnt);
	}

	LLVMBuildBr(builder, start_block);
	LLVMPositionBuilderAtEnd(builder, end_block);
	return NULL;
}

static LLVMValueRef parse_node(AstNode *node) {
	switch (node->type) {
		case AST_OPERATOR:
			return parse_operator(node);
		case AST_NUMBER:
			return parse_number(node);
		case AST_ASSIGNMENT:
			return parse_assignment(node);
		case AST_VARIABLE:
			return parse_variable(node);
		case AST_STRING:
			return parse_string(node);
		case AST_FUNCTION_CALL:
			return parse_function_call(node);
		case AST_FUNCTION:
			return parse_function(node);
		case AST_WHILE:
			return parse_while(node);
		default:
			report_invalid_node("Unhandled node types");
    }
}

void init_builtins() {
	// Types
	table_put(&types, "s1", LLVMInt8TypeInContext(context));
	table_put(&types, "s2", LLVMInt16TypeInContext(context));
	table_put(&types, "s4", LLVMInt32TypeInContext(context));
	table_put(&types, "s8", LLVMInt64TypeInContext(context));
}

void compile(List *nodes) {
    context = LLVMContextCreate();
    builder = LLVMCreateBuilderInContext(context);
    module = LLVMModuleCreateWithNameInContext("toast", context);

    table_init(&globals);
    table_init(&types);

	init_builtins();

    for (int i = 0; i < nodes->length; i++) {
        parse_node(LIST_GET(AstNode *, nodes, i));
    }

	printf("Writing code...\n");
    LLVMPrintModuleToFile(module, "test.ll", NULL);
	printf("code written to test.ll\n");
    char *code = LLVMPrintModuleToString(module);
    printf("code:\n%s", code);
}

