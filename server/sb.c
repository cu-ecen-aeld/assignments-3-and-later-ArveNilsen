#include "sb.h"

int sb_init(StringBuilder *sb, size_t initial_cap, size_t max_cap) {
	if (initial_cap == 0) initial_cap = 4096;
	if (initial_cap > max_cap) initial_cap = max_cap;

	sb->str = malloc(initial_cap);
	if (!sb->str)
		return -1;

	sb->cap = initial_cap;
	sb->len = 0;
	return 0;
}

/* Idempotent free */
void sb_free(StringBuilder *sb) {
	if (!sb->str) return;
	free(sb->str);
	sb->str = NULL;
	sb->cap = 0;
	sb->len = 0;
}

int sb_reserve(StringBuilder *sb, size_t need, size_t max_cap) {
	if (!sb) { errno = EINVAL; return -1; }

	if (need > max_cap) {
		errno = EOVERFLOW; /* caller should drop-until '\n' */
		return -1;
	}

	if (sb->cap >= need) return 0;

	size_t new_cap = sb->cap;
	const size_t MIN_CAP = 4096;
	const size_t FACTOR = 2;

	if (new_cap < MIN_CAP) new_cap = MIN_CAP;

	/* Safe growth to above need */
	while (new_cap < need) {
		if (new_cap > SIZE_MAX / FACTOR) {
			new_cap = need;
			break;
		}

		new_cap *= FACTOR;
	}

	if (new_cap > max_cap) new_cap = max_cap;

	void *tmp = realloc(sb->str, new_cap);
	if (!tmp) { errno = ENOMEM; return -1; }

	sb->str = tmp;
	sb->cap = new_cap;
	return 0;
}

void sb_clear(StringBuilder *sb) {
	sb->len = 0;
}

size_t sb_len(const StringBuilder *sb) {
	return sb->len;
}

size_t sb_cap(const StringBuilder *sb) {
	return sb->cap;
}

const char *sb_data(const StringBuilder *sb) {
	return sb->str;
}

