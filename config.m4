dnl config.m4 for extension memprof

PHP_ARG_ENABLE(memprof, whether to enable memprof support,
 [  --enable-memprof        Enable memprof support])

PHP_ARG_WITH(judy-dir, for judy lib,
 [  --with-judy-dir         Specify judy dir])

AC_ARG_ENABLE(memprof-debug,
[  --enable-memprof-debug   Enable memprof debugging],[
  PHP_MEMPROF_DEBUG=$enableval
],[
  PHP_MEMPROF_DEBUG=no
])

if test "$PHP_MEMPROF" != "no"; then

  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="/include/Judy.h"
  if test -r $PHP_JUDY_DIR/$SEARCH_FOR; then
    JUDY_DIR=$PHP_JUDY_DIR
  else
    AC_MSG_CHECKING([for judy files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        JUDY_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi

  if test -z "$JUDY_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the judy distribution])
  fi

  PHP_ADD_INCLUDE($JUDY_DIR/include)

  LIBNAME=Judy
  LIBSYMBOL=Judy1Set

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $JUDY_DIR/$PHP_LIBDIR, MEMPROF_SHARED_LIBADD)
    AC_DEFINE(HAVE_JUDYLIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong judy lib version or lib not found])
  ],[
    -L$JUDY_DIR/$PHP_LIBDIR -lJudy
  ])
  dnl
  PHP_SUBST(MEMPROF_SHARED_LIBADD)

  AC_MSG_CHECKING(for malloc hooks)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <malloc.h>
    extern void * malloc_hook(size_t size, const void *caller);
    extern void * realloc_hook(void *ptr, size_t size, const void *caller);
    extern void free_hook(void *ptr, const void *caller);
    extern void * memalign_hook(size_t alignment, size_t size, const void *caller);
  ]], [[
    __malloc_hook = malloc_hook;
    __free_hook = free_hook;
    __realloc_hook = realloc_hook;
    __memalign_hook = memalign_hook;
  ]])],[
    AC_DEFINE([HAVE_MALLOC_HOOKS], 1, [Define to 1 if libc supports malloc hooks])
    AC_MSG_RESULT(yes)
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_DEFINE([MEMPROF_CONFIGURE_VERSION], 3, [Define configure version])

  PHP_NEW_EXTENSION(memprof, memprof.c util.c, $ext_shared)
fi

if test "$PHP_MEMPROF_DEBUG" != "no"; then
  AC_DEFINE([MEMPROF_DEBUG], 1, [memprof debug])
else
  AC_DEFINE([MEMPROF_DEBUG], 0, [memprof debug])
fi

