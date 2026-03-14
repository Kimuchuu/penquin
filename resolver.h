#ifndef PENQUIN_RESOLVER_H
#define PENQUIN_RESOLVER_H

#include "parser.h"
#include "table.h"

char *resolve_module_path(char *dir, String module_name);
void resolve(AstNode *node, char *dir);
void resolver_initialize(Table *modules);

#endif
