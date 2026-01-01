extern fun puts(str: *s1): s4;

fun hello(): *s1 {
	puts("Congratulations you've found the hello phrase");
	return "hello";
}
