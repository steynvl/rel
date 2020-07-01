# Copyright 2007-2009 Russ Cox.  All Rights Reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

CC=gcc
CFLAGS=-ggdb -Wall -O2 `pkg-config --cflags --libs glib-2.0`

TARG=re
OFILES=\
	compile.o\
	transition.o\
	buildprog.o\
	utils.o\
	hashset.o\
	main.o\
	pike.o\
	sub.o\
	y.tab.o\

HFILES=\
	regexp.h\
	utils.h\
	hashset.h\
	transition.h\
	y.tab.h\

re: $(OFILES)
	$(CC) -o re $(OFILES) -lglib-2.0

%.o: %.c $(HFILES)
	$(CC) -c $(CFLAGS) $*.c

y.tab.h y.tab.c: parse.y
	bison -v -y parse.y

clean:
	rm -f *.o core re y.tab.[ch] y.output
