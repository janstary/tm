#include <stdlib.h>
#include <stdint.h>

#define ISMOVE(_m) ((_m == 'L') || (_m == 'N') || (_m == 'R'))

struct inst {
	char		r;
	char		w;
	char		m;
	struct state*	t;
	struct inst*	next;
};

struct state {
	char		s;
	struct inst*	inst;
	struct inst*	last;
	struct state*	next;
};

struct tm {
	struct state*	s;
	struct state*	list;
	struct state*	last;
	size_t		tlen;
	char*		tape;
	char*		head;
	uint64_t	step;
};
