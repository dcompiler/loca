CC = gcc
#CFLAGS = -O0 -g
CFLAGS = -O2 -Wno-return-type
LDFLAGS = -lm

all: OPTStackDistanceAnalyzer

OPTStackDistanceAnalyzer: OPTStackDistanceAnalyzer.o
	${CC} ${LDFLAGS} OPTStackDistanceAnalyzer.o -o OPTStackDistanceAnalyzer
	${CC} ${LDFLAGS} reverse_file.o -o reverse_file

OPTStackDistanceAnalyzer.o: OPTStackDistanceAnalyzer.c
	${CC} ${CFLAGS} -c OPTStackDistanceAnalyzer.c
	${CC} ${CFLAGS} -c reverse_file.c

clean:
	rm -rf *.o OPTStackDistanceAnalyzer reverse_file
