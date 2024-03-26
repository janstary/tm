PREFIX	= $(HOME)
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/man/man1
CFLAGS	= -Wall -pedantic -D_OPENBSD_SOURCE -lbsd

BINS	= tm
MANS	= tm.1

all: $(BINS)

tm: tm.c tm.h
	$(CC) $(CFLAGS) -o tm tm.c

test: tm
	./tm add.tm    < add.in    | diff - add.out
	./tm sign.tm   < sign.in   | diff - sign.out
	./tm succ.tm   < succ.in   | diff - succ.out
	./tm copy.tm   < copy.in   | diff - copy.out
	./tm empty.tm  < empty.in  | diff - empty.out
	./tm proj23.tm < proj23.in | diff - proj23.out

	./tm bb1.tm    < bb1.in    | diff - bb1.out
	./tm bb2.tm    < bb2.in    | diff - bb2.out
	./tm bb3.tm    < bb3.in    | diff - bb3.out
	./tm bb4.tm    < bb4.in    | diff - bb4.out

	./tm -b _ abcbin.tm < abcbin.in | diff - abcbin.out

lint: $(MANS)
	mandoc -Tlint -Wstyle $(MANS)

install: $(BINS) $(MANS) test
	install -d -m 755 $(BINDIR) && install -m 755 $(BINS) $(BINDIR)
	install -d -m 755 $(MANDIR) && install -m 644 $(MANS) $(MANDIR)

uninstall:
	( cd $(BINDIR) && rm -f -- $(BINS) )
	( cd $(MANDIR) && rm -f -- $(MANS) )

clean:
	rm -f -- $(BINS) *.o *.core *~
