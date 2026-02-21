#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Object.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Target.h>
#include "codegen.h"
#include "common.h"
#include "list.h"
#include "parser.h"
#include "table.h"
#include "token.h"

typedef struct Scope {
	struct Scope *prev;
	Table *locals;
	Table *definitions;
} Scope;

static LLVMContextRef context;
static LLVMBuilderRef builder;
static LLVMModuleRef module;
static LLVMValueRef *current_function;
Table types;
Table *modules;
Table *imports;
Scope *current_scope;
Scope global_scope;
char *module_dir;
char *module_path;

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

static LLVMValueRef lookup_identifier(String name) {
	char *global_name = resolve_identifier(name, false);

	LLVMValueRef value_pointer = NULL;
	Scope *scope = current_scope;
	while (scope != NULL && value_pointer == NULL) {
		String lookup_name = scope->locals == global_scope.locals ? STRING(global_name) : name;
		value_pointer = (LLVMValueRef)table_get(scope->locals, lookup_name);
		scope = scope->prev;
	}

	if (value_pointer == NULL) {
		// External declarations, could be moved to separate private place
		value_pointer = (LLVMValueRef)table_get(global_scope.locals, name);
	}

	free(global_name);
	return value_pointer;
}

static AstNode *lookup_definition(String name) {
	AstNode *node = (AstNode *)table_get(global_scope.definitions, name);
	return node;
}

static LLVMValueRef handle_rvalue(LLVMValueRef rvalue) {
	LLVMValueKind kind = LLVMGetValueKind(rvalue);
	if (kind == LLVMInstructionValueKind && LLVMGetInstructionOpcode(rvalue) == LLVMAlloca) {
		return LLVMBuildLoad2(
			builder,
			LLVMGetAllocatedType(rvalue),
			rvalue,
			""
		);
	}
	return rvalue;
}

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

	char *cstring = String_to_cstring(node->as.string);
	return LLVMBuildGlobalString(builder, cstring, "");
}

static LLVMValueRef parse_operator(AstNode *node) {
	LLVMValueRef left = handle_rvalue(parse_node(node->as.operator_.left));
	LLVMValueRef right = handle_rvalue(parse_node(node->as.operator_.right));

	switch (node->as.operator_.type) {
		case TOKEN_PLUS:
			return LLVMBuildAdd(builder, left, right, "");
		case TOKEN_MINUS:
			return LLVMBuildSub(builder, left, right, "");
		case TOKEN_STAR:
			return LLVMBuildMul(builder, left, right, "");
		case TOKEN_SLASH:
			return LLVMBuildSDiv(builder, left, right, "");
		case TOKEN_DOUBLE_EQUAL:
			return LLVMBuildICmp(builder, LLVMIntEQ, left, right, "");
		case TOKEN_LESS_THAN:
			return LLVMBuildICmp(builder, LLVMIntSLT, left, right, "");
		case TOKEN_LESS_THAN_OR_EQUAL:
			return LLVMBuildICmp(builder, LLVMIntSLE, left, right, "");
		case TOKEN_GREATER_THAN:
			return LLVMBuildICmp(builder, LLVMIntSGT, left, right, "");
		case TOKEN_GREATER_THAN_OR_EQUAL:
			return LLVMBuildICmp(builder, LLVMIntSGE, left, right, "");
		default:
			report_invalid_node("Unexpected identifier in operator node");
	}
	return NULL; // Will never hit, but just to appease the warning gods
}

static LLVMValueRef parse_variable(AstNode *node) {
	return lookup_identifier(node->as.string);
}

static LLVMValueRef parse_assignment(AstNode *node) {
	LLVMValueRef pointer = lookup_identifier(node->as.assignment.name);
	LLVMValueRef value_node = handle_rvalue(parse_node(node->as.assignment.value));

	if (pointer == NULL) {
		char *name = resolve_identifier(node->as.assignment.name, current_scope->locals != global_scope.locals);
		pointer = LLVMBuildAlloca(builder, LLVMTypeOf(value_node), name);
		table_put(current_scope->locals, STRING(name), pointer);
	}
	LLVMBuildStore(builder, value_node, pointer);

	return value_node;
}

static LLVMValueRef parse_function_call(AstNode *node) {
	LLVMValueRef fn = parse_node(node->as.call.variable);
	LLVMTypeRef fn_type = LLVMGlobalGetValueType(fn);

	size_t name_length;
	const char *name = LLVMGetValueName2(fn, &name_length);
	String Name = { .p = (char *)name, .length = name_length };
	AstNode *fn_node = lookup_definition(Name);
	List parameters = fn_node->as.fn.parameters;

	bool rest = false;
	if (parameters.length > 0) {
		Parameter rest_parameter = LIST_GET(Parameter, &parameters, parameters.length - 1);
		rest = rest_parameter.rest;
	}

	int n_arguments = fn_node->as.fn.vararg ? node->as.call.arguments.length : parameters.length;

	LLVMValueRef args[n_arguments];
	for (int i = 0; i < n_arguments; i++) {
		if (rest && i == n_arguments - 1) {
			int rest_length = node->as.call.arguments.length - i;
			AstNode *item_node = LIST_GET(AstNode *, &node->as.call.arguments, i);
			LLVMValueRef item = handle_rvalue(parse_node(item_node));
			LLVMTypeRef item_type = LLVMTypeOf(item);
			LLVMTypeRef array_type = LLVMArrayType2(item_type, rest_length);
			LLVMValueRef alloca = LLVMBuildAlloca(builder, array_type, "");

			LLVMValueRef indices[2];
			indices[0] = LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0);
			indices[1] = indices[0];
			LLVMValueRef item_ptr = LLVMBuildGEP2(builder, array_type, alloca, indices, 2, "");
			LLVMBuildStore(builder, item, item_ptr);

			for (int j = i + 1; j < node->as.call.arguments.length; j++) {
				item_node = LIST_GET(AstNode *, &node->as.call.arguments, j);
				item = handle_rvalue(parse_node(item_node));

				indices[1] = LLVMConstInt(LLVMInt32TypeInContext(context), 0, j);
				LLVMValueRef item_ptr = LLVMBuildGEP2(builder, array_type, alloca, indices, 2, "");
				LLVMBuildStore(builder, item, item_ptr);
			}

			args[i] = alloca;
		} else {
			args[i] = handle_rvalue(parse_node(LIST_GET(AstNode *, &node->as.call.arguments, i)));
		}
	}

	return LLVMBuildCall2(builder, fn_type, fn, args, n_arguments, "");
}

static LLVMValueRef parse_function_definition(char *name, AstNode *node) {
	LLVMTypeRef return_type = NULL;
	if (node->as.fn.type == NULL) {
		return_type = LLVMVoidTypeInContext(context);
	} else {
		return_type = table_get(&types, node->as.fn.type->name);
		if (node->as.fn.type->pointer) {
			return_type = LLVMPointerType(return_type, 0);
		}
	}

	LLVMTypeRef *parameters = NULL;
	if (node->as.fn.parameters.length != 0) {
		parameters = malloc(sizeof(LLVMTypeRef) * node->as.fn.parameters.length);
		for (int i = 0; i < node->as.fn.parameters.length; i++) {
			Parameter parameter = LIST_GET(Parameter, &node->as.fn.parameters, i);
			parameters[i] = table_get(&types, parameter.type);
			if (parameter.pointer) {
				parameters[i] = LLVMPointerType(parameters[i], 0);
			}
			if (parameter.rest) {
				parameters[i] = LLVMPointerType(parameters[i], 0);
			}
		}
	}

	LLVMTypeRef fn_type = LLVMFunctionType(
		return_type,
		parameters,
		node->as.fn.parameters.length,
		node->as.fn.vararg
	);
    LLVMValueRef fn = LLVMAddFunction(module, name, fn_type);
	table_put(global_scope.locals, STRING(name), fn);
	table_put(global_scope.definitions, STRING(name), node);
	return fn;
}

static LLVMValueRef parse_function(AstNode *node) {
	char *name = resolve_identifier(node->as.fn.name, node->as.fn.external);

	Table locals;
    table_init(&locals);
	Scope function_scope = { .prev = current_scope, .locals = &locals };
	current_scope = &function_scope;

	LLVMValueRef fn = parse_function_definition(name, node);
	current_function = &fn;
	if (node->as.fn.statements.elements != NULL) {
		LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(context, fn, "");
		LLVMPositionBuilderAtEnd(builder, block);


		for (int i = 0; i < node->as.fn.parameters.length; i++) {
			Parameter parameter = LIST_GET(Parameter, &node->as.fn.parameters, i);
			LLVMValueRef param_value = LLVMGetParam(fn, i);
			char *param_name = String_to_cstring(parameter.name);
			LLVMSetValueName2(param_value, param_name, parameter.name.length);
			table_put(&locals, parameter.name, param_value);
		}
		

		for (int i = 0; i < node->as.fn.statements.length; i++) {
			parse_node(LIST_GET(AstNode *, &node->as.fn.statements, i));
		}
		LLVMPositionBuilderAtEnd(builder, block);
	}

	if (node->as.fn.type == NULL &&
		(node->as.fn.statements.elements == NULL ||
		LIST_GET(AstNode *, &node->as.fn.statements, node->as.fn.statements.length - 1)->type != AST_RETURN)) {
		LLVMBuildRetVoid(builder);
	}

	current_function = NULL;
	current_scope = function_scope.prev;
	return fn;
}

static LLVMValueRef parse_while(AstNode *node) {
	LLVMBasicBlockRef start_block = LLVMAppendBasicBlockInContext(context, *current_function, "while.start");
	LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(context, *current_function, "while.body");
	LLVMBasicBlockRef end_block = LLVMAppendBasicBlockInContext(context, *current_function, "while.end");

	LLVMBuildBr(builder, start_block);
	LLVMPositionBuilderAtEnd(builder, start_block);
	LLVMValueRef expr = handle_rvalue(parse_node(node->as.while_.condition));
	LLVMValueRef null = LLVMConstNull(LLVMTypeOf(expr));
	LLVMValueRef cond = LLVMBuildICmp(builder, LLVMIntNE, expr, null, "");
	LLVMBuildCondBr(builder, cond, body_block, end_block);

	LLVMPositionBuilderAtEnd(builder, body_block);
	parse_node(node->as.while_.statement);
	LLVMBuildBr(builder, start_block);
	
	LLVMPositionBuilderAtEnd(builder, end_block);
	return NULL;
}

static LLVMValueRef parse_if(AstNode *node) {
	LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(context, *current_function, "if.then");
	LLVMBasicBlockRef else_block;
	if (node->as.if_.else_statement != NULL) {
		else_block = LLVMAppendBasicBlockInContext(context, *current_function, "if.else");
	} else {
		else_block = NULL;
	}
	LLVMBasicBlockRef end_block = LLVMAppendBasicBlockInContext(context, *current_function, "if.end");
	
	LLVMValueRef expr = handle_rvalue(parse_node(node->as.if_.condition));
	LLVMValueRef null = LLVMConstNull(LLVMTypeOf(expr));
	LLVMValueRef cond = LLVMBuildICmp(builder, LLVMIntNE, expr, null, "");
	LLVMBuildCondBr(builder, cond, then_block, else_block == NULL ? end_block : else_block);

	LLVMPositionBuilderAtEnd(builder, then_block);
	LLVMValueRef statement_value = parse_node(node->as.if_.statement);
	if (statement_value == NULL || LLVMGetInstructionOpcode(statement_value) > LLVMUnreachable) {
		LLVMBuildBr(builder, end_block);
	}

	if (else_block != NULL) {
		LLVMPositionBuilderAtEnd(builder, else_block);
		LLVMValueRef then_value = parse_node(node->as.if_.else_statement);
		if (then_value == NULL || LLVMGetInstructionOpcode(then_value) > LLVMUnreachable) {
			LLVMBuildBr(builder, end_block);
		}
	}
	
	LLVMPositionBuilderAtEnd(builder, end_block);
	return NULL;
}

static LLVMValueRef parse_return(AstNode *node) {
	LLVMValueRef expr = handle_rvalue(parse_node(node->as.return_.expression));
	return LLVMBuildRet(builder, expr);
}

static LLVMValueRef parse_block(AstNode *node) {
	Table locals;
    table_init(&locals);
	Scope block_scope = { .prev = current_scope, .locals = &locals };
	current_scope = &block_scope;
	LLVMValueRef value;
	for (int i = 0; i < node->as.block.statements.length; i++) {
		AstNode *stmnt = LIST_GET(AstNode *, &node->as.block.statements, i);
		value = parse_node(stmnt);
	}
	current_scope = block_scope.prev;
	return value;
}

static LLVMValueRef parse_import(AstNode *node) {
	char *import_path = resolve_module_path(module_dir, node->as.import.path);
	char *identifier = path_to_name(import_path);
	AstNode *file_node = table_get(modules, STRING(import_path));
	table_put(imports, STRING(identifier), &file_node->as.file);
	List *import_file_nodes = &file_node->as.file.nodes;
	for (int i = 0; i < import_file_nodes->length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, import_file_nodes, i);
		if (i_node->type == AST_FUNCTION && !i_node->as.fn.external) {
			char *i_name = resolve_identifier_name(import_path, i_node->as.fn.name);
			parse_function_definition(i_name, i_node);
		}
	}
	return NULL;
}

static LLVMValueRef parse_accessor(AstNode *node) {
	File *file = table_get(imports, node->as.accessor.left->as.string);

	char *current_module_path = module_path;

	module_path = file->path;
	LLVMValueRef result = parse_node(node->as.accessor.right);

	module_path = current_module_path;
	return result;
}

static LLVMValueRef parse_array(AstNode *node) {
	List items = node->as.array.items;
	LLVMValueRef values[items.length];
	for (int i = 0; i < items.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &items, i);
		values[i] = parse_node(i_node);
	}
	
	LLVMValueRef value = LLVMConstArray2(LLVMTypeOf(values[0]), values, items.length);
	return value;
}

// TODO: tempory works, but a lot wrong here
static LLVMValueRef parse_item_access(AstNode *node) {
	LLVMValueRef indexable = parse_node(node->as.item_access.indexable);
	LLVMValueRef index = handle_rvalue(parse_node(node->as.item_access.index));

	LLVMValueRef item_pointer;
	LLVMTypeRef item_type;
	if (LLVMGetValueKind(indexable) == LLVMInstructionValueKind &&
		LLVMGetInstructionOpcode(indexable) == LLVMAlloca) {
		LLVMTypeRef allocated_type = LLVMGetAllocatedType(indexable);
		if (LLVMGetTypeKind(allocated_type) == LLVMArrayTypeKind) {
			item_type = LLVMGetElementType(allocated_type);
			LLVMValueRef indices[2];
			indices[0] = LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0);
			indices[1] = index;
			item_pointer = LLVMBuildGEP2(builder, allocated_type, indexable, indices, 2, "");
		} else {
			item_type = allocated_type;
			LLVMValueRef indices[1] = { index };
			item_pointer = LLVMBuildGEP2(builder, allocated_type, indexable, indices, 1, "");
		}
	} else {
		LLVMTypeRef indexable_type = LLVMTypeOf(indexable);
		// TODO: find item type
		item_type = indexable_type;
		LLVMValueRef indices[1] = { index };
		item_pointer = LLVMBuildGEP2(builder, indexable_type, indexable, indices, 1, "");
	}
	return LLVMBuildLoad2(builder, item_type, item_pointer, "");
}

static LLVMValueRef parse_file_node(AstNode *node) {
	List *nodes = &node->as.file.nodes;
    for (int i = 0; i < nodes->length; i++) {
        parse_node(LIST_GET(AstNode *, nodes, i));
    }

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
		case AST_FILE:
			return parse_file_node(node);
		case AST_FUNCTION_CALL:
			return parse_function_call(node);
		case AST_FUNCTION:
			return parse_function(node);
		case AST_WHILE:
			return parse_while(node);
		case AST_IF:
			return parse_if(node);
		case AST_RETURN:
			return parse_return(node);
		case AST_BLOCK:
			return parse_block(node);
		case AST_IMPORT:
			return parse_import(node);
		case AST_ACCESSOR:
			return parse_accessor(node);
		case AST_ARRAY:
			return parse_array(node);
		case AST_ITEM_ACCESS:
			return parse_item_access(node);
		default:
			report_invalid_node("Unhandled node types");
    }
	return NULL; // Will never hit, but just to appease the warning gods
}

void compiler_initialize(Table *modules_) {
    context = LLVMContextCreate();
	modules = modules_;

	global_scope.prev = NULL;
	global_scope.locals = malloc(sizeof(Table));
	global_scope.definitions = malloc(sizeof(Table));
    table_init(global_scope.locals);
    table_init(global_scope.definitions);

    table_init(&types);
	table_put(&types, STRING("s1"), LLVMInt8TypeInContext(context));
	table_put(&types, STRING("s2"), LLVMInt16TypeInContext(context));
	table_put(&types, STRING("s4"), LLVMInt32TypeInContext(context));
	table_put(&types, STRING("s8"), LLVMInt64TypeInContext(context));
}

LLVMModuleRef build_module(AstNode *file_node, char *dir, char *name, bool entry) {
    builder = LLVMCreateBuilderInContext(context);
    module = LLVMModuleCreateWithNameInContext(name, context);
	module_dir = dir;
	module_path = file_node->as.file.path;

	current_scope = &global_scope;

	Table imports_;
	imports = &imports_;
    table_init(imports);

	parse_node(file_node);

	LLVMDisposeBuilder(builder);
	return module;
}

void compile(LLVMModuleRef module, char *name) {
#ifdef DEBUG
	char *code = LLVMPrintModuleToString(module);
	printf("code:\n\n%s\n", code);
#endif

	LLVMInitializeAllTargetInfos();
	LLVMInitializeAllTargets();
	LLVMInitializeAllTargetMCs();
	LLVMInitializeAllAsmParsers();
	LLVMInitializeAllAsmPrinters();

	char object_file_path[256];
	sprintf(object_file_path, "/tmp/%s.o", name);

	char *err;
	LLVMBool failed;
	LLVMTargetRef target_ref;

	const char *target_triple = LLVMGetDefaultTargetTriple();
	failed = LLVMGetTargetFromTriple(target_triple, &target_ref, &err);
	if (failed) {
		printf("LLVM: %s\n", err);
		exit(1);
	}

	LLVMTargetMachineOptionsRef target_machine_options_ref = LLVMCreateTargetMachineOptions();
	LLVMTargetMachineOptionsSetRelocMode(target_machine_options_ref, LLVMRelocPIC);
	LLVMTargetMachineRef target_machine_ref = LLVMCreateTargetMachineWithOptions(target_ref, target_triple, target_machine_options_ref);

	failed = LLVMTargetMachineEmitToFile(target_machine_ref, module, object_file_path, LLVMObjectFile, &err);
	if (failed) {
		printf("LLVM: %s\n", err);
		exit(1);
	}

	char cmd[512];
	sprintf(cmd, "clang %s -o %s", object_file_path, name);
	int link_result = system(cmd);
	if (link_result) {
		printf("Unable to link using clang, command: %s\n", cmd);
		exit(1);
	}
}

