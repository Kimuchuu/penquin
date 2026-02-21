extern fun puts(str: *s1): s4;
extern fun printf(s: *s1, ...);

fun print(str: *s1) {
	puts(str);
}

fun print_format(str: *s1, ...rest: s4) {
	printf(str, rest[0]);
}
