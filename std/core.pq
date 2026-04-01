extern fun puts(str: *s1): s4;
extern fun putchar(char: s1): s4;
extern fun printf(s: *s1, ...);

fun print(str: *s1) {
	puts(str);
}

fun print_format(str: *s1, ...rest: s4) {
	rest_i = 0;
	i = 0;
	c = str[i];
	while c != 0 {
		if c == 37 {
			if str[i + 1] == 100 {
				i = i + 1;
				printf("%d", rest[rest_i]);
				rest_i = rest_i + 1;
			} else if str[i + 1] == 115 {
				i = i + 1;
				printf("%s", rest[rest_i]);
				rest_i = rest_i + 1;
			} else putchar(c);
		} else putchar(c);

		i = i + 1;
		c = str[i];
	}
}
