#
# Makefile - Fabio Baracca
#

CC = gcc 
#-DDEBUG #-ggdb
CFLAGS = -O2 -s -fno-strength-reduce
LDFLAGS = 
CNTFLGS =
EDITOR = joe

default: cdjob psxdump remainder

cdjob: cdjob.c
	$(CC) $(CFLAGS) cdjob.c -o cdjob

psxdump: psxdump.c
	$(CC) $(CFLAGS) psxdump.c -o psxdump

remainder:
	@echo "Make completed."
	@echo "Please also execute make batchconf to revise configuration of batch files."

batchconf:
	$(EDITOR) write-psx
	$(EDITOR) read-psx

clean:
	rm -f *~
	rm -f *.o

distclean:
	rm -f psxdump
	rm -f cdjob