extern fun puts(str: *s1): s4;
extern fun exit(status: s4);

fun main(): s4 {
	puts("first test");
	exit(42);
	puts("second test");
	return 69;
}
