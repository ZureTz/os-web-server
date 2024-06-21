PROGRAM = webserver
FILES.c = src/webserver.c src/interrupt.c src/logger.c src/timer.c src/business.c src/threadpool.c src/cache.c src/memory.c
FILES.h = src/include/interrupt.h src/include/logger.h src/include/timer.h src/include/types.h src/include/business.h src/include/threadpool.h src/include/cache.h src/include/memory.h
FILES.o = ${FILES.c:.c=.o}

NGINX_FILES.c = nginx-malloc/ngx_alloc.c nginx-malloc/ngx_palloc.c 
NGINX_FILES.h = nginx-malloc/ngx_alloc.h nginx-malloc/ngx_config.h nginx-malloc/ngx_core.h nginx-malloc/ngx_palloc.h
NGINX_FILES.o = ${NGINX_FILES.c:.c=.o}

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

${PROGRAM}: ${FILES.c} ${FILES.h} ${FILES.o} ${NGINX_FILES.c} ${NGINX_FILES.h} ${NGINX_FILES.o}
	${CC} -o $@ ${CFLAGS} ${FILES.o} ${NGINX_FILES.o} ${LDFLAGS} ${LDLIBS} 
		
RM_FR  = rm -fr
SRC_STR = src/
EMPTY = 

RUNTIME_ARGS = 8088 web/

run: all
	./${PROGRAM} ${RUNTIME_ARGS}

clean:
	${RM_FR} ${FILES.o} ${NGINX_FILES.o} ${PROGRAM}

remake: clean all 
