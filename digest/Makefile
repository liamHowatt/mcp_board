test: test.c digest/sha2.h digest/sha2.c digest/finalize.h digest/finalize.c digest/digest.h
	gcc -Wall -Wno-parentheses -g -fsanitize=address test.c digest/sha2.c digest/finalize.c -o test

run:
	echo -n 'Lorem ipsum dolor sit amet, consectetur adipiscing elit viverra fusce.' | sha256sum
	./test | hexdump -C
