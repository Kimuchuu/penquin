#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "list.h"
#include "token.h"

List    *tokens;
Token   *current_token;
AstNode *current_node;

static AstNode *parse_expression();
static AstNode *parse_statement();

static void print_tree(AstNode *node) {
    if (node == NULL) {
        return;
    }
    switch (node->type) {
        case AST_ACCESSOR:
            printf("(");
            print_tree(node->as.accessor.left);
            printf("::");
            print_tree(node->as.accessor.right);
            printf(")");
            break;
	    case AST_ASSIGNMENT: {
			DEFINE_CSTRING(name, node->as.assignment.name)
            printf("(%s = ", name);
            print_tree(node->as.assignment.value);
            printf(")");
            break;
		}
        case AST_BLOCK:
            printf("(");
        		List *statements = &node->as.block.statements;
        		AstNode *statement;
        		for (int i = 0; i < statements->length; i++) {
        				statement = LIST_GET(AstNode *, statements, i);
        				print_tree(statement);
        		}
            printf(")");
            break;
        case AST_FILE:
            printf("(%s\n", node->as.file.path);
			List *nodes = &node->as.file.nodes;
			for (int i = 0; i < nodes->length; i++) {
				print_tree(LIST_GET(AstNode *, nodes, i));
			}
            printf(")");
            break;
        case AST_FUNCTION: {
			DEFINE_CSTRING(name, node->as.fn.name)
            printf("(fn:%s", name);
			List *parameters = &node->as.fn.parameters;
			Parameter parameter;
			for (int i = 0; i < parameters->length; i++) {
				parameter = LIST_GET(Parameter, parameters, i);
				char str[parameter.type.length + 1];
				str[parameter.type.length] = '\0';
				memcpy(str, parameter.type.p, parameter.type.length);
				printf(" %s", str);
			}
            printf(")");
            break;
		}
        case AST_FUNCTION_CALL:
            printf("(");
            print_tree(node->as.call.variable);
			List *arguments = &node->as.call.arguments;
			for (int i = 0; i < arguments->length; i++) {
				printf(" ");
				print_tree(LIST_GET(AstNode *, arguments, i));
			}
            printf(")");
            break;
        case AST_IF:
            printf("(if ");
            print_tree(node->as.if_.condition);
			printf("then ");
            print_tree(node->as.if_.statement);
			if (node->as.if_.else_statement != NULL) {
				printf("else ");
				print_tree(node->as.if_.else_statement);
			}
            printf(")");
            break;
	    case AST_IMPORT: {
			DEFINE_CSTRING(path, node->as.import.path)
            printf("(import %s)", path);
            break;
		}
        case AST_NUMBER:
            printf("%f", node->as.number);
            break;
        case AST_OPERATOR:
            printf("(");
            print_tree(node->as.operator_.left);
            printf(" %c ", node->as.operator_.type);
            print_tree(node->as.operator_.right);
            printf(")");
            break;
	    case AST_RETURN: {
            printf("(return ");
			print_tree(node->as.return_.expression);
            printf(")");
            break;
		}
        case AST_STRING:
        case AST_VARIABLE: {
			DEFINE_CSTRING(str, node->as.string)
            printf("%s", str);
            break;
        }
        case AST_WHILE:
            printf("(while ");
            print_tree(node->as.while_.statement);
            print_tree(node->as.while_.condition);
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
    return node;
}

static AstNode *create_number() {
    float n = strtof(current_token->raw, NULL);
    AstNode *node = create_node(AST_NUMBER);
    node->as.number = n;
    return node;
}

static AstNode *create_operator() {
    AstNode *node = create_node(AST_OPERATOR);
    node->as.operator_.type = current_token->type;
    return node;
}

static AstNode *create_string() {
    AstNode *node = create_node(AST_STRING);
    node->as.string.p = current_token->raw + 1;
    node->as.string.length = current_token->length - 2;
    return node;
}

static AstNode *create_variable() {
    AstNode *node = create_node(AST_VARIABLE);
    node->as.string.p = current_token->raw;
    node->as.string.length = current_token->length;
    return node;
}

static AstNode *parse_primary() {
    if (current_token->type == TOKEN_NUMBER) {
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

static AstNode *parse_factor() {
    AstNode *call = parse_call();
    while (current_token->type == TOKEN_STAR || current_token->type == TOKEN_SLASH) {
        AstNode *operator = create_operator();
        current_token++;
        operator->as.operator_.left = call;
        operator->as.operator_.right = parse_call();
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
		current_token->type == TOKEN_LESS_THAN_OR_EQUAL) {
        AstNode *operator = create_operator();
        current_token++;
        operator->as.operator_.left = term;
        operator->as.operator_.right = parse_term();
		term = operator;
	}
    return term;
}

static AstNode *parse_assignment() {
    AstNode *dst = parse_comparison();
    if (current_token->type == TOKEN_EQUAL) {
        assert(dst->type == AST_VARIABLE);
        AstNode *ass = create_node(AST_ASSIGNMENT);
        current_token++;
        ass->as.assignment.name = dst->as.string;
        ass->as.assignment.value = parse_comparison();
		free(dst);
        dst = ass;
    }
    return dst;
}

static AstNode *parse_expression() {
    return parse_assignment();
}

static AstNode *parse_expression_statement() {
    AstNode *expression = parse_expression();
    consume(TOKEN_SEMICOLON);
    return expression;
}

static AstNode *parse_return_statement() {
	AstNode *return_node = create_node(AST_RETURN);
	current_token++;
	return_node->as.return_.expression = parse_expression_statement();
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
		return parse_expression_statement();
	}
}

static AstNode *parse_block() {
	consume(TOKEN_LEFT_BRACE);
	AstNode *block_node = create_node(AST_BLOCK);
	
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
	current_token++;
	if (current_token->type != TOKEN_IDENTIFIER) {
		fprintf(stderr, "Epic fail, expected identifier but got: %s.\n",
				token_type_to_string(current_token->type));
		exit(1);
	}

	fn_node->as.fn.external = external;
    fn_node->as.fn.name.p = current_token->raw;
    fn_node->as.fn.name.length = current_token->length;
	current_token++;

	consume(TOKEN_LEFT_PAREN);

	// Parameters
	if (current_token->type != TOKEN_RIGHT_PAREN) {
		list_init(&fn_node->as.fn.parameters, sizeof(Parameter));
			
		Parameter parameter;
		do {
			if (current_token->type != TOKEN_IDENTIFIER) {
				fprintf(stderr, "Epic fail, expected identifier (type) but got: %s.\n",
						token_type_to_string(current_token->type));
				exit(1);
			}
			parameter.name.p = current_token->raw;
			parameter.name.length = current_token->length;
			current_token++;
			consume(TOKEN_COLON);

			parameter.pointer = consume_if(TOKEN_STAR);
			if (current_token->type != TOKEN_IDENTIFIER) {
				fprintf(stderr, "Epic fail, expected identifier (name) but got: %s.\n",
						token_type_to_string(current_token->type));
				exit(1);
			}
			parameter.type.p = current_token->raw;
			parameter.type.length = current_token->length;
			current_token++;
			list_add(&fn_node->as.fn.parameters, &parameter);
		} while (consume_if(TOKEN_COMMA));
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
        print_tree(expression);
    	putchar('\n');
#endif
    	list_add(nodes, &expression);
	}
}

AstNode *parse_file(char *path, List *t) {
	AstNode *file_node = create_node(AST_FILE);
	file_node->as.file.path = path;
	file_node->as.file.tokens = *t;
	list_init(&file_node->as.file.nodes, sizeof(AstNode *));

	parse(&file_node->as.file.tokens, &file_node->as.file.nodes);
	return file_node;
}
