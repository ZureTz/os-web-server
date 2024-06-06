PROGRAM = webserver
FILES.c = src/webserver.c src/interrupt.c src/logger.c src/timer.c src/business.c src/threadpool.c
FILES.h = src/include/interrupt.h src/include/logger.h src/include/timer.h src/include/types.h src/include/business.h src/include/threadpool.h
FILES.o = ${FILES.c:.c=.o}

CC      = gcc
SFLAGS  = -std=gnu17
GFLAGS  = -g
OFLAGS  = -O0
WFLAG1  = -Wall
WFLAG2  = -Wextra
WFLAG3  = # -Werror
WFLAG4  = # -Wstrict-prototypes
WFLAG5  = # -Wmissing-prototypes
WFLAGS  = ${WFLAG1} ${WFLAG2} ${WFLAG3} ${WFLAG4} ${WFLAG5}
UFLAGS  = # Set on command line only

CFLAGS  = ${SFLAGS} ${GFLAGS} ${OFLAGS} ${WFLAGS} ${UFLAGS} -I/usr/local/include/glib-2.0 -I/usr/local/lib/aarch64-linux-gnu/glib-2.0/include -I/usr/local/include -pthread 
LDFLAGS = 
LDLIBS  = -L/usr/local/lib/aarch64-linux-gnu -lglib-2.0

all:	${PROGRAM}

${PROGRAM}: ${FILES.c} ${FILES.h} ${FILES.o}
	${CC} -o $@ ${CFLAGS} ${FILES.o} ${LDFLAGS} ${LDLIBS} 
		
RM_FR  = rm -fr
SRC_STR = src/
EMPTY = 

clean:
	${RM_FR} ${subst ${SRC_STR}, ${EMPTY}, ${FILES.o}} ${FILES.o} ${PROGRAM}

RUNTIME_ARGS = 8088 web/

run: all
	./${PROGRAM} ${RUNTIME_ARGS}
