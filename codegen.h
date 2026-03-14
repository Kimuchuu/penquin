#ifndef PENQUIN_CODEGEN_H
#define PENQUIN_CODEGEN_H

#include <stdbool.h>
#include <llvm-c/Types.h>
#include "table.h"
#include "parser.h"

LLVMModuleRef build_module(AstNode *file_node, char *dir, char *name, bool entry);
void compile(LLVMModuleRef module, char *name);
void compiler_initialize(Table *modules);

#endif
