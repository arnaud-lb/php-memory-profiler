/*
  +----------------------------------------------------------------------+
  | Memprof                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 Arnaud Le Blanc                              |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Arnaud Le Blanc <arnaud.lb@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_memprof.h"
#include "malloc.h"
#include "zend_extensions.h"
#include <stdint.h>
#include <sys/queue.h>
#include "util.h"

#ifdef ZTS
#error "ZTS build not supported"
#endif

#define MEMPROF_DEBUG 1

typedef LIST_HEAD(_alloc_list_head, _alloc) alloc_list_head;

/* a call frame */
typedef struct _frame {
    struct _frame * prev;
    size_t calls;
    HashTable next_cache;
    alloc_list_head allocs;
} frame;

/* an allocated block header */
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

static void malloc_init_hook();
static void * malloc_hook(size_t size, const void *caller);
static void * realloc_hook(void *ptr, size_t size, const void *caller);
static void free_hook(void *ptr, const void *caller);
static void * memalign_hook(size_t alignment, size_t size, const void *caller);

static void * (*old_malloc_hook) (size_t size, const void *caller) = NULL;
static void * (*old_realloc_hook) (void *ptr, size_t size, const void *caller) = NULL;
static void (*old_free_hook) (void *ptr, const void *caller) = NULL;
static void * (*old_memalign_hook) (size_t alignment, size_t size, const void *caller) = NULL;

static void (*old_zend_execute)(zend_op_array *op_array TSRMLS_DC);
static void (*old_zend_execute_internal)(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC);

void (*__malloc_initialize_hook) (void) = malloc_init_hook;

static int memprof_reserved_offset;
static int memprof_initialized = 0;
static int malloc_init_hook_called = 0;
static int track_mallocs = 0;

static frame default_frame;
static frame * current_frame = &default_frame;
static alloc_list_head * current_alloc_list = &default_frame.allocs;

/* ZEND_DECLARE_MODULE_GLOBALS(memprof) */

/* returns the actual data of an allocated block */
#define ALLOC_DATA(alloc) alloc_data(alloc)

/* returns the block header, given a pointer to the begining of its data */
#define ALLOC_BLOCK(data) alloc_block(data)

/* size of a block header */
#define ALLOC_HEAD_SIZE \
    (ZEND_MM_ALIGNED_SIZE(sizeof(struct _alloc)))

/* actual size of a allocation, given the requested size (i.e. data size + header size and alignments) */
#define ALLOC_SIZE(size) \
    (size > SIZE_MAX - ALLOC_HEAD_SIZE ? 0 : ALLOC_HEAD_SIZE + size)

#define ALLOC_INIT(alloc, size) alloc_init(alloc, size)

#define ALLOC_LIST_INSERT_HEAD(head, elem) alloc_list_insert_head(head, elem)
#define ALLOC_LIST_REMOVE(elem) alloc_list_remove(elem, __FUNCTION__, __LINE__)

static inline void * alloc_data(alloc * a) {
    return (void*)(((uintptr_t)a)+ALLOC_HEAD_SIZE);
}

static inline alloc * alloc_block(void * a) {
    return (alloc*) &((char*)a)[-ALLOC_HEAD_SIZE];
}

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

static void alloc_list_remove(alloc * elem, const char * function, int line) {
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
    for (alloc = current_alloc_list->lh_first; alloc; alloc = alloc->list.le_next) {
        alloc_check_single(alloc, function, line);
    }
}

# define ALLOC_CHECK(alloc) alloc_check(alloc, __FUNCTION__, __LINE__);
#else
# define ALLOC_CHECK(alloc)
#endif

static void destroy_frame(frame * f)
{
    alloc * a;

    while (f->allocs.lh_first) {
        a = f->allocs.lh_first;
        ALLOC_CHECK(a);
        ALLOC_LIST_REMOVE(a);
    }

    zend_hash_destroy(&f->next_cache);
}

/* HashTable destructor */
static void frame_dtor(void * pDest)
{
    frame * f = * (frame **) pDest;
    destroy_frame(f);
    free(f);
}

static void init_frame(frame * f, frame * prev)
{
    zend_hash_init(&f->next_cache, 0, NULL, frame_dtor, 0);
    f->calls = 0;
    f->prev = prev;
    LIST_INIT(&f->allocs);
}

static frame * new_frame(frame * prev)
{
    frame * f = malloc(sizeof(*f));
    init_frame(f, prev);
    return f;
}

static frame * get_or_create_frame(zend_execute_data * current_execute_data, frame * prev)
{
    frame * f;
    frame ** f_pp;

    char name[256];
    size_t name_len;

    name_len = get_function_name(current_execute_data, name, sizeof(name));

    if (SUCCESS == zend_hash_find(&prev->next_cache, name, name_len+1, (void**) &f_pp)) {
        f = *f_pp;
    } else {
        f = new_frame(prev);
        zend_hash_add(&prev->next_cache, name, name_len+1, &f, sizeof(f), NULL);
    }

    return f;
}

#define MALLOC_HOOK_RESTORE_OLD() \
    /* Restore all old hooks */ \
    __malloc_hook = old_malloc_hook; \
    __free_hook = old_free_hook; \
    __realloc_hook = old_realloc_hook; \
    __memalign_hook = old_memalign_hook; \

#define MALLOC_HOOK_SAVE_OLD() \
    /* Save underlying hooks */ \
    old_malloc_hook = __malloc_hook; \
    old_free_hook = __free_hook; \
    old_realloc_hook = __realloc_hook; \
    old_memalign_hook = __memalign_hook; \

#define MALLOC_HOOK_SET_OWN() \
    /* Restore our own hooks */ \
    __malloc_hook = malloc_hook; \
    __free_hook = free_hook; \
    __realloc_hook = realloc_hook; \
    __memalign_hook = memalign_hook; \

#define WITHOUT_MALLOC_TRACKING do { \
    int ___old_track_mallocs = track_mallocs; \
    track_mallocs = 0; \
    do

#define END_WITHOUT_MALLOC_TRACKING \
    while (0); \
    track_mallocs = ___old_track_mallocs; \
} while (0)

static void malloc_init_hook()
{
    malloc_init_hook_called = 1;

    putenv("LD_PRELOAD=");
    putenv("USE_ZEND_ALLOC=0");

    init_frame(&default_frame, NULL);
    default_frame.calls = 1;

    MALLOC_HOOK_SAVE_OLD();
    MALLOC_HOOK_SET_OWN();
}

static void * malloc_hook(size_t size, const void *caller)
{
    void *result;
    size_t block_size;

    MALLOC_HOOK_RESTORE_OLD();

    if (0 == (block_size = ALLOC_SIZE(size))) {
        result = NULL;
    } else {
        result = malloc(block_size);
        if (result != NULL) {
            ALLOC_INIT((alloc*)result, size);
            if (track_mallocs) {
                ALLOC_LIST_INSERT_HEAD(current_alloc_list, (alloc*)result);
            }
            ALLOC_CHECK((alloc*)result);
            result = ALLOC_DATA(result);
        }
    }

    MALLOC_HOOK_SAVE_OLD();
    MALLOC_HOOK_SET_OWN();

    return result;
}

static void * realloc_hook(void *ptr, size_t size, const void *caller)
{
    void *result;
    size_t block_size;

    MALLOC_HOOK_RESTORE_OLD();

    if (0 == (block_size = ALLOC_SIZE(size))) {
        result = NULL;
    } else {
        /* ptr may be freed by realloc, so we must remove it from list now */
        if (ptr != NULL) {
            ALLOC_CHECK(ALLOC_BLOCK(ptr));
            ALLOC_LIST_REMOVE(ALLOC_BLOCK(ptr));
        }
        result = realloc(ptr ? ALLOC_BLOCK(ptr) : NULL, block_size);
        if (result != NULL) {
            if (ptr != NULL) {
                ALLOC_CHECK((alloc*)result);
            }
            /* succeeded; add result */
            ALLOC_INIT((alloc*)result, size);
            if (track_mallocs) {
                ALLOC_LIST_INSERT_HEAD(current_alloc_list, (alloc*)result);
            }
            ALLOC_CHECK((alloc*)result);
            result = ALLOC_DATA(result);
        } else if (ptr != NULL) {
            ALLOC_CHECK(ALLOC_BLOCK(ptr));
            if (track_mallocs) {
                /* failed, re-add ptr, since it hasn't been freed */
                ALLOC_LIST_INSERT_HEAD(current_alloc_list, ALLOC_BLOCK(ptr));
            }
            ALLOC_CHECK(ALLOC_BLOCK(ptr));
        }
    }

    MALLOC_HOOK_SAVE_OLD();
    MALLOC_HOOK_SET_OWN();

    return result;
}

static void free_hook(void *ptr, const void *caller)
{
    MALLOC_HOOK_RESTORE_OLD();

    if (ptr != NULL) {
        ALLOC_CHECK(ALLOC_BLOCK(ptr));
        ALLOC_LIST_REMOVE(ALLOC_BLOCK(ptr));
        memset(ALLOC_BLOCK(ptr), 0, ALLOC_SIZE(ALLOC_BLOCK(ptr)->size));
        free(ALLOC_BLOCK(ptr));
    }

    MALLOC_HOOK_SAVE_OLD();
    MALLOC_HOOK_SET_OWN();
}

static void * memalign_hook(size_t alignment, size_t size, const void *caller)
{
    /* TODO: would require special handling in free and realloc */
    return malloc_hook(size, caller);
}

static void memprof_zend_execute(zend_op_array *op_array TSRMLS_DC)
{
    WITHOUT_MALLOC_TRACKING {

        current_frame = get_or_create_frame(EG(current_execute_data), current_frame);
        current_frame->calls++;
        current_alloc_list = &current_frame->allocs;

    } END_WITHOUT_MALLOC_TRACKING;

    old_zend_execute(op_array TSRMLS_CC);

    current_frame = current_frame->prev;
    current_alloc_list = &current_frame->allocs;
}

static void memprof_zend_execute_internal(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC)
{
    int ignore = 0;

    if (execute_data_ptr->function_state.function->common.function_name) {
        const char * name = execute_data_ptr->function_state.function->common.function_name;
        if (0 == memcmp(name, "call_user_func", sizeof("call_user_func"))
                || 0 == memcmp(name, "call_user_func_array", sizeof("call_user_func_array")))
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
        execute_internal(execute_data_ptr, return_value_used);
    } else {
        old_zend_execute_internal(execute_data_ptr, return_value_used);
    }

    if (!ignore) {
        current_frame = current_frame->prev;
        current_alloc_list = &current_frame->allocs;
    }
}

ZEND_DLEXPORT int memprof_zend_startup(zend_extension *extension)
{
    int ret;

    memprof_initialized = 1;

    ret = zend_startup_module(&memprof_module_entry);

    if (-1 == zend_get_resource_handle(extension)) {
        fprintf(stderr, "zend_get_resource_handle failed");
        abort();
    }
    memprof_reserved_offset = extension->resource_number;

    return ret;
}

ZEND_DLEXPORT void memprof_init_oparray(zend_op_array *op_array)
{   
    TSRMLS_FETCH();
    op_array->reserved[memprof_reserved_offset] = 0;
}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API    ZEND_DLEXPORT
#endif
ZEND_EXTENSION();

ZEND_DLEXPORT zend_extension zend_extension_entry = {
    MEMPROF_NAME,
    MEMPROF_VERSION,
    "Arnaud Le Blanc",
    "https://github.com/arnaud-lb/php-memprof",
    "Copyright (c) 2012",
    memprof_zend_startup,
    NULL,
    NULL,           /* activate_func_t */
    NULL,           /* deactivate_func_t */
    NULL,           /* message_handler_func_t */
    NULL,           /* op_array_handler_func_t */
    NULL,           /* statement_handler_func_t */
    NULL,           /* fcall_begin_handler_func_t */
    NULL,           /* fcall_end_handler_func_t */
    memprof_init_oparray,   /* op_array_ctor_func_t */
    NULL,           /* op_array_dtor_func_t */
    STANDARD_ZEND_EXTENSION_PROPERTIES
};

ZEND_BEGIN_ARG_INFO_EX(arginfo_memprof_dump_callgrind, 0, 0, 1)
    ZEND_ARG_INFO(0, handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_memprof_dump_array, 0, 0, 0)
ZEND_END_ARG_INFO()

/* {{{ memprof_functions[]
 */
const zend_function_entry memprof_functions[] = {
    PHP_FE(memprof_dump_callgrind, arginfo_memprof_dump_callgrind)
    PHP_FE(memprof_dump_array, arginfo_memprof_dump_array)
    PHP_FE_END    /* Must be the last line in memprof_functions[] */
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
    PHP_RINIT(memprof),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(memprof),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(memprof),
#if ZEND_MODULE_API_NO >= 20010901
    MEMPROF_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MEMPROF
ZEND_GET_MODULE(memprof)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("memprof.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_memprof_globals, memprof_globals)
    STD_PHP_INI_ENTRY("memprof.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_memprof_globals, memprof_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_memprof_init_globals
 */
/*
static void php_memprof_init_globals(zend_memprof_globals *memprof_globals)
{
    memset(memprof_globals, 0, sizeof(*memprof_globals));
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(memprof)
{
    /* ZEND_INIT_MODULE_GLOBALS(memprof, php_memprof_init_globals, memprof_globals); */
    /* If you have INI entries, uncomment these lines 
    REGISTER_INI_ENTRIES();
    */

    if (!memprof_initialized) {
        zend_error(E_CORE_ERROR, "memprof must be loaded as a Zend extension (zend_extension=/path/to/memprof.so)");
        return FAILURE;
    }
    if (!malloc_init_hook_called) {
        zend_error(E_CORE_ERROR, "malloc init hook has not been called; make sure to preload memprof (e.g. LD_PRELOAD)");
        return FAILURE;
    }

    old_zend_execute = zend_execute;
    old_zend_execute_internal = zend_execute_internal;
    zend_execute = memprof_zend_execute;
    zend_execute_internal = memprof_zend_execute_internal;

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(memprof)
{
    /* uncomment this line if you have INI entries
    UNREGISTER_INI_ENTRIES();
    */

    zend_execute_internal = old_zend_execute_internal;
    zend_execute = old_zend_execute;

    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(memprof)
{
    track_mallocs = 1;

    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(memprof)
{
    track_mallocs = 0;

    destroy_frame(&default_frame);
    init_frame(&default_frame, NULL);
    default_frame.calls = 1;

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(memprof)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "memprof support", "enabled");
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}
/* }}} */

static void frame_inclusive_cost(frame * f, size_t * inclusive_size, size_t * inclusive_count)
{
    size_t size = 0;
    size_t count = 0;
    alloc * alloc;
    HashPosition pos;
    frame ** next_pp;

    LIST_FOREACH(alloc, &f->allocs, list) {
        size += alloc->size;
        count ++;
    }

    zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
    while (zend_hash_get_current_data_ex(&f->next_cache, (void **)&next_pp, &pos) == SUCCESS) {
        char * str_key;
        uint str_key_len;
        ulong num_key;
        size_t call_size;
        size_t call_count;

        if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &str_key_len, &num_key, 0, &pos)) {
            continue;
        }

        frame_inclusive_cost(*next_pp, &call_size, &call_count);

        size += call_size;
        count += call_count;

        zend_hash_move_forward_ex(&f->next_cache, &pos);
    }

    *inclusive_size = size;
    *inclusive_count = count;
}

static zval * dump_frame_array(frame * f)
{
    zval * z;
    HashPosition pos;
    frame ** next_pp;
    zval * zframe;
    zval * zcalled_functions;
    alloc * alloc;
    size_t alloc_size = 0;
    size_t alloc_count = 0;
    size_t inclusive_size;
    size_t inclusive_count;

    MAKE_STD_ZVAL(z);
    array_init(z);

    MAKE_STD_ZVAL(zframe);
    array_init(zframe);

    LIST_FOREACH(alloc, &f->allocs, list) {
        alloc_size += alloc->size;
        alloc_count ++;
    }

    add_assoc_long_ex(zframe, ZEND_STRS("memory_size"), alloc_size);
    add_assoc_long_ex(zframe, ZEND_STRS("blocks_count"), alloc_count);

    frame_inclusive_cost(f, &inclusive_size, &inclusive_count);
    add_assoc_long_ex(zframe, ZEND_STRS("memory_size_inclusive"), inclusive_size);
    add_assoc_long_ex(zframe, ZEND_STRS("blocks_count_inclusive"), inclusive_count);

    add_assoc_long_ex(zframe, ZEND_STRS("calls"), f->calls);

    MAKE_STD_ZVAL(zcalled_functions);
    array_init(zcalled_functions);

    add_assoc_zval_ex(zframe, ZEND_STRS("called_functions"), zcalled_functions);

    zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
    while (zend_hash_get_current_data_ex(&f->next_cache, (void **)&next_pp, &pos) == SUCCESS) {

        char * str_key;
        uint str_key_len;
        ulong num_key;
        zval * zcalled_function;

        if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &str_key_len, &num_key, 0, &pos)) {
            continue;
        }

        zcalled_function = dump_frame_array(*next_pp);
        add_assoc_zval_ex(zcalled_functions, str_key, str_key_len, zcalled_function);

        zend_hash_move_forward_ex(&f->next_cache, &pos);
    }

    return zframe;
}

static void dump_frame_callgrind(php_stream * stream, frame * f, char * fname, size_t * inclusive_size, size_t * inclusive_count)
{
    size_t size = 0;
    size_t count = 0;
    size_t self_size = 0;
    size_t self_count = 0;
    alloc * alloc;
    HashPosition pos;
    frame ** next_pp;

    zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
    while (zend_hash_get_current_data_ex(&f->next_cache, (void **)&next_pp, &pos) == SUCCESS) {
        char * str_key;
        uint str_key_len;
        ulong num_key;
        size_t call_size;
        size_t call_count;

        if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &str_key_len, &num_key, 0, &pos)) {
            continue;
        }

        dump_frame_callgrind(stream, *next_pp, str_key, &call_size, &call_count);

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
    while (zend_hash_get_current_data_ex(&f->next_cache, (void **)&next_pp, &pos) == SUCCESS) {
        char * str_key;
        uint str_key_len;
        ulong num_key;
        size_t call_size;
        size_t call_count;

        if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &str_key_len, &num_key, 0, &pos)) {
            continue;
        }

        frame_inclusive_cost(*next_pp, &call_size, &call_count);

        stream_printf(stream, "cfl=/todo.php\n");
        stream_printf(stream, "cfn=%s\n", str_key);
        stream_printf(stream, "calls=%zu 1\n", (*next_pp)->calls);
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

/* {{{ proto void memprof_dump_array(void)
   Returns current memory usage as an array */
PHP_FUNCTION(memprof_dump_array)
{
    zval * ret;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
        return;
    }

    WITHOUT_MALLOC_TRACKING {

        ret = dump_frame_array(&default_frame);

    } END_WITHOUT_MALLOC_TRACKING;

    RETURN_ZVAL(ret, 0, 0);
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

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &arg1) == FAILURE) {
        return;
    }

    php_stream_from_zval(stream, &arg1);

    WITHOUT_MALLOC_TRACKING {

        stream_printf(stream, "version: 1\n");
        stream_printf(stream, "cmd: unknown\n");
        stream_printf(stream, "positions: line\n");
        stream_printf(stream, "events: MemorySize BlocksCount\n");
        stream_printf(stream, "\n");

        dump_frame_callgrind(stream, &default_frame, "root", &total_size, &total_count);

        stream_printf(stream, "total: %zu %zu\n", total_size, total_count);

    } END_WITHOUT_MALLOC_TRACKING;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: et sw=4 ts=4 fdm=marker
 * vim<600: et sw=4 ts=4
 */
