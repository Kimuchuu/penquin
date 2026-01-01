fun fib(n: s4): s4 {
    if n <= 1 {
        return n;
	}
	return fib(n - 2) + fib(n - 1);
}

fun main(): s4 {
	x = 0;
	i = 0;
	while i < 14 {
		x = fib(i);
		i = i + 1;
	}
	return x;
}
