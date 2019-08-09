CC=gcc
#CFLAGS=-Wall -O2 -g -I.
CFLAGS=-g -I.

ALLCEXE=$(patsubst %.c,%,$(wildcard *.c))

all: $(ALLCEXE) $(ALLCPPEXE)

.c.o:
	$(CC) -c $(CFLAGS) $<

$(ALLCEXE): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(ALLCEXE) $(ALLCPPEXE)

