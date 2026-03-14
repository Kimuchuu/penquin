#!/bin/sh

set -e

gcc -std=c11 -g -O0 -Wall\
	-o build/penquin\
	-lLLVM\
	main.c list.c token.c parser.c codegen.c table.c string.c file.c resolver.c typechecker.c
