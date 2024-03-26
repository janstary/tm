#include <unistd.h>
#include <string.h>
#include <bsd/stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include "tm.h"

char	blank = '0';
int	qflag = 0;
long	steps = 0;
int	nbusy = 0;
int	tflag = 0;
int	Tflag = 0;

static void
usage(void)
{
	fprintf(stderr,
		"usage: tm [-b blank] [-s steps] [-qtT] machine\n"
		"usage: tm -B num [-s steps] [-qtT]\n");
}

struct inst*
mkinst(char R, char W, char M, struct state *t)
{
	struct inst *i = NULL;
	if (W && t == NULL) {
		warn("jump state missing");
		return NULL;
	}
	if ((i = calloc(1, sizeof(struct inst))) == NULL)
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
		if (i->r == R && i->w)
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
	for (i = s->inst; i; i = i->next) {
		if (i->w)
			printf("%c%c%c%c%c\n", s->s, i->r, i->w, i->m, i->t->s);
		/*
		else
			printf("%c%cnop\n", s->s, i->r);
		*/
	}
}

struct state*
mkstat(char S)
{
	struct state *s = NULL;
	if ((s = calloc(1, sizeof(struct state))) == NULL)
		err(1, NULL);
	s->s = S;
	return s;
}

void
frstat(struct state *s)
{
	struct inst* i;
	struct inst* n;
	if (s) {
		i = s->inst;
		while (i) {
			n = i->next;
			free(i);
			i = n;
		}
		free(s);
	}
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
	struct state *s;
	struct state *t = NULL;
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
	if (T && (t = getstat(tm->list, T)) == NULL) {
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
					printf("\033[4m%c\033[24m", *c);
				else
					printf("%c%c%c", *c, '\b', *c);
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

/* Given an input line, prepare the tape.
 * Point the head to the first non-blank (if any), or in the middle.
 * Return tape length for success, 0 for an empty tape, -1 for error. */
int
mktape(struct tm *tm, char* line)
{
	char *c;
	char *h = NULL;
	size_t len = 0;
	if (tm == NULL)
		return -1;
	if (line == NULL || *line == '\n' || *line == '\0')
		return 0;
	for (c = line, len = 0; c && *c; c++, len++) {
		if (*c == '\0')
			break;
		if (*c == '\n') {
			*c = '\0';
			break;
		}
		if (!isprint(*c)) {
			warnx("'%c' is not a valid tape symbol", *c);
			return -1;
		}
		if (h == NULL && *c != blank)
			h = c;
	}
	tm->tape = strdup(line);
	tm->head = tm->tape + (h ? (h - line) : len/2);
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

void
freetm(struct tm* tm)
{
	struct state *s;
	struct state *n;
	if (tm) {
		s = tm->list;
		while (s) {
			n = s->next;
			frstat(s);
			s = n;
		}
		free(tm->tape);
		free(tm);
	}
}

struct tm*
mktm(const char* file)
{
	FILE* f;
	char s, r, w, m, t;
	struct tm *tm = NULL;
	if ((f = fopen(file, "r")) == NULL)
		err(1, "%s", file);
	if ((tm = calloc(1, sizeof(struct tm))) == NULL)
		err(1, NULL);
	while (fscanf(f, "%c%c%c%c%c\n", &s, &r, &w, &m, &t) == 5) {
		if (!(isprint(s))) {
			warnx("'%c' is not a valid state name", s);
			goto bad;
		}
		if (!(isprint(r))) {
			warnx("'%c' is not a valid tape symbol", r);
			goto bad;
		}
		if (!(isprint(w))) {
			warnx("'%c' is not a valid tape symbol", w);
			goto bad;
		}
		if (!(ISMOVE(m))) {
			warnx("'%c' is not a valid move", m);
			goto bad;
		}
		if (!(isprint(t))) {
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
	freetm(tm);
	fclose(f);
	return NULL;
}

void
reset(struct tm* tm)
{
	if (tm == NULL)
		return;
	free(tm->tape);
	tm->s = tm->list;
	tm->tape = NULL;
	tm->tlen = 0;
	tm->step = 0;
}

/* If a move would take us past the end of the tape,
 * double the tape size in that direction, keep the content,
 * and reposition the head accordingly. */
void
cktape(struct tm *tm, char m)
{
	char *newt;
	size_t newl;
	size_t off;
	if (tm == NULL || tm->tape == NULL)
		return;
	newl = 2 * tm->tlen;
	off = tm->head - tm->tape;
	if ((m == 'L' && off == 0)
	||  (m == 'R' && (off == tm->tlen - 1))) {
		/* FIXME: this does not terminate with \0
		 * Or do we break that with the memset()? */
		if (NULL == (newt = realloc(tm->tape, 1+newl)))
			err(1, NULL);
		if (m == 'R') {
			tm->tape = newt;
			tm->head = newt + off;
			memset(tm->head + 1, blank, tm->tlen);
			tm->tlen = newl;
		} else {
			memcpy(newt + tm->tlen, newt, tm->tlen);
			memset(newt, blank, tm->tlen);
			tm->head = newt + tm->tlen;
			tm->tape = newt;
			tm->tlen = newl;
		}
		tm->tape[tm->tlen] = '\0';
	}
}

long
ones(struct tm *tm)
{
	char *c;
	size_t l;
	long num = 0;
	if (tm->step == steps) {
		/* Must halt. */
		return 0;
	}
	for (l = 0, c = tm->tape; *c && l < tm->tlen; c++, l++)
		if (*c == '1')
			num++;
	return num;
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
		if (steps && tm->step == steps) {
			if (!qflag)
				warnx("Halting after %lu steps", tm->step);
			break;
		}
		if (tm->s == NULL
		|| ((i = getinst(tm->s->inst, *tm->head)) == NULL)) {
			/* halt */
			break;
		}
		tm->s = i->t;
		*tm->head = i->w;
		cktape(tm, i->m);
		if (i->m == 'L') {
			tm->head--;
		} else if (i->m == 'R') {
			tm->head++;
		}
		tm->step++;
	} while (1);
	if (!tflag && !qflag)
		prtape(tm);
	return 0;
}

int
nexti(struct inst *i, struct tm *tm)
{
	if (i == NULL)
		return 0;
	if (i->w == 0) {
		/*printf("upping NOP\n");*/
		i->w = '0';
		i->m = 'L';
		i->t = tm->list;
		return 1;
	}
	/*printf("upping %c%c%c\n", i->w, i->m, i->t->s);*/
	if (i->t->next) {
		i->t = i->t->next;
		return 1;
	}
	if (i->m == 'L') {
		i->m = 'N';
		i->t = tm->list;
		return 1;
	}
	if (i->m == 'N') {
		i->m = 'R';
		i->t = tm->list;
		return 1;
	}
	if (i->w == '0') {
		i->w = '1';
		i->m = 'L';
		i->t = tm->list;
		return 1;
	}
	return 0;
}

int
nexts(struct state *s, struct tm *tm)
{
	struct inst *i;
	/*printf("upping state '%c'\n", s->s);*/
	for (i = s->inst; i ; i = i->next) {
		/*printf("upping for r = '%c'\n", i->r);*/
		if (nexti(i, tm))
			return 1;
		i->w = 0;
		i->m = 0;
		i->t = 0;
	}
	return 0;
}

int
nextm(struct tm *tm)
{
	struct inst *i;
	struct state *s;
	for (s = tm->list; s ; s = s->next) {
		if (nexts(s, tm))
			return 1;
		for (i = s->inst; i; i = i->next) {
			i->w = 0;
			i->m = 0;
			i->t = 0;
		}
	}
	return 0;
}

void
bb(int states)
{
	int s;
	long min = 0;
	long max = 0;
	long num = 0;
	struct tm *tm;
	char* tape = "00000000";

	if ((tm = calloc(1, sizeof(struct tm))) == NULL)
		err(1, NULL);

	for (s = 0; s < states; s++) {
		/* Having (s,r) -> (0, 0, 0) means NOOP */
		add(tm, 'A' + s, '0', 0, 0, 0);
		add(tm, 'A' + s, '1', 0, 0, 0);
	}

	do {
		reset(tm);
		mktape(tm, tape);
		tm->s = tm->list;
		run(tm);

		if (((num = ones(tm)) > max)
		|| (num == max && tm->step < min)) {
			max = num;
			min = tm->step;
			printf("new best: %ld in %ld steps\n", max, min);
			/* FIXME: save the machine in bbN.tm */
			prtm(tm);
		}
	} while (nextm(tm));
}

int
main(int argc, char** argv)
{
	int c;
	struct tm *tm;
	ssize_t len = 0;
	size_t size = 0;
	char *line = NULL;
	const char *e = NULL;

	while ((c = getopt(argc, argv, "B:b:qs:S:tT")) != -1) switch (c) {
		case 'B':
			nbusy = strtonum(optarg, 1, 10, &e);
			if (e)
				errx(1, "%s is %s", optarg, e);
			break;
		case 'b':
			if (!isprint(blank = *optarg)) {
				warnx("'%c' is not a valid blank", blank);
				return -1;
			}
			break;
		case 'q':
			qflag = 1;
			break;
		case 's':
			steps = strtonum(optarg, 0, 1 << 16, &e);
			if (e)
				errx(1, "%s is %s", optarg, e);
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

	if (nbusy) {
		bb(nbusy);
		return 0;
	}

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
				/* no tape; do not run. */
				break;
			default:
				run(tm);
				break;
		}
		reset(tm);
	}

	free(line);
	return 0;
}
