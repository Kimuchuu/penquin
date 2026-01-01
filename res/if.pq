fun puts(str: *s1): s4;

fun main(): s4 {
	x = 12;
	
	if x >= 11 {
		puts("x >= 10");
	} else if x < 10 {
		puts("x < 10");
	} else if x > 10 {
		puts("x > 10");
	} else {
		puts("x = 0");
	}

	return 0;
}
