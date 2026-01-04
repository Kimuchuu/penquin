#include <stdlib.h>
#include <string.h>
#include "common.h"

char *cstring_concat_String(char *c, String s) {
	int clen = strlen(c);
	char *res = malloc(clen + s.length + 1);
	strncpy(res, c, clen);
	strncpy(res + clen, s.p, s.length);
	res[clen + s.length] = '\0';
	return res;
}

char *cstring_duplicate(char *c) {
	char *res = malloc(sizeof(c));
	strcpy(res, c);
	return res;
}

String String_concat_cstring(String s, char *c) {
	int clen = strlen(c);
	String res;
	res.length = s.length + clen;
	res.p = malloc(res.length);
	strncpy(res.p, s.p, s.length);
	strncpy(res.p + s.length, c, clen);
	return res;
}

int String_cmp(String s, char *c) {
	int clen = strlen(c);
	if (clen != s.length) {
		return s.length - clen;
	}
	return strncmp(s.p, c, clen);
}

void String_free(String s) {
	free(s.p);
}

char *String_to_cstring(String s) {
	char *c = (char *)malloc((s.length + 1) * sizeof(char *));
	strncpy(c, s.p, s.length + 1);
	c[s.length] = '\0';
	return c;
}

