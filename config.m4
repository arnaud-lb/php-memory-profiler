dnl $Id$
dnl config.m4 for extension memprof

PHP_ARG_ENABLE(memprof, whether to enable memprof support,
 [  --enable-memprof        Enable memprof support])

PHP_ARG_WITH(judy-dir, for judy lib,
 [  --with-judy-dir         Specify judy dir])

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
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $JUDY_DIR/lib, MEMPROF_SHARED_LIBADD)
    AC_DEFINE(HAVE_JUDYLIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong judy lib version or lib not found])
  ],[
    -L$JUDY_DIR/lib -lJudy
  ])
  dnl
  PHP_SUBST(MEMPROF_SHARED_LIBADD)

  PHP_NEW_EXTENSION(memprof, memprof.c util.c, $ext_shared)
fi
