import "std:core"

fun main(): s4 {
	x = 12;
	
	if x >= 11 {
		core::print("x >= 10");
	} else if x < 10 {
		core::print("x < 10");
	} else if x > 10 {
		core::print("x > 10");
	} else {
		core::print("x = 0");
	}

	return 0;
}
