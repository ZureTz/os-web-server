PROGRAM = webserver
FILES.c = src/webserver.c src/interrupt.c src/logger.c src/timer.c src/business.c src/threadpool.c src/cache.c
FILES.h = src/include/interrupt.h src/include/logger.h src/include/timer.h src/include/types.h src/include/business.h src/include/threadpool.h src/include/cache.h
FILES.o = ${FILES.c:.c=.o}

CC      = gcc
SFLAGS  = -std=gnu17
GFLAGS  = -g
OFLAGS  = -O0
WFLAG1  = -Wall
WFLAG2  = -Wextra
WFLAG3  = -Werror
WFLAGS  = ${WFLAG1} ${WFLAG2} ${WFLAG3}
UFLAGS  = # Set on command line only
GLIBFLAGS = `pkg-config --cflags glib-2.0`

CFLAGS  = ${SFLAGS} ${GFLAGS} ${OFLAGS} ${WFLAGS} ${UFLAGS}  ${GLIBFLAGS}
LDFLAGS = 
LDLIBS  = `pkg-config --libs glib-2.0`

all:	${PROGRAM}

${PROGRAM}: ${FILES.c} ${FILES.h} ${FILES.o}
	${CC} -o $@ ${CFLAGS} ${FILES.o} ${LDFLAGS} ${LDLIBS} 
		
RM_FR  = rm -fr
SRC_STR = src/
EMPTY = 

RUNTIME_ARGS = 8088 web/

run: all
	./${PROGRAM} ${RUNTIME_ARGS}


clean:
	${RM_FR} ${FILES.o} ${PROGRAM}

remake: clean all 
