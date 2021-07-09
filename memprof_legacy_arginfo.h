/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 2ccb0e0939a9f13919125dd80717932d2737a359 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_memprof_enabled, 0, 0, 0)
ZEND_END_ARG_INFO()

#define arginfo_memprof_enabled_flags arginfo_memprof_enabled

#define arginfo_memprof_enable arginfo_memprof_enabled

#define arginfo_memprof_disable arginfo_memprof_enabled

#define arginfo_memprof_dump_array arginfo_memprof_enabled

ZEND_BEGIN_ARG_INFO_EX(arginfo_memprof_dump_callgrind, 0, 0, 1)
	ZEND_ARG_INFO(0, handle)
ZEND_END_ARG_INFO()

#define arginfo_memprof_dump_pprof arginfo_memprof_dump_callgrind


ZEND_FUNCTION(memprof_enabled);
ZEND_FUNCTION(memprof_enabled_flags);
ZEND_FUNCTION(memprof_enable);
ZEND_FUNCTION(memprof_disable);
ZEND_FUNCTION(memprof_dump_array);
ZEND_FUNCTION(memprof_dump_callgrind);
ZEND_FUNCTION(memprof_dump_pprof);


static const zend_function_entry ext_functions[] = {
	ZEND_FE(memprof_enabled, arginfo_memprof_enabled)
	ZEND_FE(memprof_enabled_flags, arginfo_memprof_enabled_flags)
	ZEND_FE(memprof_enable, arginfo_memprof_enable)
	ZEND_FE(memprof_disable, arginfo_memprof_disable)
	ZEND_FE(memprof_dump_array, arginfo_memprof_dump_array)
	ZEND_FE(memprof_dump_callgrind, arginfo_memprof_dump_callgrind)
	ZEND_FE(memprof_dump_pprof, arginfo_memprof_dump_pprof)
	ZEND_FE_END
};
