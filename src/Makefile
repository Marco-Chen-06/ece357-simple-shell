CC = gcc
CFLAGS += -g

simpleshell :  main.c simpleshell.c
	${CC} ${CFLAGS} -o $@ $^

.PHONY: clean
clean:
	@rm -rvf *.o