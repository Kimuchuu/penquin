#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <llvm-c/Linker.h>
#include "common.h"
#include "table.h"
#include "token.h"
#include "parser.h"
#include "codegen.h"


AstNode *build_file_node(char *path, char *buffer) {
	List tokens;
	list_init(&tokens, sizeof(Token));
	scan(buffer, &tokens);
#ifdef DEBUG
	printf("number of tokens: %d\n", tokens.length);
	for (int i = 0; i < tokens.length; i++) {
		Token t = ((Token *)tokens.elements)[i];
		printf("token %s\n", token_type_to_string(t.type));
	}
#endif
	return parse_file(path, &tokens);
}

void traverse_imports(Table *file_node_table, AstNode *file_node, char *dir) {
	List *nodes = &file_node->as.file.nodes;
	for (int i = 0; i < nodes->length; i++) {
		AstNode *node = LIST_GET(AstNode *, nodes, i);
		if (node->type == AST_IMPORT) {
			char *import_path = resolve_module_path(dir, node->as.import.path);
			char *import_dir = get_directory(import_path);

#ifdef DEBUG
			printf("import_path: %s\n", import_path);
			printf("import_dir: %s\n", import_dir);
#endif

			AstNode *import_module = table_get(file_node_table, STRING(import_path));
			if (import_module != NULL) {
				free(import_path);
				free(import_dir);
				continue;
			}
			char *buffer;
			read_file_from_path(import_path, &buffer);

			AstNode *module = build_file_node(import_path, buffer);

			table_put(file_node_table, STRING(import_path), module);
			traverse_imports(file_node_table, module, import_dir);
			free(import_dir);
		}
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: penquin file\n");
		exit(1);
	}

	char *name = path_to_name(argv[1]);
	char *dir = get_directory(argv[1]);
	
	char *buffer;
	read_file_from_path(argv[1], &buffer);

	AstNode *main_file_node = build_file_node(argv[1], buffer);

	Table modules;
	table_init(&modules);
	traverse_imports(&modules, main_file_node, dir);

	compiler_initialize(&modules);
	LLVMModuleRef main_llvm_module = build_module(main_file_node, dir, name, true);

	AstNode **module_list = (AstNode **) table_get_all(&modules);
	for (int i = 0; i < modules.length; i++) {
		AstNode *file_node = module_list[i];
		LLVMModuleRef llvm_module = build_module(file_node, dir, file_node->as.file.path, false);
		LLVMLinkModules2(main_llvm_module, llvm_module);
	}
	compile(main_llvm_module, "test");
#ifdef DEBUG
	printf("exec:\n\n");
#endif
	execl("test", "test", (char *) NULL);

	return 0;
}
