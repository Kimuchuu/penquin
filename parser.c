#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "list.h"
#include "token.h"

#define ALLOCATE(value) malloc(sizeof(value))


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
        case AST_NUMBER:
            printf("%f", node->as.number);
            break;
        case AST_OPERATOR:
            printf("(");
            print_tree(node->left);
            printf(" %c ", node->as.operator);
            print_tree(node->right);
            printf(")");
            break;
        case AST_VARIABLE:
        case AST_STRING: {
            char str[node->as.string.length + 1];
            str[node->as.string.length] = '\0';
            memcpy(str, node->as.string.p, node->as.string.length);
            printf("%s", str);
            break;
        }
        case AST_ASSIGNMENT:
            printf("(");
            print_tree(node->left);
            printf(" = ");
            print_tree(node->right);
            printf(")");
            break;
        case AST_FUNCTION_CALL:
            printf("(");
            print_tree(node->left);
			List *arguments = &node->as.arguments;
			for (int i = 0; i < node->as.arguments.length; i++) {
				printf(" ");
				print_tree(LIST_GET(AstNode *, arguments, i));
			}
            printf(")");
            break;
        case AST_FUNCTION:
            printf("(fn:");
			print_tree(node->left);
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
        case AST_WHILE:
            printf("(while ");
			print_tree(node->left);
			List *statements = &node->as.block.statements;
			AstNode *statement;
			for (int i = 0; i < statements->length; i++) {
				statement = LIST_GET(AstNode *, statements, i);
				print_tree(statement);
			}
            printf(")");
            break;
    }
}

static void consume(enum TokenType type) {
    if (current_token->type != type) {
        fprintf(stderr, "Epic fail, expected token: %s.\n", token_type_to_string(type));
        exit(1);
    }
    current_token++;
}

static char consume_if(enum TokenType type) {
    if (current_token->type != type) {
        return 0;
    }
    current_token++;
	return 1;
}


static inline AstNode *create_node(AstType type) {
    AstNode *node = ALLOCATE(AstNode);
    node->type = type;
    node->left = NULL;
    node->right = NULL;
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
    node->as.operator = *current_token->raw;
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

static AstNode *parse_call() {
    AstNode *primary = parse_primary();
	if (primary->type == AST_VARIABLE && current_token->type == TOKEN_LEFT_PAREN) {
		current_token++;
		AstNode *call = create_node(AST_FUNCTION_CALL);
		call->left = primary;
		primary = call;
		list_init(&call->as.arguments, sizeof(AstNode *));
		if (current_token->type != TOKEN_RIGHT_PAREN) {
			AstNode *argument = parse_expression();
			list_add(&call->as.arguments, &argument);
			while (current_token->type == TOKEN_COMMA) {
				current_token++;
				argument = parse_expression();
				list_add(&call->as.arguments, &argument);
			}
		}
		consume(TOKEN_RIGHT_PAREN);
	}
    return primary;
}

static AstNode *parse_factor() {
    AstNode *call = parse_call();
    while (current_token->type == TOKEN_STAR || current_token->type == TOKEN_SLASH) {
        AstNode *operator = create_operator();
        current_token++;
        operator->left = call;
        AstNode *other = parse_call();
        operator->right = other;
        call = operator;
    }
    return call;
}

static AstNode *parse_term() {
    AstNode *factor = parse_factor();
    while (current_token->type == TOKEN_PLUS || current_token->type == TOKEN_MINUS) {
        AstNode *operator = create_operator();
        current_token++;
        operator->left = factor;
        AstNode *other = parse_factor();
        operator->right = other;
        factor = operator;
    }
    return factor;
}

static AstNode *parse_assignment() {
    AstNode *dst = parse_term();
    if (current_token->type == TOKEN_EQUAL) {
        assert(dst->type == AST_VARIABLE);
        AstNode *ass = create_node(AST_ASSIGNMENT);
        current_token++;
        ass->left = dst;
        ass->right = parse_term();
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

static AstNode *parse_while_statement() {
	if (current_token->type == TOKEN_WHILE) {
		AstNode *while_node = create_node(AST_WHILE);
		current_token++;

		AstNode *expr = parse_expression();
		while_node->left = expr;

		consume(TOKEN_LEFT_BRACE);
		list_init(&while_node->as.block.statements, sizeof(AstNode *));
		while (current_token->type != TOKEN_RIGHT_BRACE && (current_token - (Token *)tokens->elements) < tokens->length) {
			AstNode *statement = parse_statement();
			list_add(&while_node->as.block.statements, &statement);
		}
		consume(TOKEN_RIGHT_BRACE);
		return while_node;
	}
	return parse_expression_statement();
}

static AstNode *parse_function() {
	if (current_token->type == TOKEN_FUN) {
		AstNode *fn_node = create_node(AST_FUNCTION);
		current_token++;
		if (current_token->type != TOKEN_IDENTIFIER) {
			fprintf(stderr, "Epic fail, expected identifier but got: %s.\n",
					token_type_to_string(current_token->type));
			exit(1);
		}

		
		AstNode *identifier_node = create_variable();
		identifier_node->as.string.p = current_token->raw;
		identifier_node->as.string.length = current_token->length;
		fn_node->left = identifier_node;
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
		}


		consume(TOKEN_RIGHT_PAREN);

		// Type
		if (current_token->type == TOKEN_COLON) {
			current_token++;
			if (current_token->type != TOKEN_IDENTIFIER) {
				fprintf(stderr, "Epic fail, expected identifier (type) but got: %s.\n",
						token_type_to_string(current_token->type));
				exit(1);
			}

			AstNode *type_node = create_variable();
			type_node->as.string.p = current_token->raw;
			type_node->as.string.length = current_token->length;
			fn_node->right = type_node;
			current_token++;
		}

		if (current_token->type == TOKEN_LEFT_BRACE) {
			current_token++;
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
	return parse_while_statement();
}

static AstNode *parse_statement() {
    return parse_function();
}

void parse(List *t, List *nodes) {
    tokens = t;
    current_token = tokens->elements;

    AstNode *expression;
    while (current_token->type != TOKEN_EOF && (current_token - (Token *)tokens->elements) < tokens->length) {
        expression = parse_statement();
        print_tree(expression);
    	putchar('\n');
    	list_add(nodes, &expression);
	}
}
