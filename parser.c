#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "parser.h"
#include "common.h"
#include "list.h"
#include "token.h"

List    *tokens;
Token   *current_token;
AstNode *current_node;

static AstNode *parse_expression();
static AstNode *parse_statement();

static void print_type_info(TypeInfo *type_info) {
	switch (type_info->type) {
		case TYPE_VALUE: {
			DEFINE_CSTRING(str, type_info->value_of);
			printf("%s", str);
			break;
		}
		case TYPE_ARRAY:
			printf("[");
			print_type_info(type_info->array.of);
			printf("]");
			break;
		case TYPE_POINTER:
			printf("*");
			print_type_info(type_info->pointer_to);
			break;
	}
}

static void print_tree(AstNode *node, int level) {
    if (node == NULL) {
        return;
    }
    switch (node->type) {
        case AST_ACCESSOR:
            print_tree(node->as.accessor.left, level);
            printf("::");
            print_tree(node->as.accessor.right, level);
            break;
        case AST_ARRAY:
            printf("[");
			List *items = &node->as.array.items;
			AstNode *item;
			for (int i = 0; i < items->length; i++) {
				item = LIST_GET(AstNode *, items, i);
				print_tree(item, level);
			}
            printf("]");
            break;
	    case AST_ASSIGNMENT: {
			DEFINE_CSTRING(name, node->as.assignment.name)
            printf("(%s = ", name);
            print_tree(node->as.assignment.value, level);
            printf(")");
            break;
		}
        case AST_BLOCK:
            printf("(");
			List *statements = &node->as.block.statements;
			AstNode *statement;
			for (int i = 0; i < statements->length; i++) {
				printf("\n%*c", level + 2, ' ');
				statement = LIST_GET(AstNode *, statements, i);
				print_tree(statement, level + 2);
			}
			printf(")");
            break;
        case AST_BOOL:
            printf("%b", node->as.bool_);
            break;
        case AST_FILE:
            printf("(%s\n", node->as.file.path);
			List *nodes = &node->as.file.nodes;
			for (int i = 0; i < nodes->length; i++) {
				print_tree(LIST_GET(AstNode *, nodes, i), level);
			}
            printf(")");
            break;
        case AST_FUNCTION: {
			DEFINE_CSTRING(name, node->as.fn.name)
            printf("(fn:%s", name);
			List *parameters = &node->as.fn.parameters;
			for (int i = 0; i < parameters->length; i++) {
				AstNode *parameter_node = LIST_GET(AstNode *, parameters, i);
				printf(" ");
				print_tree(parameter_node, level);
			}
			if (node->as.fn.statements.elements != NULL) {
				List *statements = &node->as.fn.statements;
				for (int i = 0; i < statements->length; i++) {
					printf("\n%*c", level + 2, ' ');
					AstNode *statement_node = LIST_GET(AstNode *, statements, i);
					print_tree(statement_node, level + 2);
				}
			}
            printf(")");
            break;
		}
        case AST_FUNCTION_CALL:
            printf("(");
            print_tree(node->as.call.variable, level);
			List *arguments = &node->as.call.arguments;
			for (int i = 0; i < arguments->length; i++) {
				printf(" ");
				print_tree(LIST_GET(AstNode *, arguments, i), level);
			}
            printf(")");
            break;
        case AST_IF:
            printf("(if ");
            print_tree(node->as.if_.condition, level);
			printf("then ");
            print_tree(node->as.if_.statement, level);
			if (node->as.if_.else_statement != NULL) {
				printf("else ");
				print_tree(node->as.if_.else_statement, level);
			}
            printf(")");
            break;
		case AST_ITEM_ACCESS:
			print_tree(node->as.item_access.indexable, level);
			printf("[");
			print_tree(node->as.item_access.index, level);
			printf("]");
			break;
	    case AST_IMPORT: {
			DEFINE_CSTRING(path, node->as.import.path)
            printf("(import %s)", path);
            break;
		}
        case AST_MATCH: {
            printf("(match ");
			print_tree(node->as.match.matcher, level);
			List *branches = &node->as.match.branches;
			for (int i = 0; i < branches->length; i++) {
				MatchBranch branch = LIST_GET(MatchBranch, &node->as.match.branches, i);
				printf("\n%*c(", level + 2, ' ');
				print_type_info(branch.type_info);
				printf(" ");
				print_tree(branch.identifier, level);
				printf(" ");
				print_tree(branch.expression, level);
				printf(")");
			}
			printf(")");
            break;
		}
        case AST_NUMBER:
            printf("%f", node->as.number);
            break;
        case AST_OPERATOR:
            printf("(");
            print_tree(node->as.operator_.left, level);
            printf(" %s ", token_type_to_string(node->as.operator_.type));
            print_tree(node->as.operator_.right, level);
            printf(")");
            break;
        case AST_PARAMETER:
			print_type_info(&node->as.parameter.type_info);
            break;
	    case AST_RETURN: {
            printf("(return ");
			print_tree(node->as.return_.expression, level);
            printf(")");
            break;
		}
        case AST_STRING: {
			DEFINE_CSTRING(str, node->as.string)
			// Disable wrapping
			for (int i = 0; i < node->as.string.length; i++) {
				if (str[i] == '\n') {
					str[i] = '\\';
				}
			}
            printf("%s", str);
            break;
        }
        case AST_VARIABLE: {
			DEFINE_CSTRING(str, node->as.variable.name)
            printf("%s", str);
            break;
        }
        case AST_WHILE:
            printf("(while ");
            print_tree(node->as.while_.condition, level);
            printf(" ");
            print_tree(node->as.while_.statement, level);
            printf(")");
            break;
    }
}

static void consume(TokenType type) {
    if (current_token->type != type) {
        fprintf(stderr, "Epic fail, expected token: %s.\n", token_type_to_string(type));
        exit(1);
    }
    current_token++;
}

static char consume_if(TokenType type) {
    if (current_token->type != type) {
        return 0;
    }
    current_token++;
	return 1;
}

static inline AstNode *create_node(AstType type) {
    AstNode *node = malloc(sizeof(AstNode));
    node->type = type;
	node->type_info = NULL;
	node->backend_ref = NULL;
    return node;
}

static AstNode *create_number() {
    float n = strtof(current_token->raw, NULL);
    AstNode *node = create_node(AST_NUMBER);
    node->as.number = n;
    return node;
}

static AstNode *create_bool() {
    AstNode *node = create_node(AST_BOOL);
    node->as.bool_ = current_token->type == TOKEN_TRUE;
    return node;
}

static AstNode *create_operator() {
    AstNode *node = create_node(AST_OPERATOR);
    node->as.operator_.type = current_token->type;
    return node;
}

static String escape_string(String original) {
	char *str = malloc(original.length + 1);
	char last = '\0';
	int pos = 0;

	for (int i = 0; i < original.length; i++) {
		char current = original.p[i];
		if (last == '\\' && current == 'n') {
			str[pos - 1] = '\n';
		} else {
			str[pos++] = current;
		}
		last = original.p[i];
	}
	String res = { .p = str, .length = pos };
	return res;
}

static AstNode *create_string() {
    AstNode *node = create_node(AST_STRING);
    node->as.string.p = current_token->raw + 1;
    node->as.string.length = current_token->length - 2;
	node->as.string = escape_string(node->as.string);
    return node;
}

static AstNode *create_variable() {
    AstNode *node = create_node(AST_VARIABLE);
    node->as.variable.name.p = current_token->raw;
    node->as.variable.name.length = current_token->length;
    node->as.variable.declaration = NULL;
    return node;
}

static TypeInfo *parse_type() {
	TypeInfo *type_info = malloc(sizeof(TypeInfo));
	if (current_token->type == TOKEN_STAR) {
		current_token++;
		type_info->type = TYPE_POINTER;
		type_info->pointer_to = parse_type();
	} else if (current_token->type == TOKEN_IDENTIFIER) {
		String type_name = { current_token->raw, current_token->length };
		current_token++;
		type_info->type = TYPE_VALUE;
		type_info->value_of = type_name;
		if (current_token->type == TOKEN_LEFT_BRACKET) {
			current_token++;
			// TODO: improve validation
			if (current_token->type != TOKEN_NUMBER) {
				fprintf(stderr, "Epic fail, expected number in array type but got: %s.\n",
						token_type_to_string(current_token->type));
			}
			int n = strtol(current_token->raw, NULL, 10);
			current_token++;

			TypeInfo *array_type_info = malloc(sizeof(TypeInfo));
			array_type_info->type = TYPE_ARRAY;
			array_type_info->array.length = n;
			array_type_info->array.of = type_info;
			type_info = array_type_info;
			consume(TOKEN_RIGHT_BRACKET);
		}
	} else {
		fprintf(stderr, "Epic fail, expected star or identifier but got: %s.\n",
				token_type_to_string(current_token->type));
		exit(1);
	}
	return type_info;
}

static AstNode *parse_primary() {
    if (current_token->type == TOKEN_TRUE || current_token->type == TOKEN_FALSE) {
        AstNode *bool_ = create_bool();
        current_token++;
        return bool_;
	} else if (current_token->type == TOKEN_NUMBER) {
        AstNode *number = create_number();
        current_token++;
        return number;
    } else if (current_token->type == TOKEN_STRING) {
        AstNode *string = create_string();
        current_token++;
        return string;
    } else if (current_token->type == TOKEN_IDENTIFIER) {
        AstNode *variable = create_variable();
        current_token++;
        return variable;
    } else {
        const char *name = token_type_to_string(current_token->type);
        fprintf(stderr, "Epic fail, we can't handle '%s' as a primary.\n", name);
        exit(1);
    }
}

static AstNode *parse_accessor() {
	AstNode *primary = parse_primary();
	if (primary->type == AST_VARIABLE && current_token->type == TOKEN_DOUBLE_COLON) {
		current_token++;
		if (current_token->type != TOKEN_IDENTIFIER) {
			fprintf(stderr, "Epic fail, expected identifier but got: %s.\n",
					token_type_to_string(current_token->type));
			exit(1);
		}
		AstNode *accessor_node = create_node(AST_ACCESSOR);
		accessor_node->as.accessor.left = primary;
		accessor_node->as.accessor.right = create_variable();
		current_token++;
		primary = accessor_node;
	}
	return primary;
}

static AstNode *parse_call() {
    AstNode *accessor = parse_accessor();
	if ((accessor->type == AST_VARIABLE || accessor->type == AST_ACCESSOR) && current_token->type == TOKEN_LEFT_PAREN) {
		current_token++;
		AstNode *call = create_node(AST_FUNCTION_CALL);
		call->as.call.variable = accessor;
		accessor = call;
		list_init(&call->as.call.arguments, sizeof(AstNode *));
		if (current_token->type != TOKEN_RIGHT_PAREN) {
			AstNode *argument = parse_expression();
			list_add(&call->as.call.arguments, &argument);
			while (current_token->type == TOKEN_COMMA) {
				current_token++;
				argument = parse_expression();
				list_add(&call->as.call.arguments, &argument);
			}
		}
		consume(TOKEN_RIGHT_PAREN);
	}
    return accessor;
}

static AstNode *parse_item_access() {
    AstNode *call = parse_call();
	while (current_token->type == TOKEN_LEFT_BRACKET) {
		current_token++;
		AstNode *index = parse_expression();
		consume(TOKEN_RIGHT_BRACKET);
		AstNode *access = create_node(AST_ITEM_ACCESS);
		access->as.item_access.index = index;
		access->as.item_access.indexable = call;
		call = access;
	}
	return call;
}

static AstNode *parse_factor() {
    AstNode *call = parse_item_access();
    while (current_token->type == TOKEN_STAR || current_token->type == TOKEN_SLASH) {
        AstNode *operator = create_operator();
        current_token++;
        operator->as.operator_.left = call;
        operator->as.operator_.right = parse_item_access();
        call = operator;
    }
    return call;
}

static AstNode *parse_term() {
    AstNode *factor = parse_factor();
    while (current_token->type == TOKEN_PLUS || current_token->type == TOKEN_MINUS) {
        AstNode *operator = create_operator();
        current_token++;
        operator->as.operator_.left = factor;
        operator->as.operator_.right = parse_factor();
        factor = operator;
    }
    return factor;
}

static AstNode *parse_comparison() {
    AstNode *term = parse_term();
    if (current_token->type == TOKEN_DOUBLE_EQUAL ||
		current_token->type == TOKEN_GREATER_THAN || 
		current_token->type == TOKEN_GREATER_THAN_OR_EQUAL ||
		current_token->type == TOKEN_LESS_THAN ||
		current_token->type == TOKEN_LESS_THAN_OR_EQUAL ||
		current_token->type == TOKEN_NOT_EQUAL) {
        AstNode *operator = create_operator();
        current_token++;
        operator->as.operator_.left = term;
        operator->as.operator_.right = parse_term();
		term = operator;
	}
    return term;
}

static AstNode *parse_logical_and() {
    AstNode *comparison = parse_comparison();
    while (current_token->type == TOKEN_LOGICAL_AND) {
        AstNode *operator = create_operator();
        current_token++;
        operator->as.operator_.left = comparison;
        operator->as.operator_.right = parse_comparison();
		comparison = operator;
	}
    return comparison;
}

static AstNode *parse_logical_or() {
    AstNode *logical_and = parse_logical_and();
    while (current_token->type == TOKEN_LOGICAL_OR) {
        AstNode *operator = create_operator();
        current_token++;
        operator->as.operator_.left = logical_and;
        operator->as.operator_.right = parse_logical_and();
		logical_and = operator;
	}
    return logical_and;
}

static AstNode *parse_array() {
    if (current_token->type == TOKEN_LEFT_BRACKET) {
		current_token++;
		AstNode *node = create_node(AST_ARRAY);
		list_init(&node->as.array.items, sizeof(AstNode *));
		while (current_token->type != TOKEN_RIGHT_BRACKET && (current_token - (Token *)tokens->elements) < tokens->length) {
			AstNode *expr = parse_logical_or();
			list_add(&node->as.array.items, &expr);
			if (!consume_if(TOKEN_COMMA)) {
				break;
			}
		}
		consume(TOKEN_RIGHT_BRACKET);
		return node;
	}

	return parse_logical_or();
}

static AstNode *parse_match() {
    if (current_token->type == TOKEN_MATCH) {
		current_token++;
		AstNode *node = create_node(AST_MATCH);
		node->as.match.matcher = parse_array();
		consume(TOKEN_LEFT_BRACE);

		list_init(&node->as.match.branches, sizeof(MatchBranch));

		while (current_token->type != TOKEN_RIGHT_BRACE) {
			MatchBranch branch;
			branch.type_info = parse_type();

			assert(current_token->type == TOKEN_IDENTIFIER);
			branch.identifier = create_variable();
			current_token++;

			consume(TOKEN_ARROW);

			branch.expression = parse_expression();

			list_add(&node->as.match.branches, &branch);
		}

		consume(TOKEN_RIGHT_BRACE);
		return node;
	}

	return parse_array();
}

static AstNode *parse_expression() {
	return parse_match();
}

static AstNode *parse_assignment_or_expression_statement() {
    AstNode *dst = parse_expression();

	TypeInfo *type_info = NULL;
	if (current_token->type == TOKEN_COLON) {
		assert(dst->type == AST_VARIABLE);
		current_token++;
		type_info = parse_type();
	}

	bool explicit_assignment = current_token->type == TOKEN_EQUAL;
	if (explicit_assignment || type_info != NULL) {
        assert(dst->type == AST_VARIABLE);
        AstNode *ass = create_node(AST_ASSIGNMENT);
        ass->as.assignment.name = dst->as.variable.name;
		if (explicit_assignment) {
			current_token++;
			ass->as.assignment.value = parse_expression();
		} else {
			ass->as.assignment.value = NULL;
		}
        ass->as.assignment.initial = NULL;
        ass->as.assignment.type_info = type_info;
		free(dst);
        dst = ass;
    }

	consume(TOKEN_SEMICOLON);

    return dst;
}

static AstNode *parse_return_statement() {
	AstNode *return_node = create_node(AST_RETURN);
	current_token++;
	return_node->as.return_.expression = parse_expression();
    consume(TOKEN_SEMICOLON);
	return return_node;
}

static AstNode *parse_block();

static AstNode *parse_if_statement() {
	AstNode *if_node = create_node(AST_IF);
	current_token++;

	if_node->as.if_.condition = parse_expression();
	if_node->as.if_.statement = parse_block();
	if (current_token->type == TOKEN_ELSE) {
		current_token++;
		if_node->as.if_.else_statement = parse_statement();
	} else {
		if_node->as.if_.else_statement = NULL;
	}
	
	return if_node;
}

static AstNode *parse_while_statement() {
	AstNode *while_node = create_node(AST_WHILE);
	current_token++;
	while_node->as.while_.condition = parse_expression();
	while_node->as.while_.statement = parse_block();
	return while_node;
}

static AstNode *parse_statement() {
	switch (current_token->type) {
	case TOKEN_WHILE:
		return parse_while_statement();
	case TOKEN_IF:
		return parse_if_statement();
	case TOKEN_RETURN:
		return parse_return_statement();
	case TOKEN_LEFT_BRACE:
		return parse_block();
	default:
		return parse_assignment_or_expression_statement();
	}
}

static AstNode *parse_block() {
	consume(TOKEN_LEFT_BRACE);
	AstNode *block_node = create_node(AST_BLOCK);
	block_node->as.block.scope = NULL;
	
	list_init(&block_node->as.block.statements, sizeof(AstNode *));
	while (current_token->type != TOKEN_RIGHT_BRACE && (current_token - (Token *)tokens->elements) < tokens->length) {
		AstNode *statement = parse_statement();
		list_add(&block_node->as.block.statements, &statement);
	}

	consume(TOKEN_RIGHT_BRACE);
	return block_node;
}

static AstNode *parse_function(bool external) {
	AstNode *fn_node = create_node(AST_FUNCTION);
	fn_node->as.fn.scope = NULL;
	current_token++;
	if (current_token->type != TOKEN_IDENTIFIER) {
		fprintf(stderr, "Epic fail, expected identifier but got: %s.\n",
				token_type_to_string(current_token->type));
		exit(1);
	}

	fn_node->as.fn.external = external;
	fn_node->as.fn.vararg = false;
    fn_node->as.fn.name.p = current_token->raw;
    fn_node->as.fn.name.length = current_token->length;
	current_token++;

	consume(TOKEN_LEFT_PAREN);

	// Parameters
	if (current_token->type != TOKEN_RIGHT_PAREN) {
		list_init(&fn_node->as.fn.parameters, sizeof(AstNode *));
			
		Parameter parameter;
		do {
			parameter.rest = consume_if(TOKEN_TRIPLE_DOT);
			if (current_token->type != TOKEN_IDENTIFIER && !parameter.rest) {
				fprintf(stderr, "Epic fail, expected identifier (type) but got: %s.\n",
						token_type_to_string(current_token->type));
				exit(1);
			} else if (current_token->type != TOKEN_IDENTIFIER) {
				fn_node->as.fn.vararg = true;
				break;
			}
			parameter.name.p = current_token->raw;
			parameter.name.length = current_token->length;
			current_token++;
			consume(TOKEN_COLON);

			TypeInfo *type_info = parse_type();
			if (parameter.rest) {
				parameter.type_info.type = TYPE_POINTER;
				parameter.type_info.pointer_to = type_info;
			} else {
				parameter.type_info = *type_info;
				free(type_info);
			}
			
			AstNode *parameter_node = create_node(AST_PARAMETER);
			parameter_node->as.parameter = parameter;
			list_add(&fn_node->as.fn.parameters, &parameter_node);
		} while (!parameter.rest && consume_if(TOKEN_COMMA));
    } else {
		fn_node->as.fn.parameters.length = 0;
    }

	consume(TOKEN_RIGHT_PAREN);

	// Type
	if (current_token->type == TOKEN_COLON) {
		current_token++;
		bool pointer = consume_if(TOKEN_STAR);
		if (current_token->type != TOKEN_IDENTIFIER) {
			fprintf(stderr, "Epic fail, expected identifier (type) but got: %s.\n",
					token_type_to_string(current_token->type));
			exit(1);
		}

		Type *type = malloc(sizeof(Type));
		type->name.p = current_token->raw;
		type->name.length = current_token->length;
		type->pointer = pointer;
		fn_node->as.fn.type = type;
		current_token++;
	} else {
		fn_node->as.fn.type = NULL;
	}

	if (!external) {
		consume(TOKEN_LEFT_BRACE);
		list_init(&fn_node->as.fn.statements, sizeof(AstNode *));
		while (current_token->type != TOKEN_RIGHT_BRACE && (current_token - (Token *)tokens->elements) < tokens->length) {
			AstNode *statement = parse_statement();
			list_add(&fn_node->as.fn.statements, &statement);
		}
		consume(TOKEN_RIGHT_BRACE);
	} else {
		fn_node->as.fn.statements.elements = NULL;
		fn_node->as.fn.statements.length = 0;
		consume(TOKEN_SEMICOLON);
	}

	return fn_node;
}

static AstNode *parse_import() {
	AstNode *import_node = create_node(AST_IMPORT);
	current_token++;
	
	if (current_token->type != TOKEN_STRING) {
		fprintf(stderr, "Epic fail, expected import path but got: %s.\n",
				token_type_to_string(current_token->type));
		exit(1);
	}
	
    import_node->as.import.path.p = current_token->raw + 1;
    import_node->as.import.path.length = current_token->length - 2;
    import_node->as.import.file_node = NULL;
	current_token++;

	return import_node;
}

static AstNode *parse_declaration() {
	switch (current_token->type) {
	case TOKEN_EXTERN:
		current_token++;
		return parse_function(true);
	case TOKEN_FUN:
		return parse_function(false);
	case TOKEN_IMPORT:
		return parse_import();
	default:
		return parse_statement();
	}
}

void parse(List *t, List *nodes) {
    tokens = t;
    current_token = tokens->elements;

    AstNode *expression;
    while (current_token->type != TOKEN_EOF && (current_token - (Token *)tokens->elements) < tokens->length) {
        expression = parse_declaration();
#ifdef DEBUG
        print_tree(expression, 0);
    	putchar('\n');
#endif
    	list_add(nodes, &expression);
	}
}

AstNode *parse_file(char *path, List *t) {
	AstNode *file_node = create_node(AST_FILE);
	file_node->as.file.scope = NULL;
	file_node->as.file.path = path;
	file_node->as.file.tokens = *t;
	list_init(&file_node->as.file.nodes, sizeof(AstNode *));

	parse(&file_node->as.file.tokens, &file_node->as.file.nodes);
	return file_node;
}
