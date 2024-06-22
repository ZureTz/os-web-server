PROGRAM = wfs
FILES.c = wfs.c file.c
FILES.h = file.h
FILES.o = ${FILES.c:.c=.o}

CC      = gcc
SFLAGS  = -std=gnu17
GFLAGS  = -g
OFLAGS  = -O0
WFLAG1  = -Wall
WFLAG2  = -Wextra
WFLAG3  = # -Werror
WFLAGS  = ${WFLAG1} ${WFLAG2} ${WFLAG3}
UFLAGS  = # Set on command line only

CFLAGS  = ${SFLAGS} ${GFLAGS} ${OFLAGS} ${WFLAGS} ${UFLAGS} ${shell pkg-config --cflags fuse} ${shell pkg-config --cflags glib-2.0}
LDFLAGS = 
LDLIBS  = ${shell pkg-config --libs fuse} ${shell pkg-config --libs glib-2.0}

all:	${PROGRAM}

${PROGRAM}: ${FILES.c} ${FILES.h} ${FILES.o}
	${CC} -o $@ ${CFLAGS} ${FILES.o} ${LDFLAGS} ${LDLIBS} 
	@echo 'To Mount: ./wfs -f [mount point]'
		
RM_FR  = rm -fr

clean:
	${RM_FR} ${FILES.o} ${PROGRAM}

remake: clean all 
