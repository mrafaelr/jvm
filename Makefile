LIBS =
INCS =
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -g -O0 -std=c99 -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}
LINT = splint
LINTFLAGS = -nullret -predboolint

all: java javap

java: java.o util.o class.o file.o frame.o
	${CC} -o $@ java.o frame.o class.o util.o op.o file.o ${LDFLAGS}

javap: javap.o util.o class.o file.o
	${CC} -o $@ javap.o class.o util.o op.o file.o ${LDFLAGS}

java.o:  class.h frame.h op.h util.h file.h
javap.o: class.h         op.h util.h file.h
file.o:  class.h         op.h util.h file.h
op.o:    class.h frame.h op.h
frame.o: class.h frame.h
class.o: class.h

lint:
	-${LINT} ${CPPFLAGS} ${LINTFLAGS} javap.c util.c class.c file.c

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	-rm java javap *.o

.PHONY: all clean lint
