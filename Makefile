LIBS =
INCS =
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -g -O0 -std=c99 -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}
LINT = splint
LINTFLAGS = -nullret -predboolint

all: javap java

javap: javap.o util.o class.o file.o
	${CC} -o $@ javap.o util.o class.o file.o ${LDFLAGS}

java: java.o util.o class.o file.o
	${CC} -o $@ java.o util.o class.o file.o ${LDFLAGS}

java.o:  class.h util.h file.h
javap.o: class.h util.h file.h
file.o:  class.h util.h

lint:
	-${LINT} ${CPPFLAGS} ${LINTFLAGS} javap.c util.c class.c file.c

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	-rm java javap *.o

.PHONY: all clean lint
