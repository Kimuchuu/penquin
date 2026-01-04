import "std:core"
import "hello"

extern fun puts(str: *s1): s4;

fun main(): s4 {
	msg = hello::say();
	core::print(msg);
	return 0;
}
