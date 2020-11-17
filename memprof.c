/*
  +----------------------------------------------------------------------+
  | Memprof                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) 2012-2013 Arnaud Le Blanc                              |
  +----------------------------------------------------------------------+
  | Redistribution and use in source and binary forms, with or without   |
  | modification, are permitted provided that the conditions mentioned   |
  | in the accompanying LICENSE file are met.                            |
  +----------------------------------------------------------------------+
  | Author: Arnaud Le Blanc <arnaud.lb@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "SAPI.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_memprof.h"
#include "zend_extensions.h"
#include "zend_exceptions.h"
#include <stdint.h>
#include <sys/queue.h>
#include "util.h"
#include <Judy.h>
#if MEMPROF_DEBUG
#	undef NDEBUG
#endif
#include <assert.h>

#define MEMPROF_ENV_PROFILE "MEMPROF_PROFILE"
#define MEMPROF_FLAG_NATIVE "native"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#ifdef ZTS
	/* TODO: support ZTS builds (without malloc hooks) */
#	error "ZTS build not supported (yet)"
#endif

#if MEMPROF_CONFIGURE_VERSION != 3
#	error Please rebuild configure (run phpize and reconfigure)
#endif

#if HAVE_MALLOC_HOOKS
#	include <malloc.h>

#	if MEMPROF_DEBUG
#		define MALLOC_HOOK_CHECK_NOT_OWN() \
			if (__malloc_hook == malloc_hook) { \
				fprintf(stderr, "__malloc_hook == malloc_hook (set at %s:%d)", malloc_hook_file, malloc_hook_line); \
				abort(); \
			} \

#		define MALLOC_HOOK_CHECK_OWN() \
			if (__malloc_hook != malloc_hook) { \
				fprintf(stderr, "__malloc_hook != malloc_hook"); \
				abort(); \
			} \

#		define MALLOC_HOOK_SET_FILE_LINE() do { \
			malloc_hook_file = __FILE__; \
			malloc_hook_line = __LINE__; \
		} while (0)

#	else /* MEMPROF_DEBUG */
#		define MALLOC_HOOK_CHECK_NOT_OWN()
#		define MALLOC_HOOK_CHECK_OWN()
#		define MALLOC_HOOK_SET_FILE_LINE()
#	endif /* MEMPROF_DEBUG */

#	define WITHOUT_MALLOC_HOOKS \
		do { \
			int ___malloc_hook_restored = 0; \
			if (__malloc_hook == malloc_hook) { \
				MALLOC_HOOK_RESTORE_OLD(); \
				___malloc_hook_restored = 1; \
			} \
			do \

#	define END_WITHOUT_MALLOC_HOOKS \
			while (0); \
			if (___malloc_hook_restored) { \
				MALLOC_HOOK_SAVE_OLD(); \
				MALLOC_HOOK_SET_OWN(); \
			} \
		} while (0)

#	define MALLOC_HOOK_RESTORE_OLD() \
		/* Restore all old hooks */ \
		MALLOC_HOOK_CHECK_OWN(); \
		__malloc_hook = old_malloc_hook; \
		__free_hook = old_free_hook; \
		__realloc_hook = old_realloc_hook; \
		__memalign_hook = old_memalign_hook; \

#	define MALLOC_HOOK_SAVE_OLD() \
		/* Save underlying hooks */ \
		MALLOC_HOOK_CHECK_NOT_OWN(); \
		old_malloc_hook = __malloc_hook; \
		old_free_hook = __free_hook; \
		old_realloc_hook = __realloc_hook; \
		old_memalign_hook = __memalign_hook; \

#	define MALLOC_HOOK_SET_OWN() \
		/* Restore our own hooks */ \
		__malloc_hook = malloc_hook; \
		__free_hook = free_hook; \
		__realloc_hook = realloc_hook; \
		__memalign_hook = memalign_hook; \
		MALLOC_HOOK_SET_FILE_LINE();

#else /* HAVE_MALLOC_HOOKS */

#	warning No support for malloc hooks, this build will not track persistent allocations

#	define MALLOC_HOOK_CHECK_NOT_OWN()
#	define MALLOC_HOOK_IS_SET() (__malloc_hook == malloc_hook)
#	define MALLOC_HOOK_RESTORE_OLD()
#	define MALLOC_HOOK_SAVE_OLD()
#	define MALLOC_HOOK_SET_OWN()
#	define WITHOUT_MALLOC_HOOKS \
		do { \
			do
#	define END_WITHOUT_MALLOC_HOOKS \
			while (0); \
		} while (0);

#endif /* HAVE_MALLOC_HOOKS */

typedef LIST_HEAD(_alloc_list_head, _alloc) alloc_list_head;

/* a call frame */
typedef struct _frame {
	char * name;
	size_t name_len;
	struct _frame * prev;
	size_t calls;
	HashTable next_cache;
	alloc_list_head allocs;
} frame;

/* an allocated block's infos */
typedef struct _alloc {
#if MEMPROF_DEBUG
	size_t canary_a;
#endif
	LIST_ENTRY(_alloc) list;
	size_t size;
#if MEMPROF_DEBUG
	size_t canary_b;
#endif
} alloc;

typedef union _alloc_bucket_item {
	alloc alloc;
	union _alloc_bucket_item * next_free;
} alloc_bucket_item;

typedef struct _alloc_buckets {
	size_t growsize;
	size_t nbuckets;
	alloc_bucket_item * next_free;
	alloc_bucket_item ** buckets;
} alloc_buckets;

#if HAVE_MALLOC_HOOKS
static void * malloc_hook(size_t size, const void *caller);
static void * realloc_hook(void *ptr, size_t size, const void *caller);
static void free_hook(void *ptr, const void *caller);
static void * memalign_hook(size_t alignment, size_t size, const void *caller);
#if MEMPROF_DEBUG
static int malloc_hook_line = 0;
static const char * malloc_hook_file = NULL;
#endif

static void * (*old_malloc_hook) (size_t size, const void *caller) = NULL;
static void * (*old_realloc_hook) (void *ptr, size_t size, const void *caller) = NULL;
static void (*old_free_hook) (void *ptr, const void *caller) = NULL;
static void * (*old_memalign_hook) (size_t alignment, size_t size, const void *caller) = NULL;
#endif /* HAVE_MALLOC_HOOKS */

static void (*old_zend_execute)(zend_execute_data *execute_data);
static void (*old_zend_execute_internal)(zend_execute_data *execute_data_ptr, zval *return_value);
#define zend_execute_fn zend_execute_ex

static PHP_INI_MH((*origOnChangeMemoryLimit)) = NULL;

#define MEMPROF_ENABLED			(1<<0)
#define MEMPROF_ENABLED_NATIVE  (1<<1)

static int memprof_enabled = 0;
static int memprof_dumped = 0;
static int track_mallocs = 0;

static frame root_frame;
static frame * current_frame;
static alloc_list_head * current_alloc_list;
static alloc_buckets current_alloc_buckets;

static Pvoid_t allocs_set = (Pvoid_t) NULL;

static const size_t zend_mm_heap_size = 4096;
static zend_mm_heap * zheap = NULL;
static zend_mm_heap * orig_zheap = NULL;

#define ALLOC_INIT(alloc, size) alloc_init(alloc, size)

#define ALLOC_LIST_INSERT_HEAD(head, elem) alloc_list_insert_head(head, elem)
#define ALLOC_LIST_REMOVE(elem) alloc_list_remove(elem)

static inline void alloc_init(alloc * alloc, size_t size) {
	alloc->size = size;
	alloc->list.le_next = NULL;
	alloc->list.le_prev = NULL;
#if MEMPROF_DEBUG
	alloc->canary_a = alloc->canary_b = size ^ 0x5a5a5a5a;
#endif
}

static void alloc_list_insert_head(alloc_list_head * head, alloc * elem) {
	LIST_INSERT_HEAD(head, elem, list);
}

static void list_remove(alloc * elm) {
	LIST_REMOVE(elm, list);
}

static void alloc_list_remove(alloc * elem) {
	if (elem->list.le_prev || elem->list.le_next) {
		list_remove(elem);
		elem->list.le_next = NULL;
		elem->list.le_prev = NULL;
	}
}

#if MEMPROF_DEBUG

static void alloc_check_single(alloc * alloc, const char * function, int line) {
	if (alloc->canary_a != (alloc->size ^ 0x5a5a5a5a) || alloc->canary_a != alloc->canary_b) {
		fprintf(stderr, "canary mismatch for %p at %s:%d\n", alloc, function, line);
		abort();
	}
}

static void alloc_check(alloc * alloc, const char * function, int line) {
	/* fprintf(stderr, "checking %p at %s:%d\n", alloc, function, line); */
	alloc_check_single(alloc, function, line);
	/*
	for (alloc = current_alloc_list->lh_first; alloc; alloc = alloc->list.le_next) {
		alloc_check_single(alloc, function, line);
	}
	*/
}

#	define ALLOC_CHECK(alloc) alloc_check(alloc, __FUNCTION__, __LINE__);
#else /* MEMPROF_DEBUG */
#	define ALLOC_CHECK(alloc)
#endif /* MEMPROF_DEBUG */

static void alloc_buckets_destroy(alloc_buckets * buckets)
{
	size_t i;

	for (i = 0; i < buckets->nbuckets; ++i) {
		free(buckets->buckets[i]);
	}

#if MEMPROF_DEBUG
	memset(buckets->buckets, 0x5a, buckets->nbuckets * sizeof(buckets->buckets[0]));
#endif
	free(buckets->buckets);

#if MEMPROF_DEBUG
	memset(buckets, 0x5a, sizeof(*buckets));
#endif
}

static void alloc_buckets_grow(alloc_buckets * buckets)
{
	size_t i;
	alloc_bucket_item * bucket;

	buckets->nbuckets++;
	buckets->buckets = realloc(buckets->buckets, sizeof(*buckets->buckets)*buckets->nbuckets);

	buckets->growsize <<= 1;
	bucket = malloc(sizeof(*bucket)*buckets->growsize);
	buckets->buckets[buckets->nbuckets-1] = bucket;

	for (i = 1; i < buckets->growsize; ++i) {
		bucket[i-1].next_free = &bucket[i];
	}
	bucket[buckets->growsize-1].next_free = buckets->next_free;
	buckets->next_free = &bucket[0];
}

static void alloc_buckets_init(alloc_buckets * buckets)
{
	buckets->growsize = 128;
	buckets->nbuckets = 0;
	buckets->buckets = NULL;
	buckets->next_free = NULL;
	alloc_buckets_grow(buckets);
}

static alloc * alloc_buckets_alloc(alloc_buckets * buckets, size_t size)
{
	alloc_bucket_item * item = buckets->next_free;

	if (item == NULL) {
		alloc_buckets_grow(buckets);
		item = buckets->next_free;
	}

	buckets->next_free = item->next_free;

	ALLOC_INIT(&item->alloc, size);

	return &item->alloc;
}

static void alloc_buckets_free(alloc_buckets * buckets, alloc * a)
{
	alloc_bucket_item * item;
	item = (alloc_bucket_item*) a;
	item->next_free = buckets->next_free;
	buckets->next_free = item;
}

static void destroy_frame(frame * f)
{
	alloc * a;

#if MEMPROF_DEBUG
	memset(f->name, 0x5a, f->name_len);
#endif
	free(f->name);

	while (f->allocs.lh_first) {
		a = f->allocs.lh_first;
		ALLOC_CHECK(a);
		ALLOC_LIST_REMOVE(a);
	}

	zend_hash_destroy(&f->next_cache);

#if MEMPROF_DEBUG
	memset(f, 0x5a, sizeof(*f));
#endif
}

/* HashTable destructor */
static void frame_dtor(zval * pDest)
{
	frame * f = Z_PTR_P(pDest);
	destroy_frame(f);
	free(f);
}

static void init_frame(frame * f, frame * prev, char * name, size_t name_len)
{
	zend_hash_init(&f->next_cache, 0, NULL, frame_dtor, 0);
	f->name = malloc(name_len+1);
	memcpy(f->name, name, name_len+1);
	f->name_len = name_len;
	f->calls = 0;
	f->prev = prev;
	LIST_INIT(&f->allocs);
}

static frame * new_frame(frame * prev, char * name, size_t name_len)
{
	frame * f = malloc(sizeof(*f));
	init_frame(f, prev, name, name_len);
	return f;
}

static frame * get_or_create_frame(zend_execute_data * current_execute_data, frame * prev)
{
	frame * f;

	char name[256];
	size_t name_len;

	name_len = get_function_name(current_execute_data, name, sizeof(name));

	f = zend_hash_str_find_ptr(&prev->next_cache, name, name_len);
	if (f == NULL) {
		f = new_frame(prev, name, name_len);
		zend_hash_str_add_ptr(&prev->next_cache, name, name_len, f);
	}

	return f;
}

static size_t frame_alloc_size(const frame * f)
{
	size_t size = 0;
	alloc * alloc;

	LIST_FOREACH(alloc, &f->allocs, list) {
		size += alloc->size;
	}

	return size;
}

static int frame_stack_depth(const frame * f)
{
	const frame * prev;
	int depth = 0;

	for (prev = f; prev != &root_frame; prev = prev->prev) {
		depth ++;
	}

	return depth;
}

static void mark_own_alloc(Pvoid_t * set, void * ptr, alloc * a)
{
	Word_t * p;
	JLI(p, *set, (Word_t)ptr);
	*p = (Word_t) a;
}

static void unmark_own_alloc(Pvoid_t * set, void * ptr)
{
	int ret;

	MALLOC_HOOK_CHECK_NOT_OWN();

	JLD(ret, *set, (Word_t)ptr);
}

alloc * is_own_alloc(Pvoid_t * set, void * ptr)
{
	Word_t * p;

	MALLOC_HOOK_CHECK_NOT_OWN();

	JLG(p, *set, (Word_t)ptr);
	if (p != NULL) {
		return (alloc*) *p;
	} else {
		return 0;
	}
}

#if HAVE_MALLOC_HOOKS

static void * malloc_hook(size_t size, const void *caller)
{
	void *result;

	WITHOUT_MALLOC_HOOKS {

		result = malloc(size);
		if (result != NULL) {
			alloc * a = alloc_buckets_alloc(&current_alloc_buckets, size);
			if (track_mallocs) {
				ALLOC_LIST_INSERT_HEAD(current_alloc_list, a);
			}
			mark_own_alloc(&allocs_set, result, a);
			assert(is_own_alloc(&allocs_set, result));
		}

	} END_WITHOUT_MALLOC_HOOKS;

	return result;
}

static void * realloc_hook(void *ptr, size_t size, const void *caller)
{
	void *result;
	alloc *a;

	WITHOUT_MALLOC_HOOKS {

		if (ptr != NULL && !(a = is_own_alloc(&allocs_set, ptr))) {
			result = realloc(ptr, size);
		} else {
			/* ptr may be freed by realloc, so we must remove it from list now */
			if (ptr != NULL) {
				ALLOC_CHECK(a);
				ALLOC_LIST_REMOVE(a);
				unmark_own_alloc(&allocs_set, ptr);
				alloc_buckets_free(&current_alloc_buckets, a);
			}

			result = realloc(ptr, size);
			if (result != NULL) {
				/* succeeded; add result */
				a = alloc_buckets_alloc(&current_alloc_buckets, size);
				if (track_mallocs) {
					ALLOC_LIST_INSERT_HEAD(current_alloc_list, a);
				}
				mark_own_alloc(&allocs_set, result, a);
			} else if (ptr != NULL) {
				/* failed, re-add ptr, since it hasn't been freed */
				a = alloc_buckets_alloc(&current_alloc_buckets, size);
				if (track_mallocs) {
					ALLOC_LIST_INSERT_HEAD(current_alloc_list, a);
				}
				mark_own_alloc(&allocs_set, ptr, a);
			}
		}

	} END_WITHOUT_MALLOC_HOOKS;

	return result;
}

static void free_hook(void *ptr, const void *caller)
{
	WITHOUT_MALLOC_HOOKS {

		if (ptr != NULL) {
			alloc * a;
			if ((a = is_own_alloc(&allocs_set, ptr))) {
				ALLOC_CHECK(a);
				ALLOC_LIST_REMOVE(a);
				free(ptr);
				unmark_own_alloc(&allocs_set, ptr);
				alloc_buckets_free(&current_alloc_buckets, a);
			} else {
				free(ptr);
			}
		}

	} END_WITHOUT_MALLOC_HOOKS;
}

static void * memalign_hook(size_t alignment, size_t size, const void *caller)
{
	void * result;

	WITHOUT_MALLOC_HOOKS {

		result = memalign(alignment, size);
		if (result != NULL) {
			alloc *a = alloc_buckets_alloc(&current_alloc_buckets, size);
			if (track_mallocs) {
				ALLOC_LIST_INSERT_HEAD(current_alloc_list, a);
			}
			mark_own_alloc(&allocs_set, result, a);
		}	

	} END_WITHOUT_MALLOC_HOOKS;

	return result;
}
#endif /* HAVE_MALLOC_HOOKS */

#define WITHOUT_MALLOC_TRACKING do { \
	int ___old_track_mallocs = track_mallocs; \
	track_mallocs = 0; \
	do

#define END_WITHOUT_MALLOC_TRACKING \
	while (0); \
	track_mallocs = ___old_track_mallocs; \
} while (0)

static void * zend_malloc_handler(size_t size)
{
	void *result;

	assert(memprof_enabled);

	WITHOUT_MALLOC_HOOKS {

		result = zend_mm_alloc(orig_zheap, size);
		if (result != NULL) {
			alloc * a = alloc_buckets_alloc(&current_alloc_buckets, size);
			if (track_mallocs) {
				ALLOC_LIST_INSERT_HEAD(current_alloc_list, a);
			}
			mark_own_alloc(&allocs_set, result, a);
			assert(is_own_alloc(&allocs_set, result));
		}

	} END_WITHOUT_MALLOC_HOOKS;

	return result;
}

static void zend_free_handler(void * ptr)
{
	assert(memprof_enabled);

	WITHOUT_MALLOC_HOOKS {

		if (ptr != NULL) {
			alloc * a;
			if ((a = is_own_alloc(&allocs_set, ptr))) {
				ALLOC_CHECK(a);
				ALLOC_LIST_REMOVE(a);
				zend_mm_free(orig_zheap, ptr);
				unmark_own_alloc(&allocs_set, ptr);
				alloc_buckets_free(&current_alloc_buckets, a);
			} else {
				zend_mm_free(orig_zheap, ptr);
			}
		}

	} END_WITHOUT_MALLOC_HOOKS;
}

static void * zend_realloc_handler(void * ptr, size_t size)
{
	void *result;
	alloc *a;

	assert(memprof_enabled);

	WITHOUT_MALLOC_HOOKS {

		if (ptr != NULL && !(a = is_own_alloc(&allocs_set, ptr))) {
			result = zend_mm_realloc(orig_zheap, ptr, size);
		} else {
			/* ptr may be freed by realloc, so we must remove it from list now */
			if (ptr != NULL) {
				ALLOC_CHECK(a);
				ALLOC_LIST_REMOVE(a);
				unmark_own_alloc(&allocs_set, ptr);
				alloc_buckets_free(&current_alloc_buckets, a);
			}

			result = zend_mm_realloc(orig_zheap, ptr, size);
			if (result != NULL) {
				/* succeeded; add result */
				a = alloc_buckets_alloc(&current_alloc_buckets, size);
				if (track_mallocs) {
					ALLOC_LIST_INSERT_HEAD(current_alloc_list, a);
				}
				mark_own_alloc(&allocs_set, result, a);
			} else if (ptr != NULL) {
				/* failed, re-add ptr, since it hasn't been freed */
				a = alloc_buckets_alloc(&current_alloc_buckets, size);
				if (track_mallocs) {
					ALLOC_LIST_INSERT_HEAD(current_alloc_list, a);
				}
				mark_own_alloc(&allocs_set, ptr, a);
			}
		}

	} END_WITHOUT_MALLOC_HOOKS;

	return result;
}

static void memprof_zend_execute(zend_execute_data *execute_data)
{
	WITHOUT_MALLOC_TRACKING {

		current_frame = get_or_create_frame(execute_data, current_frame);
		current_frame->calls++;
		current_alloc_list = &current_frame->allocs;

	} END_WITHOUT_MALLOC_TRACKING;

	old_zend_execute(execute_data);

	if (memprof_enabled) {
		current_frame = current_frame->prev;
		current_alloc_list = &current_frame->allocs;
	}
}

static void memprof_zend_execute_internal(zend_execute_data *execute_data_ptr, zval *return_value)
{
	int ignore = 0;

	if (&execute_data_ptr->func->internal_function == &zend_pass_function) {
		ignore = 1;
	} else if (execute_data_ptr->func->common.function_name) {
		zend_string * name = execute_data_ptr->func->common.function_name;
		if (ZSTR_LEN(name) == sizeof("call_user_func")-1
				&& 0 == memcmp(name, "call_user_func", sizeof("call_user_func")))
		{
			ignore = 1;
		} else if (ZSTR_LEN(name) == sizeof("call_user_func_array")-1
				&& 0 == memcmp(name, "call_user_func_array", sizeof("call_user_func_array")))
		{
			ignore = 1;
		}
	}

	WITHOUT_MALLOC_TRACKING {

		if (!ignore) {
			current_frame = get_or_create_frame(execute_data_ptr, current_frame);
			current_frame->calls++;
			current_alloc_list = &current_frame->allocs;
		}

	} END_WITHOUT_MALLOC_TRACKING;

	if (!old_zend_execute_internal) {
		execute_internal(execute_data_ptr, return_value);
	} else {
		old_zend_execute_internal(execute_data_ptr, return_value);
	}

	if (!ignore && memprof_enabled) {
		current_frame = current_frame->prev;
		current_alloc_list = &current_frame->allocs;
	}
}

static PHP_INI_MH(OnChangeMemoryLimit)
{
	int ret;

	if (!origOnChangeMemoryLimit) {
		return FAILURE;
	}

	ret = origOnChangeMemoryLimit(entry, new_value, mh_arg1, mh_arg2, mh_arg3, stage);

	if (ret != SUCCESS) {
		return ret;
	}

	if (memprof_enabled && orig_zheap) {
		zend_mm_set_heap(orig_zheap);
		zend_set_memory_limit(PG(memory_limit));
		zend_mm_set_heap(zheap);
	}

	return SUCCESS;
}

static void memprof_enable(int flags)
{
	assert(flags & MEMPROF_ENABLED);

	alloc_buckets_init(&current_alloc_buckets);

	init_frame(&root_frame, &root_frame, "root", sizeof("root")-1);
	root_frame.calls = 1;

	current_frame = &root_frame;
	current_alloc_list = &root_frame.allocs;

	if (flags & MEMPROF_ENABLED_NATIVE) {
		MALLOC_HOOK_SAVE_OLD();
		MALLOC_HOOK_SET_OWN();
	}

	memprof_enabled = flags;
	memprof_dumped = 0;

	if (is_zend_mm()) {
		/* There is no way to completely free a zend_mm_heap with custom
		 * handlers, so we have to allocate it ourselves. We don't know the
		 * actual size of a _zend_mm_heap struct, but this should be enough. */
		zheap = malloc(zend_mm_heap_size);
		memset(zheap, 0, zend_mm_heap_size);
		zend_mm_set_custom_handlers(zheap, zend_malloc_handler, zend_free_handler, zend_realloc_handler);
		orig_zheap = zend_mm_set_heap(zheap);
	} else {
		zheap = NULL;
		orig_zheap = NULL;
	}

	old_zend_execute = zend_execute_fn;
	old_zend_execute_internal = zend_execute_internal;
	zend_execute_fn = memprof_zend_execute;
	zend_execute_internal = memprof_zend_execute_internal;

	track_mallocs = 1;
}

static void memprof_disable()
{
	track_mallocs = 0;

	zend_execute_fn = old_zend_execute;
	zend_execute_internal = old_zend_execute_internal;

	if (zheap) {
		zend_mm_set_heap(orig_zheap);
		free(zheap);
	}

	if (memprof_enabled & MEMPROF_ENABLED_NATIVE) {
		MALLOC_HOOK_RESTORE_OLD();
	}

	memprof_enabled = 0;

	destroy_frame(&root_frame);

	alloc_buckets_destroy(&current_alloc_buckets);

	JudyLFreeArray(&allocs_set, PJE0);
	allocs_set = (Pvoid_t) NULL;

	if (!memprof_dumped) {
		// Calling this during RSHUTDOWN breaks zend_deactivate_modules(), which
		// causes corruption of global state.
		// zend_error(E_WARNING, "Memprof profiling was enabled, but no profile was dumped. Did you forget to call one of memprof_dump_callgrind(), memprof_dump_pprof(), or memprof_dump_array() ?");
	}
}

static void disable_opcache()
{
	zend_string *key = zend_string_init(ZEND_STRL("opcache.enable"), 0);
	zend_alter_ini_entry_chars_ex(
		key,
		"0",
		1,
		ZEND_INI_USER,
		ZEND_INI_STAGE_ACTIVATE,
		0
	);
	zend_string_release(key);
}

static zend_string* read_env_get_post(char *name, size_t len)
{
	zval *value;

	char *env = sapi_getenv(name, len);
	if (env != NULL) {
		zend_string *str = zend_string_init(env, strlen(env), 0);
		efree(env);
		return str;
	}

	env = getenv(name);
	if (env != NULL) {
		return zend_string_init(env, strlen(env), 0);
	}

	if (Z_ARR(PG(http_globals)[TRACK_VARS_GET]) != NULL) {
		value = zend_hash_str_find(Z_ARR(PG(http_globals)[TRACK_VARS_GET]), name, len);
		if (value != NULL) {
			convert_to_string_ex(value);
			zend_string_addref(Z_STR_P(value));
			return Z_STR_P(value);
		}
	}

	if (Z_ARR(PG(http_globals)[TRACK_VARS_POST]) != NULL) {
		value = zend_hash_str_find(Z_ARR(PG(http_globals)[TRACK_VARS_POST]), name, len);
		if (value != NULL) {
			convert_to_string_ex(value);
			zend_string_addref(Z_STR_P(value));
			return Z_STR_P(value);
		}
	}

	return NULL;
}

static int should_autostart()
{
	char *saveptr;
	const char *delim = ",";
	char *flag;

	zend_string *value = read_env_get_post(MEMPROF_ENV_PROFILE, strlen(MEMPROF_ENV_PROFILE));
	if (value == NULL) {
		return 0;
	}

	int autostart = ZSTR_LEN(value) > 0 ? MEMPROF_ENABLED : 0;

	for (flag = strtok_r(ZSTR_VAL(value), delim, &saveptr); flag != NULL; flag = strtok_r(NULL, delim, &saveptr)) {
		if (HAVE_MALLOC_HOOKS && strcmp(MEMPROF_FLAG_NATIVE, flag) == 0) {
			autostart |= MEMPROF_ENABLED_NATIVE;
		}
	}

	zend_string_release(value);

	return autostart;
}

ZEND_DLEXPORT int memprof_zend_startup(zend_extension *extension)
{
	return zend_startup_module(&memprof_module_entry);
}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API    ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
	MEMPROF_NAME,
	PHP_MEMPROF_VERSION,
	"Arnaud Le Blanc",
	"https://github.com/arnaud-lb/php-memory-profiler",
	"Copyright (c) 2013",
	memprof_zend_startup,
	NULL,
	NULL,           /* activate_func_t */
	NULL,           /* deactivate_func_t */
	NULL,           /* message_handler_func_t */
	NULL,           /* op_array_handler_func_t */
	NULL,           /* statement_handler_func_t */
	NULL,           /* fcall_begin_handler_func_t */
	NULL,           /* fcall_end_handler_func_t */
	NULL,           /* op_array_ctor_func_t */
	NULL,           /* op_array_dtor_func_t */
	STANDARD_ZEND_EXTENSION_PROPERTIES
};

ZEND_BEGIN_ARG_INFO_EX(arginfo_memprof_dump_callgrind, 0, 0, 1)
	ZEND_ARG_INFO(0, handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_memprof_dump_pprof, 0, 0, 1)
	ZEND_ARG_INFO(0, handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_memprof_memory_get_usage, 0, 0, 0)
	ZEND_ARG_INFO(0, real)
ZEND_END_ARG_INFO()

/* {{{ memprof_functions[]
 */
const zend_function_entry memprof_functions[] = {
	PHP_FE(memprof_enabled, arginfo_void)
	PHP_FE(memprof_enabled_flags, arginfo_void)
	PHP_FE(memprof_enable, arginfo_void)
	PHP_FE(memprof_disable, arginfo_void)
	PHP_FE(memprof_dump_array, arginfo_void)
	PHP_FE(memprof_dump_callgrind, arginfo_memprof_dump_callgrind)
	PHP_FE(memprof_dump_pprof, arginfo_memprof_dump_pprof)
	PHP_FE_END    /* Must be the last line in memprof_functions[] */
};
/* }}} */

/* {{{ memprof_functions_overrides[]
 */
const zend_function_entry memprof_function_overrides[] = {
	PHP_FALIAS(memory_get_peak_usage, memprof_memory_get_peak_usage, arginfo_memprof_memory_get_usage)
	PHP_FALIAS(memory_get_usage, memprof_memory_get_usage, arginfo_memprof_memory_get_usage)
	PHP_FE_END    /* Must be the last line in memprof_function_overrides[] */
};
/* }}} */

/* {{{ memprof_module_entry
 */
zend_module_entry memprof_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	MEMPROF_NAME,
	memprof_functions,
	PHP_MINIT(memprof),
	PHP_MSHUTDOWN(memprof),
	PHP_RINIT(memprof),
	PHP_RSHUTDOWN(memprof),
	PHP_MINFO(memprof),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_MEMPROF_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMPROF
ZEND_GET_MODULE(memprof)
#endif

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(memprof)
{
	zend_ini_entry * entry;
	const zend_function_entry * fentry;

	entry = zend_hash_str_find_ptr(EG(ini_directives), "memory_limit", sizeof("memory_limit")-1);

	if (entry == NULL) {
		zend_error(E_CORE_ERROR, "memory_limit ini entry not found");
		return FAILURE;
	}

	origOnChangeMemoryLimit = entry->on_modify;
	entry->on_modify = OnChangeMemoryLimit;

	for (fentry = memprof_function_overrides; fentry->fname; fentry++) {
		size_t name_len = strlen(fentry->fname);
		zend_internal_function * orig = zend_hash_str_find_ptr(CG(function_table), fentry->fname, name_len);
		if (orig != NULL && orig->type == ZEND_INTERNAL_FUNCTION) {
			orig->handler = fentry->handler;
		} else {
			zend_error(E_WARNING, "memprof: Could not override %s(), return value from this function may be be accurate.", fentry->fname);
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(memprof)
{
	if (origOnChangeMemoryLimit) {
		zend_ini_entry * entry;

		entry = zend_hash_str_find_ptr(EG(ini_directives), "memory_limit", sizeof("memory_limit")-1);

		if (entry != NULL) {
			entry->on_modify = origOnChangeMemoryLimit;
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(memprof)
{
	int flags = should_autostart();

	if (flags) {
		disable_opcache();
		memprof_enable(flags);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(memprof)
{
	if (memprof_enabled) {
		memprof_disable();
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(memprof)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "memprof support", "enabled");
	php_info_print_table_header(2, "memprof version", PHP_MEMPROF_VERSION);
	php_info_print_table_header(2, "memprof native malloc support", HAVE_MALLOC_HOOKS ? "yes" : "no");
#if MEMPROF_DEBUG
	php_info_print_table_header(2, "debug build", "Yes");
#endif
	php_info_print_table_end();
}
/* }}} */

static void frame_inclusive_cost(frame * f, size_t * inclusive_size, size_t * inclusive_count)
{
	size_t size = 0;
	size_t count = 0;
	alloc * alloc;
	HashPosition pos;
	zval * znext;

	LIST_FOREACH(alloc, &f->allocs, list) {
		size += alloc->size;
		count ++;
	}

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		size_t call_size;
		size_t call_count;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		frame_inclusive_cost(next, &call_size, &call_count);

		size += call_size;
		count += call_count;

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	*inclusive_size = size;
	*inclusive_count = count;
}

static void dump_frame_array(zval * dest, frame * f)
{
	HashPosition pos;
	zval * znext;
	zval * zframe = dest;
	zval zcalled_functions;
	alloc * alloc;
	size_t alloc_size = 0;
	size_t alloc_count = 0;
	size_t inclusive_size;
	size_t inclusive_count;

	array_init(zframe);

	LIST_FOREACH(alloc, &f->allocs, list) {
		alloc_size += alloc->size;
		alloc_count ++;
	}

	add_assoc_long_ex(zframe, ZEND_STRL("memory_size"), alloc_size);
	add_assoc_long_ex(zframe, ZEND_STRL("blocks_count"), alloc_count);

	frame_inclusive_cost(f, &inclusive_size, &inclusive_count);
	add_assoc_long_ex(zframe, ZEND_STRL("memory_size_inclusive"), inclusive_size);
	add_assoc_long_ex(zframe, ZEND_STRL("blocks_count_inclusive"), inclusive_count);

	add_assoc_long_ex(zframe, ZEND_STRL("calls"), f->calls);

	array_init(&zcalled_functions);

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {

		zend_string * str_key;
		zend_ulong num_key;
		zval zcalled_function;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		dump_frame_array(&zcalled_function, next);
		add_assoc_zval_ex(&zcalled_functions, ZSTR_VAL(str_key), ZSTR_LEN(str_key), &zcalled_function);

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	add_assoc_zval_ex(zframe, ZEND_STRL("called_functions"), &zcalled_functions);
}

static void dump_frame_callgrind(php_stream * stream, frame * f, char * fname, size_t * inclusive_size, size_t * inclusive_count)
{
	size_t size = 0;
	size_t count = 0;
	size_t self_size = 0;
	size_t self_count = 0;
	alloc * alloc;
	HashPosition pos;
	zval * znext;

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		size_t call_size;
		size_t call_count;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		dump_frame_callgrind(stream, next, ZSTR_VAL(str_key), &call_size, &call_count);

		size += call_size;
		count += call_count;

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	stream_printf(stream, "fl=/todo.php\n");
	stream_printf(stream, "fn=%s\n", fname);

	LIST_FOREACH(alloc, &f->allocs, list) {
		self_size += alloc->size;
		self_count ++;
	}
	size += self_size;
	count += self_count;

	stream_printf(stream, "1 %zu %zu\n", self_size, self_count);

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		size_t call_size;
		size_t call_count;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		frame_inclusive_cost(next, &call_size, &call_count);

		stream_printf(stream, "cfl=/todo.php\n");
		stream_printf(stream, "cfn=%s\n", ZSTR_VAL(str_key));
		stream_printf(stream, "calls=%zu 1\n", next->calls);
		stream_printf(stream, "1 %zu %zu\n", call_size, call_count);

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	stream_printf(stream, "\n");

	if (inclusive_size) {
		*inclusive_size = size;
	}
	if (inclusive_count) {
		*inclusive_count = count;
	}
}

static void dump_frames_pprof(php_stream * stream, HashTable * symbols, frame * f)
{
	HashPosition pos;
	frame * prev;
	zval * znext;
	size_t size = frame_alloc_size(f);
	size_t stack_depth = frame_stack_depth(f);

	if (0 < size) {
		stream_write_word(stream, size);
		stream_write_word(stream, stack_depth);

		for (prev = f; prev != &root_frame; prev = prev->prev) {
			zend_uintptr_t symaddr;
			symaddr = (zend_uintptr_t) zend_hash_str_find_ptr(symbols, prev->name, prev->name_len);
			if (symaddr == 0) {
				/* shouldn't happen */
				zend_error(E_CORE_ERROR, "symbol address not found");
				return;
			}
			stream_write_word(stream, symaddr);
		}
	}

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		dump_frames_pprof(stream, symbols, next);

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}
}

static void dump_frames_pprof_symbols(php_stream * stream, HashTable * symbols, frame * f)
{
	HashPosition pos;
	zval * znext;
	zend_uintptr_t symaddr;

	if (!zend_hash_str_exists(symbols, f->name, f->name_len)) {
		/* addr only has to be unique */
		symaddr = (symbols->nNumOfElements+1)<<3;
		zend_hash_str_add_ptr(symbols, f->name, f->name_len, (void*) symaddr);
		stream_printf(stream, "0x%0*x %s\n", sizeof(symaddr)*2, symaddr, f->name);
	}

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		dump_frames_pprof_symbols(stream, symbols, next);

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}
}

/* {{{ proto void memprof_dump_array(void)
   Returns current memory usage as an array */
PHP_FUNCTION(memprof_dump_array)
{
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE) {
		return;
	}

	if (!memprof_enabled) {
		zend_throw_exception(EG(exception_class), "memprof_dump_array(): memprof is not enabled", 0);
		return;
	}

	WITHOUT_MALLOC_TRACKING {

		dump_frame_array(return_value, &root_frame);

	} END_WITHOUT_MALLOC_TRACKING;

	memprof_dumped = 1;
}
/* }}} */

/* {{{ proto void memprof_dump_callgrind(resource handle)
   Dumps current memory usage in callgrind format to stream $handle */
PHP_FUNCTION(memprof_dump_callgrind)
{
	zval *arg1;
	php_stream *stream;
	size_t total_size;
	size_t total_count;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &arg1) == FAILURE) {
		return;
	}

	if (!memprof_enabled) {
		zend_throw_exception(EG(exception_class), "memprof_dump_callgrind(): memprof is not enabled", 0);
		return;
	}

	php_stream_from_zval(stream, arg1);

	WITHOUT_MALLOC_TRACKING {

		stream_printf(stream, "version: 1\n");
		stream_printf(stream, "cmd: unknown\n");
		stream_printf(stream, "positions: line\n");
		stream_printf(stream, "events: MemorySize BlocksCount\n");
		stream_printf(stream, "\n");

		dump_frame_callgrind(stream, &root_frame, "root", &total_size, &total_count);

		stream_printf(stream, "total: %zu %zu\n", total_size, total_count);

	} END_WITHOUT_MALLOC_TRACKING;

	memprof_dumped = 1;
}
/* }}} */

/* {{{ proto void memprof_dump_pprof(resource handle)
   Dumps current memory usage in pprof heapprofile format to stream $handle */
PHP_FUNCTION(memprof_dump_pprof)
{
	zval *arg1;
	php_stream *stream;
	HashTable symbols;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &arg1) == FAILURE) {
		return;
	}

	if (!memprof_enabled) {
		zend_throw_exception(EG(exception_class), "memprof_dump_pprof(): memprof is not enabled", 0);
		return;
	}

	php_stream_from_zval(stream, arg1);

	WITHOUT_MALLOC_TRACKING {

		zend_hash_init(&symbols, 8, NULL, NULL, 0);

		/* symbols */

		stream_printf(stream, "--- symbol\n");
		stream_printf(stream, "binary=todo.php\n");
		dump_frames_pprof_symbols(stream, &symbols, &root_frame);
		stream_printf(stream, "---\n");
		stream_printf(stream, "--- profile\n");

		/* profile header */

		/* header count */
		stream_write_word(stream, 0);
		/* header words after this one */
		stream_write_word(stream, 3);
		/* format version */
		stream_write_word(stream, 0);
		/* sampling period */
		stream_write_word(stream, 0);
		/* unused padding */
		stream_write_word(stream, 0);

		dump_frames_pprof(stream, &symbols, &root_frame);

		zend_hash_destroy(&symbols);

	} END_WITHOUT_MALLOC_TRACKING;

	memprof_dumped = 1;
}
/* }}} */

/* {{{ proto void memprof_memory_get_usage(bool real)
   Returns the current memory usage */
PHP_FUNCTION(memprof_memory_get_usage)
{
	zend_bool real = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &real) == FAILURE) {
		return;
	}

	if (memprof_enabled && orig_zheap) {
		zend_mm_set_heap(orig_zheap);
		RETVAL_LONG(zend_memory_usage(real));
		zend_mm_set_heap(zheap);
	} else {
		RETVAL_LONG(zend_memory_usage(real));
	}
}
/* }}} */

/* {{{ proto void memprof_memory_get_peak_usage(bool real)
   Returns the peak memory usage */
PHP_FUNCTION(memprof_memory_get_peak_usage)
{
	zend_bool real = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &real) == FAILURE) {
		return;
	}

	if (memprof_enabled && orig_zheap) {
		zend_mm_set_heap(orig_zheap);
		RETVAL_LONG(zend_memory_peak_usage(real));
		zend_mm_set_heap(zheap);
	} else {
		RETVAL_LONG(zend_memory_peak_usage(real));
	}
}
/* }}} */

/* {{{ proto bool memprof_enable()
   Enables memprof */
PHP_FUNCTION(memprof_enable)
{
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE) {
		return;
	}

	if (memprof_enabled) {
		zend_throw_exception(EG(exception_class), "memprof_enable(): memprof is already enabled", 0);
		return;
	}

	zend_error(E_WARNING, "Calling memprof_enable() manually may not work as expected because of PHP optimizations. Prefer using MEMPROF_PROFILE=1 as environment variable, GET, or POST");

	memprof_enable(MEMPROF_ENABLED);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memprof_disable()
   Disables memprof */
PHP_FUNCTION(memprof_disable)
{
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE) {
		return;
	}

	if (!memprof_enabled) {
		zend_throw_exception(EG(exception_class), "memprof_disable(): memprof is not enabled", 0);
		return;
	}

	memprof_disable();

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool memprof_enabled()
   Returns whether memprof is enabled */
PHP_FUNCTION(memprof_enabled)
{
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE) {
		return;
	}

	RETURN_BOOL(memprof_enabled & MEMPROF_ENABLED);
}
/* }}} */

/* {{{ proto array memprof_enabled_flags()
   Returns whether memprof is enabled */
PHP_FUNCTION(memprof_enabled_flags)
{
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE) {
		return;
	}

	array_init(return_value);
	add_assoc_bool(return_value, "enabled", (memprof_enabled & MEMPROF_ENABLED) != 0);
	add_assoc_bool(return_value, "native", (memprof_enabled & MEMPROF_ENABLED_NATIVE) != 0);
}
/* }}} */
