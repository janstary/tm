#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include "tm.h"

char	blank = '0';
int	qflag = 0;
int	tflag = 0;
int	Tflag = 0;

static void
usage()
{
	fprintf(stderr, "usage: tm [-b blank] [-qtT] machine\n");
}

struct inst*
mkinst(char R, char W, char M, struct state *t)
{
	struct inst *i = NULL;
	if (t == NULL) {
		warn("jump state missing");
		return NULL;
	}
	if ((i = (struct inst*)calloc(1, sizeof(struct inst))) == NULL)
		err(1, NULL);
	i->r = R;
	i->w = W;
	i->m = M;
	i->t = t;
	return i;
}

struct inst*
getinst(struct inst *list, char R)
{
	struct inst *i;
	if (list == NULL)
		return NULL;
	for (i = list; i; i = i->next)
		if (i->r == R)
			return i;
	return NULL;
}

int
addinst(struct state* s, struct inst *i)
{
	if (s == NULL)
		return -1;
	if (s->inst == NULL) {
		s->inst = i;
		s->last = i;
	} else {
		s->last->next = i;
		s->last = i;
	}
	return 0;
}

void
prstat(struct state *s)
{
	struct inst *i;
	if (s == NULL)
		return;
	for (i = s->inst; i; i = i->next)
		printf("%c%c%c%c%c\n", s->s, i->r, i->w, i->m, i->t->s);
}

struct state*
mkstat(char S)
{
	struct state *s = NULL;
	if ((s = (struct state *)calloc(1, sizeof(struct state))) == NULL)
		err(1, NULL);
	s->s = S;
	return s;
}

struct state*
getstat(struct state *list, char S)
{
	struct state *s;
	if (list == NULL)
		return NULL;
	for (s = list; s; s = s->next)
		if (s->s == S)
			return s;
	return NULL;
}

int
addstat(struct tm* tm, struct state *s)
{
	if (tm == NULL)
		return -1;
	if (tm->list == NULL) {
		tm->list = s;
		tm->last = s;
	} else {
		tm->last->next = s;
		tm->last = s;
	}
	return 0;
}

int
add(struct tm* tm, char S, char R, char W, char M, char T)
{
	struct inst  *i;
	struct state *s, *t;
	if (tm == NULL)
		return -1;
	if ((s = getstat(tm->list, S))) {
		if ((i = getinst(s->inst, R))) {
			warnx("conflicting instructions for (%c,%c)", S, R);
			warnx("old: %c%c%c%c%c", S, R, i->w, i->m, i->t->s);
			warnx("new: %c%c%c%c%c", S, R, W, M, T);
			return -1;
		}
	} else {
		s = mkstat(S);
		addstat(tm, s);
	}
	if ((t = getstat(tm->list, T)) == NULL) {
		t = mkstat(T);
		addstat(tm, t);
	}
	i = mkinst(R, W, M, t);
	addinst(s, i);
	return 0;
}

void
prtape(struct tm *tm)
{
	char *c;
	if (tm == NULL || tm->tape == NULL)
		return;
	if (Tflag) {
		for (c = tm->tape; c && *c; c++) {
			if (c == tm->head) {
				if (isatty(1))
					printf("\033[1m%c\033[22m", *c);
				else
					printf("%c%c%c", *c, 0x08, *c);

			} else {
				putchar(*c);
			}
		}
		if (tm->s) {
			/* beware the empty machine */
			printf(" %c", tm->s->s);
		}
		putchar('\n');
	} else {
		printf("%s\n", tm->tape);
	}
		
}

/* Given an input line, prepare the machine's tape.
 * Point the head to the first 1 (it there is one).
 * Return length for success, 0 for empty line, -1 for error. */
int
mktape(struct tm *tm, char* line)
{
	char *c;
	char *h = NULL;
	size_t len = 0;
	if (tm == NULL)
		return -1;
	if (line == NULL || *line == '\n' || *line == 0)
		return 0;
	for (c = line, len = 0; c; c++, len++) {
		if (*c == '\n') {
			*c = 0;
			break;
		}
		if (!isalnum(*c)) {
			warnx("'%c' is not a valid tape symbol", *c);
			return -1;
		}
		if ((*c != blank) && (h == NULL)) {
			/* the first non-blank */
			h = c;
		}
	}
	tm->tape = strdup(line);
	tm->head = h ? tm->tape + (h - line) : tm->tape + len/2;
	tm->tlen = len;
	return len;
}

void
prtm(struct tm *tm)
{
	struct state *s;
	if (tm == NULL)
		return;
	for (s = tm->list; s; s = s->next)
		prstat(s);
}

struct tm*
mktm(const char* file)
{
	FILE* f;
	char s, r, w, m, t;
	struct tm *tm = NULL;
	if ((f = fopen(file, "r")) == NULL)
		err(1, "%s", file);
	if ((tm = (struct tm*)calloc(1, sizeof(struct tm))) == NULL)
		err(1, NULL);
	while (fscanf(f, "%c%c%c%c%c\n", &s, &r, &w, &m, &t) == 5) {
		if (!(isalnum(s))) {
			warnx("'%c' is not a valid state name", s);
			goto bad;
		}
		if (!(isalnum(r))) {
			warnx("'%c' is not a valid tape symbol", r);
			goto bad;
		}
		if (!(isalnum(w))) {
			warnx("'%c' is not a valid tape symbol", w);
			goto bad;
		}
		if (!(ISMOVE(m))) {
			warnx("'%c' is not a valid move", m);
			goto bad;
		}
		if (!(isalnum(t))) {
			warnx("'%c' is not a valid state name", t);
			goto bad;
		}
		if (add(tm, s, r, w, m, t) == -1) {
			warnx("could not add %c%c%c%c%c", s, r, w, m, t);
			goto bad;
		}
	}
	tm->s = tm->list;
	fclose(f);
	return tm;
bad:
	fclose(f);
	free(tm);
	tm = NULL;
	return NULL;
}

void
reset(struct tm* tm)
{
	if (tm == NULL)
		return;
	tm->s = tm->list;
	free(tm->tape);
	tm->tlen = 0;
}

/* Given a machine and a next head move, check if 
 * the move would be out of alloce'd segment. If so
 * expand (double the size) of the tape in direction 
 * of the next head move. */
int 
chktape(struct tm* tm, char m)
{
	const int expConst = 2;
	size_t tapeLen = tm->tlen, headPos = tm->head - tm->tape; 
	
	if ((tm->head == tm->tape && m == 'L') || (tm->head == tm->tape + tapeLen - 1 && m == 'R')) {
		char* newTape = (char*)calloc(tapeLen * expConst + 1, sizeof(*newTape));
		int offset = tm->head == tm->tape ? tapeLen : 0;
		
		if (!newTape) {
			printf("New memory block could not be allocated!\n");
			printf("Terminating.\n");
			return -1;
		}
		
		for (size_t i = 0; i < tapeLen * expConst; ++i)
			newTape[i] = '0';
		newTape[tapeLen] = '\0';

		memcpy(newTape + offset, tm->tape, tapeLen);
		free(tm->tape);
		
		tm->head = newTape + offset + headPos;
		tm->tape = newTape;
		tm->tlen = tm->tlen * expConst;
	}
	return 0;
}

int
run(struct tm *tm)
{
	struct inst *i;
	if (tm == NULL)
		return -1;
	do {
		if (tflag)
			prtape(tm);
		if (tm->s == NULL
		|| ((i = getinst(tm->s->inst, *tm->head)) == NULL)) {
			/* halt */
			break;
		}
		if (chktape(tm, i->m) == -1) {
			/* failed memory alloc */
			return -1;
		}
		tm->s = i->t;
		*tm->head = i->w;
		if (i->m == 'L') {
			tm->head--;
		} else if (i->m == 'R') {
			tm->head++;
		}
	} while (1);
	if (!tflag && !qflag)
		prtape(tm);
	return 0;
}

/* Simple free function for singly tm (singly linked list) */
void freetm(struct tm* tm) 
{
	struct state* s = tm->list, * prev = NULL;
	while (s) {
		prev = s;
		s = s->next;
		struct inst* i = prev->inst, * previ = NULL;
		while (i) {
			previ = i;
			i = i->next;
			free(previ);
		}
		free(prev);
	}
	free(tm);
	tm = NULL;
}

int
main(int argc, char** argv)
{
	int c;
	struct tm *tm = NULL;
	ssize_t len = 0;
	size_t size = 0;
	char *line = NULL;

	while ((c = getopt(argc, argv, "b:qtT")) != -1) switch (c) {
		case 'b':
			if (!isalnum(blank = *optarg)) {
				warnx("'%c' is not a valid blank", blank);
				return -1;
			}
			break;
		case 'q':
			qflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'T':
			Tflag = 1;
			tflag = 1;
			break;
		default:
			usage();
			return -1;
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		return -1;
	}

	if ((tm = mktm(*argv)) == NULL) {
		warnx("error parsing instructions");
		return -1;
	}

	while ((len = getline(&line, &size, stdin)) != -1) {
		switch (mktape(tm, line)) {
			case -1:
				warnx("invalid tape: %s", line);
				break;
			case 0:
				/* empty */
				break;
			default:
				run(tm);
				break;
		}
		reset(tm);
	}

	free(line);
	freetm(tm);
	return 0;
}
