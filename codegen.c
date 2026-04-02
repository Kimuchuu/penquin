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

static LLVMContextRef context;
static LLVMBuilderRef builder;
static LLVMModuleRef module;
static LLVMValueRef current_function;
static Table types;
static char *module_path;

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

static LLVMValueRef handle_rvalue(LLVMValueRef rvalue) {
	LLVMValueKind kind = LLVMGetValueKind(rvalue);
	if (kind == LLVMInstructionValueKind && LLVMGetInstructionOpcode(rvalue) == LLVMAlloca) {
		LLVMTypeRef allocated_type = LLVMGetAllocatedType(rvalue);
		LLVMTypeKind allocated_kind = LLVMGetTypeKind(allocated_type);
		if (allocated_kind == LLVMArrayTypeKind) {
			return rvalue;
		}

		return LLVMBuildLoad2(
			builder,
			allocated_type,
			rvalue,
			""
		);
	}
	return rvalue;
}

static LLVMTypeRef parse_type(TypeInfo *type_info) {
	switch (type_info->type) {
		case TYPE_VALUE:
			return table_get(&types, type_info->value_of);
		case TYPE_ARRAY:
			return LLVMArrayType2(parse_type(type_info->array.of), type_info->array.length);
		case TYPE_POINTER:
			return LLVMPointerType(parse_type(type_info->pointer_to), 0);
		default:
			return NULL;
	}
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

static int score_value_type(TypeInfo *type) {
	if (String_cmp_cstring(type->value_of, "bool")) {
		return 1;
	} else if (String_cmp_cstring(type->value_of, "s1")) {
		return 2;
	} else if (String_cmp_cstring(type->value_of, "s2")) {
		return 3;
	} else if (String_cmp_cstring(type->value_of, "s4")) {
		return 4;
	} else if (String_cmp_cstring(type->value_of, "s8")) {
		return 5;
	} else {
		char *type_name = String_to_cstring(type->value_of);
		fprintf(stderr, "Cannot score type: %s\n", type_name);
		exit(1);
	}
}

static TypeInfo *deduce_type(AstNode *left, AstNode *right) {
	assert(left->type_info->type == TYPE_VALUE &&
		   left->type_info->type == right->type_info->type);
	if (score_value_type(left->type_info) > score_value_type(right->type_info)) {
		return left->type_info;
	}
	return right->type_info;
}

static LLVMValueRef parse_operator(AstNode *node) {
	LLVMValueRef left = handle_rvalue(parse_node(node->as.operator_.left));
	LLVMValueRef right = handle_rvalue(parse_node(node->as.operator_.right));

	if (node->as.operator_.type == TOKEN_PLUS ||
		node->as.operator_.type == TOKEN_MINUS ||
		node->as.operator_.type == TOKEN_STAR ||
		node->as.operator_.type == TOKEN_SLASH ||
		node->as.operator_.type == TOKEN_DOUBLE_EQUAL ||
		node->as.operator_.type == TOKEN_LESS_THAN ||
		node->as.operator_.type == TOKEN_LESS_THAN_OR_EQUAL ||
		node->as.operator_.type == TOKEN_GREATER_THAN ||
		node->as.operator_.type == TOKEN_GREATER_THAN_OR_EQUAL ||
		node->as.operator_.type == TOKEN_NOT_EQUAL) {
		TypeInfo *type = deduce_type(node->as.operator_.left, node->as.operator_.right);
		if (String_cmp(node->as.operator_.left->type_info->value_of, type->value_of) != 0) {
			left = LLVMBuildCast(builder, LLVMSExt, left, parse_type(type), "");
		} else if (String_cmp(node->as.operator_.right->type_info->value_of, type->value_of) != 0) {
			right = LLVMBuildCast(builder, LLVMSExt, right, parse_type(type), "");
		}
	}

	switch (node->as.operator_.type) {
		// Aritmethic
		case TOKEN_PLUS:
			return LLVMBuildAdd(builder, left, right, "");
		case TOKEN_MINUS:
			return LLVMBuildSub(builder, left, right, "");
		case TOKEN_STAR:
			return LLVMBuildMul(builder, left, right, "");
		case TOKEN_SLASH:
			return LLVMBuildSDiv(builder, left, right, "");
		// Comparison
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
		case TOKEN_NOT_EQUAL:
			return LLVMBuildICmp(builder, LLVMIntNE, left, right, "");
		// Logical
		case TOKEN_LOGICAL_AND:
			left = LLVMBuildIsNotNull(builder, left, "");
			right = LLVMBuildIsNotNull(builder, right, "");
			return LLVMBuildSelect(builder, left, right, left, "");
		case TOKEN_LOGICAL_OR:
			left = LLVMBuildIsNotNull(builder, left, "");
			right = LLVMBuildIsNotNull(builder, right, "");
			return LLVMBuildSelect(builder, left, left, right, "");
		default:
			report_invalid_node("Unexpected identifier in operator node");
	}
	return NULL; // Will never hit, but just to appease the warning gods
}

static LLVMValueRef parse_variable(AstNode *node) {
	return node->as.variable.declaration->backend_ref;
}

static LLVMValueRef parse_assignment(AstNode *node) {
	if (node == node->as.assignment.initial) {
		char *name = String_to_cstring(node->as.assignment.name);
		node->backend_ref = LLVMBuildAlloca(builder, parse_type(node->type_info), name);
	}

	if (node->as.assignment.value == NULL) {
		return node->as.assignment.initial->backend_ref;
	}

	LLVMValueRef value_node = handle_rvalue(parse_node(node->as.assignment.value));
	LLVMBuildStore(builder, value_node, node->as.assignment.initial->backend_ref);
	return NULL;
}

static LLVMValueRef parse_function_call(AstNode *node) {
	AstNode *fn_node = node->as.call.function;
	LLVMValueRef fn = fn_node->backend_ref;
	LLVMTypeRef fn_type = LLVMGlobalGetValueType(fn);
	List parameters = fn_node->as.fn.parameters;

	bool rest = false;
	if (parameters.length > 0) {
		AstNode *rest_parameter = LIST_GET(AstNode *, &parameters, parameters.length - 1);
		rest = rest_parameter->as.parameter.rest;
	}

	int n_arguments = fn_node->as.fn.vararg ? node->as.call.arguments.length : parameters.length;

	LLVMValueRef args[n_arguments];
	for (int i = 0; i < n_arguments; i++) {
		if (rest && i == n_arguments - 1) {
			int rest_length = node->as.call.arguments.length - i;
			AstNode *item_node = LIST_GET(AstNode *, &node->as.call.arguments, i);
			LLVMValueRef item = handle_rvalue(parse_node(item_node));
			LLVMTypeRef item_type = parse_type(item_node->type_info);
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
		return_type = parse_type(node->type_info);
	}

	LLVMTypeRef *parameters = NULL;
	if (node->as.fn.parameters.length != 0) {
		parameters = malloc(sizeof(LLVMTypeRef) * node->as.fn.parameters.length);
		for (int i = 0; i < node->as.fn.parameters.length; i++) {


			AstNode *parameter_node = LIST_GET(AstNode *, &node->as.fn.parameters, i);
			parameters[i] = parse_type(&parameter_node->as.parameter.type_info);
			if (parameter_node->as.parameter.rest) {
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
	node->backend_ref = fn;
	return fn;
}

static LLVMValueRef parse_function(AstNode *node) {
	char *name = resolve_identifier(node->as.fn.name, node->as.fn.external);

	Table locals;
    table_init(&locals);

	LLVMValueRef fn = parse_function_definition(name, node);
	current_function = fn;
	if (node->as.fn.statements.elements != NULL) {
		LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(context, fn, "");
		LLVMPositionBuilderAtEnd(builder, block);

		for (int i = 0; i < node->as.fn.parameters.length; i++) {
			AstNode *parameter_node = LIST_GET(AstNode *, &node->as.fn.parameters, i);
			LLVMValueRef param_value = LLVMGetParam(fn, i);
			char *param_name = String_to_cstring(parameter_node->as.parameter.name);
			LLVMSetValueName2(param_value, param_name, parameter_node->as.parameter.name.length);
			parameter_node->backend_ref = param_value;
		}
		

		for (int i = 0; i < node->as.fn.statements.length; i++) {
			parse_node(LIST_GET(AstNode *, &node->as.fn.statements, i));
		}
	}

	if (node->as.fn.type == NULL &&
		(node->as.fn.statements.elements == NULL ||
		LIST_GET(AstNode *, &node->as.fn.statements, node->as.fn.statements.length - 1)->type != AST_RETURN)) {
		LLVMBuildRetVoid(builder);
	}

	current_function = NULL;
	return fn;
}

static LLVMValueRef parse_while(AstNode *node) {
	LLVMBasicBlockRef start_block = LLVMAppendBasicBlockInContext(context, current_function, "while.start");
	LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(context, current_function, "while.body");
	LLVMBasicBlockRef end_block = LLVMAppendBasicBlockInContext(context, current_function, "while.end");

	LLVMBuildBr(builder, start_block);
	LLVMPositionBuilderAtEnd(builder, start_block);
	LLVMValueRef expr = handle_rvalue(parse_node(node->as.while_.condition));
	LLVMValueRef null = LLVMConstNull(parse_type(node->as.while_.condition->type_info));
	LLVMValueRef cond = LLVMBuildICmp(builder, LLVMIntNE, expr, null, "");
	LLVMBuildCondBr(builder, cond, body_block, end_block);

	LLVMPositionBuilderAtEnd(builder, body_block);
	parse_node(node->as.while_.statement);
	LLVMBuildBr(builder, start_block);
	
	LLVMPositionBuilderAtEnd(builder, end_block);
	return NULL;
}

static LLVMValueRef parse_if(AstNode *node) {
	LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(context, current_function, "if.then");
	LLVMBasicBlockRef else_block;
	if (node->as.if_.else_statement != NULL) {
		else_block = LLVMAppendBasicBlockInContext(context, current_function, "if.else");
	} else {
		else_block = NULL;
	}
	LLVMBasicBlockRef end_block = LLVMAppendBasicBlockInContext(context, current_function, "if.end");
	
	LLVMValueRef expr = handle_rvalue(parse_node(node->as.if_.condition));
	LLVMValueRef null = LLVMConstNull(parse_type(node->as.if_.condition->type_info));
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
	LLVMValueRef value;
	for (int i = 0; i < node->as.block.statements.length; i++) {
		AstNode *stmnt = LIST_GET(AstNode *, &node->as.block.statements, i);
		value = parse_node(stmnt);
	}
	return value;
}

static LLVMValueRef parse_bool(AstNode *node) {
    LLVMTypeRef bool_type = LLVMInt1TypeInContext(context);
    return LLVMConstInt(bool_type, node->as.bool_, 1);
}

static LLVMValueRef parse_import(AstNode *node) {
	AstNode *file_node = node->as.import.file_node;
	List *import_file_nodes = &file_node->as.file.nodes;
	for (int i = 0; i < import_file_nodes->length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, import_file_nodes, i);
		if (i_node->type == AST_FUNCTION && !i_node->as.fn.external) {
			char *i_name = resolve_identifier_name(file_node->as.file.path, i_node->as.fn.name);
			parse_function_definition(i_name, i_node);
		}
	}
	return NULL;
}

static LLVMValueRef parse_accessor(AstNode *node) {
	File *file = &node->as.accessor.left->as.variable.declaration->as.file;

	char *current_module_path = module_path;

	module_path = file->path;
	LLVMValueRef result = parse_node(node->as.accessor.right);

	module_path = current_module_path;
	return result;
}

static LLVMValueRef parse_array(AstNode *node) {
	List items = node->as.array.items;
	LLVMValueRef values[items.length];
	LLVMTypeRef item_type = parse_type(node->type_info->array.of);
	for (int i = 0; i < items.length; i++) {
		AstNode *i_node = LIST_GET(AstNode *, &items, i);
		values[i] = parse_node(i_node);
	}
	
	LLVMValueRef value = LLVMConstArray2(item_type, values, items.length);
	return value;
}

static LLVMValueRef parse_item_access(AstNode *node) {
	LLVMValueRef indexable = parse_node(node->as.item_access.indexable);
	LLVMValueRef index = handle_rvalue(parse_node(node->as.item_access.index));

	LLVMValueRef item_pointer;
	LLVMTypeRef item_type;
	TypeInfo *type_info = node->as.item_access.indexable->type_info;
	LLVMTypeRef allocated_type = parse_type(type_info);
	if (type_info->type == TYPE_ARRAY) {
		item_type = LLVMGetElementType(allocated_type);

		LLVMValueRef indices[2];
		indices[0] = LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0);
		indices[1] = index;
		item_pointer = LLVMBuildGEP2(builder, allocated_type, indexable, indices, 2, "");
	} else {
		item_type = parse_type(type_info->pointer_to);

		LLVMValueRef indices[1] = { index };
		item_pointer = LLVMBuildGEP2(builder, item_type, indexable, indices, 1, "");
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
		case AST_BOOL:
			return parse_bool(node);
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

    table_init(&types);
	table_put(&types, STRING("bool"), LLVMInt1TypeInContext(context));
	table_put(&types, STRING("s1"), LLVMInt8TypeInContext(context));
	table_put(&types, STRING("s2"), LLVMInt16TypeInContext(context));
	table_put(&types, STRING("s4"), LLVMInt32TypeInContext(context));
	table_put(&types, STRING("s8"), LLVMInt64TypeInContext(context));

	LLVMTypeRef	string_type = LLVMStructCreateNamed(context, "string");
	LLVMTypeRef string_elements[2];
	string_elements[0] = table_get(&types, STRING("s4"));
	string_elements[1] = LLVMPointerType(table_get(&types, STRING("s1")), 0);
	LLVMStructSetBody(string_type, string_elements, 2, false);
	table_put(&types, STRING("string"), string_type);
}

LLVMModuleRef build_module(AstNode *file_node, char *dir, char *name, bool entry) {
    builder = LLVMCreateBuilderInContext(context);
    module = LLVMModuleCreateWithNameInContext(name, context);
	module_path = file_node->as.file.path;

	parse_node(file_node);

	module_path = NULL;
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

