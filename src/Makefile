CC = gcc
CFLAGS += -g

simpleshell : simpleshell.c
	${CC} ${CFLAGS} -o $@ $^

.PHONY: clean
clean:
	@rm -rvf *.o