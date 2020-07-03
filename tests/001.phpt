--TEST--
Enable/disable
--FILE--
<?php

var_dump(memprof_enabled());

memprof_enable();

var_dump(memprof_enabled());

memprof_disable();

var_dump(memprof_enabled());

--EXPECTF--
bool(false)

Warning: Calling memprof_enable() manually may not work as expected because of PHP optimizations. Prefer using MEMPROF_PROFILE=1 as environment variable, GET, or POST in %s
bool(true)

Warning: Memprof profiling was enabled, but no profile was dumped. Did you forget to call one of memprof_dump_callgrind(), memprof_dump_pprof(), or memprof_dump_array() ? %s
bool(false)
