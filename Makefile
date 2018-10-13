.PHONY: all clean
.SUFFIXES:

all: test-memory

test-memory: test-memory.c
	cc -o $@ -O2 $^

clean:
	rm -v test-memory


