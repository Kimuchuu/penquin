#ifndef PENQUIN_COMMON_H
#define PENQUIN_COMMON_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define DEBUG


#define DEFINE_CSTRING(name, string) char name[string.length + 1];\
									 memcpy(name, string.p, string.length);\
								  	 name[string.length] = '\0';
#define STRING(cp) (String) { .length = strlen(cp), .p = cp }

typedef struct {
    char *p;
    int length;
} String;

char   *cstring_concat_String(char *c, String s);
char   *cstring_duplicate(char *str);
int     String_cmp_cstring(String s, char *c);
int     String_cmp(String s1, String s2);
String  String_concat_cstring(String s, char *c);
void    String_free(String s);
bool    String_starts_with(String s, char *c);
char   *String_to_cstring(String s);

char *get_directory(char *path);
char *path_to_name(char *path);
void read_file(FILE *file, char **data);
void read_file_from_path(char *path, char **data);

#endif
