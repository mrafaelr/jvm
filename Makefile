LIBS =
INCS =
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -g -O0 -std=c99 -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

all: javap

javap: javap.o util.o class.o file.o
	${CC} -o $@ javap.o util.o class.o file.o ${LDFLAGS}

javap.o: class.h file.h
file.o:  class.h util.h

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	-rm javap *.o

.PHONY: all clean
