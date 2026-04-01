import "std:core"

fun main(): s4 {
	x = 5;
	numbers = [83, 3, 9, 10, 8];
	while x && x != 0 && true {
		x = x - 1;
		core::print_format("list contains: %d - %d - %d\n", numbers[x], numbers[x] + 1, numbers[x] + 2);
	}
	return 0;
}

