PROGRAM = webserver
FILES.c = src/webserver.c
FILES.h = 
FILES.o = ${FILES.c:.c=.o}

CC      = gcc
SFLAGS  = -std=c17
GFLAGS  = -g
OFLAGS  = -O0
WFLAG1  = # -Wall
WFLAG2  = # -Wextra
WFLAG3  = # -Werror
WFLAG4  = # -Wstrict-prototypes
WFLAG5  = # -Wmissing-prototypes
WFLAGS  = ${WFLAG1} ${WFLAG2} ${WFLAG3} ${WFLAG4} ${WFLAG5}
UFLAGS  = # Set on command line only

CFLAGS  = ${SFLAGS} ${GFLAGS} ${OFLAGS} ${WFLAGS} ${UFLAGS}
LDFLAGS =
LDLIBS  =

all:  ${PROGRAM}

${PROGRAM}: ${FILES.c} ${FILES.h} ${FILES.o}
	${CC} -o $@ ${CFLAGS} ${FILES.o} ${LDFLAGS} ${LDLIBS} 
		
RM_FR  = rm -fr
SRC_STR = src/
EMPTY = 

clean:
	${RM_FR} ${subst ${SRC_STR}, ${EMPTY}, ${FILES.o}} ${FILES.o} ${PROGRAM}