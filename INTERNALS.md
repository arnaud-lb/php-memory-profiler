# Memprof internals

Memprof counts memory blocks allocated by PHP functions. It does so by hooking
in memory allocation functions, and linking newly allocated blocks to the
currently called function.

## Call frames

In order to link allocations with function calls, we maintain a tree
of structures representing current and past function calls. We call them
*frames*, and they are defined as follows:

    /* a call frame */
    typedef struct _frame {
        char * name;            /* function name */
        size_t name_len;
        struct _frame * prev;   /* frame of the calling function (parent) */
        size_t calls;           /* number of times it has been called */
        HashTable next_cache;   /* called functions (children) */
        alloc_list_head allocs; /* head of the allocations list */
    } frame;

Every time a function is called, we look in ``next_cache`` to see if that
function has already been called from the current function call path. If
so, we just increment its number of calls. Else, we create a new frame.

A pointer to the frame is stored in the ``current_frame`` variable.

When the function returns, ``current_frame`` is pointed back to the parent of
the current frame.

The result is a tree representing unique function call pathes, and allocation
informations about each function call path.

## Allocation lists

Informations about each allocation is stored in an ``_alloc`` struct,
defined as follows:

    /* an allocated block's infos */
    typedef struct _alloc {
        LIST_ENTRY(_alloc) list;    /* LIST_ENTRY (from sys/queue.h) declares
                                       the pointers required to form a
                                       doubly-linked list */
        size_t size;                /* size of the allocation */
    } alloc;

When an allocation is made, we also allocate an ``_alloc`` struct, and link
it in the current frame's ``alloc`` list.

Now it's very easy to dump memory allocation informations. To find the amount
of memory allocated by some function we just have to sum the sizes from the
allocations list of the corresponding frame. For instance,
``memprof_dump_array()`` does this for every function call path by traversing
the ``_frame`` tree from the root.

You may be wondering why not just increment a *"memory_allocated"* counter
on the ``_frame`` struct; the reason for the ``_alloc`` list is that it enables
taking de-allocations into account, even after the allocating function
has returned:

A pointer to the block is also stored in a global key-value structure
(named ``allocs_set``) mapping memory addresses to ``_alloc`` structs. When
a de-allocation is done, we can find the relevant ``_alloc`` struct, and
remove it from the list it is linked to. Thanks to the doubly linked list,
we don't even have to find the function that allocated it. That being done,
if we output the amount of memory allocated by the function which allocated
this block, the block is not in the list anymore and will not be counted.

## Allocating allocation informations

Allocating the ``_alloc`` structs is made pretty much overhead-less by using
a specialized memory pool. Allocations are fixed size, allowing the pool to
be very simple, and to have close to zero CPU and memory overhead.

The pool basically allocates a large memory area, divides it into blocks the
size of an ``_alloc`` struct, and links every free blocks together. Allocating
an ``_alloc`` struct is just a matter of unlinking the block at the head of
the queue. Freeing it re-links the block.

Initial memprof implementation stored the ``_alloc`` struct at the begining of
allocated memory blocks by allocating a little more memory (e.g. calling the
actual malloc function with ``sizeof(struct _alloc)+size+alignment``, and
returning a safely aligned address pointing past the ``_alloc`` struct). This
worked well, however this made the entire process dependent on memprof,
once memprof had been enabled: every single de-allocation had to go through
memprof hooks, so that the right address was passed to the actuall ``free``
function, else ``free`` would see wrong addresses. This could have been fixed
by moving the ``_alloc`` struct at the end of the blocks, however this also
required to store the size of the block externally, which is basically what
we do now, with more complexity. Current implementation allows to disable or
even unload the memprof extension a anytime.

## Hooking in ``malloc``

The GNU C library makes this very simple by [allowing it
explicitly](https://www.gnu.org/software/libc/manual/html_node/Hooks-for-Malloc.html#Hooks-for-Malloc).

These hooks are not thread safe, and are deprecated for this reason, however
this doesn't have an impact on non-ZTS PHP builds.

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

