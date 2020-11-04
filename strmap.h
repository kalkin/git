#ifndef STRMAP_H
#define STRMAP_H

#include "hashmap.h"

struct mempool;
struct strmap {
	struct hashmap map;
	struct mem_pool *pool;
	unsigned int strdup_strings:1;
};

struct strmap_entry {
	struct hashmap_entry ent;
	const char *key;
	void *value;
	char keydata[FLEX_ARRAY]; /* if strdup_strings=1, key == &keydata[0] */
};

int cmp_strmap_entry(const void *hashmap_cmp_fn_data,
		     const struct hashmap_entry *entry1,
		     const struct hashmap_entry *entry2,
		     const void *keydata);

#define STRMAP_INIT { \
			.map = HASHMAP_INIT(cmp_strmap_entry, NULL),  \
			.strdup_strings = 1,                          \
		    }
#define STRINTMAP_INIT { \
			.map.map = HASHMAP_INIT(cmp_strmap_entry, NULL),  \
			.map.strdup_strings = 1,                          \
			.default_value = 0,                               \
		    }
#define STRSET_INIT { \
			.map.map = HASHMAP_INIT(cmp_strmap_entry, NULL),  \
			.map.strdup_strings = 1,                          \
		    }

/*
 * Initialize the members of the strmap.  Any keys added to the strmap will
 * be strdup'ed with their memory managed by the strmap.
 */
void strmap_init(struct strmap *map);

/*
 * Same as strmap_init, but for those who want to control the memory management
 * carefully instead of using the default of strdup_strings=1 and pool=NULL.
 */
void strmap_init_with_options(struct strmap *map,
			      struct mem_pool *pool,
			      int strdup_strings);

/*
 * Remove all entries from the map, releasing any allocated resources.
 */
void strmap_clear(struct strmap *map, int free_values);

/*
 * Similar to strmap_clear() but leaves map->map->table allocated and
 * pre-sized so that subsequent uses won't need as many rehashings.
 */
void strmap_partial_clear(struct strmap *map, int free_values);

/*
 * Insert "str" into the map, pointing to "data".
 *
 * If an entry for "str" already exists, its data pointer is overwritten, and
 * the original data pointer returned. Otherwise, returns NULL.
 */
void *strmap_put(struct strmap *map, const char *str, void *data);

/*
 * Return the strmap_entry mapped by "str", or NULL if there is not such
 * an item in map.
 */
struct strmap_entry *strmap_get_entry(struct strmap *map, const char *str);

/*
 * Return the data pointer mapped by "str", or NULL if the entry does not
 * exist.
 */
void *strmap_get(struct strmap *map, const char *str);

/*
 * Return non-zero iff "str" is present in the map. This differs from
 * strmap_get() in that it can distinguish entries with a NULL data pointer.
 */
int strmap_contains(struct strmap *map, const char *str);

/*
 * Remove the given entry from the strmap.  If the string isn't in the
 * strmap, the map is not altered.
 */
void strmap_remove(struct strmap *map, const char *str, int free_value);

/*
 * Return how many entries the strmap has.
 */
static inline unsigned int strmap_get_size(struct strmap *map)
{
	return hashmap_get_size(&map->map);
}

/*
 * Return whether the strmap is empty.
 */
static inline int strmap_empty(struct strmap *map)
{
	return strmap_get_size(map) == 0;
}

/*
 * iterate through @map using @iter, @var is a pointer to a type strmap_entry
 */
#define strmap_for_each_entry(mystrmap, iter, var)	\
	for (var = hashmap_iter_first_entry_offset(&(mystrmap)->map, iter, 0); \
		var; \
		var = hashmap_iter_next_entry_offset(iter, 0))


/*
 * strintmap:
 *    A map of string -> int, typecasting the void* of strmap to an int.
 *
 * Primary differences:
 *    1) Since the void* value is just an int in disguise, there is no value
 *       to free.  (Thus one fewer argument to strintmap_clear)
 *    2) strintmap_get() returns an int; it also requires an extra parameter to
 *       be specified so it knows what value to return if the underlying strmap
 *       has not key matching the given string.
 *    3) No strmap_put() equivalent; strintmap_set() and strintmap_incr()
 *       instead.
 */

struct strintmap {
	struct strmap map;
	int default_value;
};

#define strintmap_for_each_entry(mystrmap, iter, var)	\
	strmap_for_each_entry(&(mystrmap)->map, iter, var)

static inline void strintmap_init(struct strintmap *map, int default_value)
{
	strmap_init(&map->map);
	map->default_value = default_value;
}

static inline void strintmap_init_with_options(struct strintmap *map,
					       int default_value,
					       struct mem_pool *pool,
					       int strdup_strings)
{
	strmap_init_with_options(&map->map, pool, strdup_strings);
	map->default_value = default_value;
}

static inline void strintmap_clear(struct strintmap *map)
{
	strmap_clear(&map->map, 0);
}

static inline void strintmap_partial_clear(struct strintmap *map)
{
	strmap_partial_clear(&map->map, 0);
}

static inline int strintmap_contains(struct strintmap *map, const char *str)
{
	return strmap_contains(&map->map, str);
}

static inline void strintmap_remove(struct strintmap *map, const char *str)
{
	return strmap_remove(&map->map, str, 0);
}

static inline int strintmap_empty(struct strintmap *map)
{
	return strmap_empty(&map->map);
}

static inline unsigned int strintmap_get_size(struct strintmap *map)
{
	return strmap_get_size(&map->map);
}

/*
 * Returns the value for str in the map.  If str isn't found in the map,
 * the map's default_value is returned.
 */
static inline int strintmap_get(struct strintmap *map, const char *str)
{
	struct strmap_entry *result = strmap_get_entry(&map->map, str);
	if (!result)
		return map->default_value;
	return (intptr_t)result->value;
}

static inline void strintmap_set(struct strintmap *map, const char *str,
				 intptr_t v)
{
	strmap_put(&map->map, str, (void *)v);
}

/*
 * Increment the value for str by amt.  If str isn't in the map, add it and
 * set its value to default_value + amt.
 */
void strintmap_incr(struct strintmap *map, const char *str, intptr_t amt);

/*
 * strset:
 *    A set of strings.
 *
 * Primary differences with strmap:
 *    1) The value is always NULL, and ignored.  As there is no value to free,
 *       there is one fewer argument to strset_clear
 *    2) No strset_get() because there is no value.
 *    3) No strset_put(); use strset_add() instead.
 */

struct strset {
	struct strmap map;
};

#define strset_for_each_entry(mystrset, iter, var)	\
	strmap_for_each_entry(&(mystrset)->map, iter, var)

static inline void strset_init(struct strset *set)
{
	strmap_init(&set->map);
}

static inline void strset_init_with_options(struct strset *set,
					    struct mem_pool *pool,
					    int strdup_strings)
{
	strmap_init_with_options(&set->map, pool, strdup_strings);
}

static inline void strset_clear(struct strset *set)
{
	strmap_clear(&set->map, 0);
}

static inline void strset_partial_clear(struct strset *set)
{
	strmap_partial_clear(&set->map, 0);
}

static inline int strset_contains(struct strset *set, const char *str)
{
	return strmap_contains(&set->map, str);
}

static inline void strset_remove(struct strset *set, const char *str)
{
	return strmap_remove(&set->map, str, 0);
}

static inline int strset_empty(struct strset *set)
{
	return strmap_empty(&set->map);
}

static inline unsigned int strset_get_size(struct strset *set)
{
	return strmap_get_size(&set->map);
}

static inline void strset_add(struct strset *set, const char *str)
{
	strmap_put(&set->map, str, NULL);
}

/* Returns 1 if str already in set.  Otherwise adds str to set and returns 0 */
int strset_check_and_add(struct strset *set, const char *str);

#endif /* STRMAP_H */
