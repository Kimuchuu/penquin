extern fun puts(str: *s1): s4;
extern fun putchar(char: s1): s4;
extern fun printf(s: *s1, ...);

fun print(str: *s1) {
	puts(str);
}

fun print_format(str: *s1, ...rest: any) {
	rest_i = 0;
	i = 0;
	c = str[i];
	while c != 0 {
		if c == 37 {
			if str[i + 1] == 100 {
				i = i + 1;
				match rest[rest_i] {
					s4 d -> printf("%d", d)
				};
				rest_i = rest_i + 1;
			} else if str[i + 1] == 115 {
				i = i + 1;
				match rest[rest_i] {
					*s1 s -> printf("%s", s)
				};
				rest_i = rest_i + 1;
			} else putchar(c);
		} else putchar(c);

		i = i + 1;
		c = str[i];
	}
}
