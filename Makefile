LIBS =
INCS =
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -g -O0 -std=c99 -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}
LINT = splint
LINTFLAGS = -nullret -predboolint

all: javap

javap: javap.o util.o class.o file.o instr.o
	${CC} -o $@ javap.o util.o class.o file.o instr.o ${LDFLAGS}

javap.o: class.h file.h
file.o:  class.h util.h

lint:
	-${LINT} ${CPPFLAGS} ${LINTFLAGS} javap.c util.c class.c file.c instr.c

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	-rm javap *.o

.PHONY: all clean lint
