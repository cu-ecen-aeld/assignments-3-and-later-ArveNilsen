#ifndef __SB_H__
#define __SB_H__

#include <stdlib.h> /* malloc, realloc */
#include <stdint.h> /* SIZE_MAX */
#include <errno.h>  /* errno */

/*
 * Invariants for all API calls:
 * Success - sb->str != NULL
 *	   - 0  <= sb->len <= sb->cap
 *	   - NOT NULL-TERMINATED
 * Failure - all fields are UNCHANGED
 * 	   - errno set to ENOMEM or EOVERFLOW accordingly
 */
typedef struct StringBuilder {
	char *str;
	size_t cap;
	size_t len;
} StringBuilder;

int sb_init(StringBuilder *sb, size_t initial_cap, size_t max_cap);
void sb_free(StringBuilder *sb);
int sb_reserve(StringBuilder *sb, size_t need, size_t max_cap);
void sb_clear(StringBuilder *sb);
size_t sb_len(const StringBuilder *sb);
size_t sb_cap(const StringBuilder *sb);
const char *sb_data(const StringBuilder *sb);

#endif 
