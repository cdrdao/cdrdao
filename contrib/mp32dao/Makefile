CC = gcc
LIBS = -lm -lpthread -lmpeg3 -lsndfile
CFLAGS += -O3 -fomit-frame-pointer -Wall

OBJS = mp3dec.o mp3dec_wrap.o

MODE = 555
#----------Put here path to SWIG binary
SWIG = /usr/bin/swig
#----------Put here path to perl include files
PERLDIR = /usr/lib/perl5/5.6.0/i686-linux
INCLUDE = $(PERLDIR)/CORE
#----------mp32dao will go here
BINDIR = /usr/bin/

OUTPUT = mp3dec.so

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CC) -shared mp3dec_wrap.o mp3dec.o $(LIBS) -o $(OUTPUT)
	$(CC) -o mp3dec mp3dec.c $(LIBS)
        
mp3dec.o:
	$(CC) -fPIC -D_REENTRANT -c mp3dec.c

mp3dec_wrap.o:
	$(SWIG) -perl5 mp3dec.i
	$(CC) -fPIC -c -Dbool=char -I $(INCLUDE) mp3dec_wrap.c

clean:        
	rm -f $(OBJS) $(OUTPUT) mp3dec_wrap.c mp3dec.pm mp3dec

install:
	install -m $(MODE) $(OUTPUT) $(PERLDIR)/$(OUTPUT)
	install -m $(MODE) mp3dec.pm $(PERLDIR)/mp3dec.pm
	install -m $(MODE) mp3handler.pm $(PERLDIR)/mp3handler.pm
	install -m $(MODE) mp32dao.pl $(BINDIR)/mp32dao.pl

uninstall:
	rm -f $(PERLDIR)/$(OUTPUT)
	rm -f $(PERLDIR)/mp3handler.pm
	rm -f $(PERLDIR)/mp3dec.pm	
	rm -f $(BINDIR)/mp32dao.pl
