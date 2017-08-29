#ifndef HTABLE_H
#define HTABLE_H
#include "CPack.h"

struct htable* htable_new(void);
void htable_free(struct htable *tbl);
int htable_get(struct htable *tbl, const char *key, const size_t key_len, KEY *value);
int htable_set(struct htable *tbl, const char *key, const size_t key_len, const KEY value);
int htable_del(struct htable *tbl, const char *key, const size_t key_len);
#endif
