PROGRAM = test
FILES.c = ngx_alloc.c ngx_palloc.c timer.c test.c
FILES.h = ngx_alloc.h ngx_config.h ngx_core.h ngx_palloc.h timer.h
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

CFLAGS  = ${SFLAGS} ${GFLAGS} ${OFLAGS} ${WFLAGS} ${UFLAGS}
LDFLAGS = 
LDLIBS  = 

all:	${PROGRAM}

${PROGRAM}: ${FILES.c} ${FILES.h} ${FILES.o}
	${CC} -o $@ ${CFLAGS} ${FILES.o} ${LDFLAGS} ${LDLIBS} 
		
RM_FR  = rm -fr
SRC_STR = src/
EMPTY = 

clean:
	${RM_FR} ${FILES.o} ${PROGRAM}

remake: clean all 
