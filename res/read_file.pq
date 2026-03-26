import "std:core"
import "std:file"

fun main(): s4 {
	path = "./res/read_file.pq";
	count = 2000;
	buf: s4[2000];
	file::read_file(path, buf, count);
	core::print(buf);
	return 0;
}

