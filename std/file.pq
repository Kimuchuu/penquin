extern fun close(fd: s4): s4;
extern fun open(path: *s1, flags: s4): s4;
extern fun read(fd: s4, buf: *s4, count: s4): s4;

fun read_file(path: *s1, buf: *s1, len: s4) {
	fd = open(path, 0);
	n_read = read(fd, buf, len);
	close(fd);
}
