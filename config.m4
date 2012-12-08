dnl $Id$
dnl config.m4 for extension memprof

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(memprof, for memprof support,
dnl Make sure that the comment is aligned:
dnl [  --with-memprof             Include memprof support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(memprof, whether to enable memprof support,
 [  --enable-memprof           Enable memprof support])

if test "$PHP_MEMPROF" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-memprof -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/memprof.h"  # you most likely want to change this
  dnl if test -r $PHP_MEMPROF/$SEARCH_FOR; then # path given as parameter
  dnl   MEMPROF_DIR=$PHP_MEMPROF
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for memprof files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       MEMPROF_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$MEMPROF_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the memprof distribution])
  dnl fi

  dnl # --with-memprof -> add include path
  dnl PHP_ADD_INCLUDE($MEMPROF_DIR/include)

  dnl # --with-memprof -> check for lib and symbol presence
  dnl LIBNAME=memprof # you may want to change this
  dnl LIBSYMBOL=memprof # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $MEMPROF_DIR/lib, MEMPROF_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_MEMPROFLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong memprof lib version or lib not found])
  dnl ],[
  dnl   -L$MEMPROF_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(MEMPROF_SHARED_LIBADD)

  PHP_NEW_EXTENSION(memprof, memprof.c util.c, $ext_shared)
fi
