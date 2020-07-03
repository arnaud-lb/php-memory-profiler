# Memprof internals

Memprof tracks the amount of memory allocated by PHP functions. It does so by hooking
in the memory allocator in order to keep track of which function allocated a block.

## Data structures

Three main structures allow to track the memory of the program:

- `frame`: Allocation informations about one particular call path.
- `allocs_set`: As the name doesn't suggest, this is a map from memory addresses to informations about one particular allocation.
- `alloc`: A struct representing one particular allocation. It's a linked list item.

### Call frames

We want to know how much memory was allocated by each call stacks. This structure represents exactly this information:

    /* a call frame */
    typedef struct _frame {
        char * name;            /* function name */
        size_t name_len;
        struct _frame * prev;   /* frame of the calling function (parent) */
        size_t calls;           /* number of times it has been called */
        HashTable next_cache;   /* called functions (children) */
        alloc_list_head allocs; /* head of the allocations list */
    } frame;

Every time a function is called, we create a new `frame` struct, unless one already exists for this call path (we use `next_cache` to find existing `frame` structs).

The frame contains a linked list of allocation informations (a list of `alloc` structs). This is a doubly linked list to make it possible to remove one item without knowing the related frame or list head.

### Allocation map

We want to forget about allocated blocks when they are freed. The `allocs_set` struct is a map from memory addresses to `alloc` structs. It makes it easy and fast to find the `alloc` struct related to a memory address being freed, and to remove this struct from its doubly linked list (whose list head or related framne we don't need to know).

### Allocating allocation information

In order to reduce the overhead of creating `alloc` structs to the minimum, we use a memory pool to allocate and recycle them.

## Hooking in ``malloc``

The GNU C library makes this very simple by [allowing it
explicitly](https://www.gnu.org/software/libc/manual/html_node/Hooks-for-Malloc.html#Hooks-for-Malloc).

These hooks are not thread safe, and are deprecated for this reason, however
this doesn't have an impact on non-ZTS PHP builds (unless an extension or library creates threads internally).

Hooking in ``malloc`` calls allow to track persistent allocations made by
PHP (i.e. allocations that should persist after script execution and are
not done through the Zend Memory Manager).

## Hooking in the Zend Memory Manager

PHP uses the Zend Memory Manager for pretty much every single allocation,
so hooking in the ZMM allows to track pretty much every allocation.

Hooking is done by creating an alternate ZMM heap that proxies calls to the
original ZMM heap.

## Hooking in function calls

Hooking in function calls is done by proxying Zend Engine's ``zend_execute_fn``
and ``zend_execute_internal`` functions. The former is called when the
engine executes a userland php function, and the later is called for internal
php functions.

These are function pointers meant to be changed by extensions. However, when
they are not changed, PHP might emit opcodes that don't use them. So we have
to change them during request init, before any file is compiled.
