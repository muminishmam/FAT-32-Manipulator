make:
	clang -Wall -Wpedantic -Wextra -Werror fat32.c -o fat32

clean:
	rm -f fat32