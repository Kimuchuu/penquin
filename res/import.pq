import "hello"

extern fun puts(str: *s1): s4;

fun main(): s4 {
	msg = hello::say();
	puts(msg);
	return 0;
}
