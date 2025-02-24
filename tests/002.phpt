--TEST--
memprof_dump_array()
--ENV--
MEMPROF_PROFILE=1
--FILE--
<?php

require __DIR__ . '/common.php';

try {
	memprof_dump_array();
} catch ( Throwable $ex ) {
	var_dump(get_class($ex));
}

$someAlloc = eat();

var_dump(memprof_dump_array());

$a = eat();
$b = Eater::eat();

var_dump(memprof_dump_array());

--EXPECTF--
array(6) {
  ["memory_size"]=>
  int(%d)
  ["blocks_count"]=>
  int(%d)
  ["memory_size_inclusive"]=>
  int(%d)
  ["blocks_count_inclusive"]=>
  int(%d)
  ["calls"]=>
  int(1)
  ["called_functions"]=>
  array(1) {
    ["require %s002.php"]=>
    array(6) {
      ["memory_size"]=>
      int(%d)
      ["blocks_count"]=>
      int(10)
      ["memory_size_inclusive"]=>
      int(%d)
      ["blocks_count_inclusive"]=>
      int(11)
      ["calls"]=>
      int(1)
      ["called_functions"]=>
      array(3) {
        ["require %scommon.php"]=>
        array(6) {
          ["memory_size"]=>
          int(0)
          ["blocks_count"]=>
          int(0)
          ["memory_size_inclusive"]=>
          int(0)
          ["blocks_count_inclusive"]=>
          int(0)
          ["calls"]=>
          int(1)
          ["called_functions"]=>
          array(0) {
          }
        }
        ["memprof_dump_array"]=>
        array(6) {
          ["memory_size"]=>
          int(0)
          ["blocks_count"]=>
          int(0)
          ["memory_size_inclusive"]=>
          int(0)
          ["blocks_count_inclusive"]=>
          int(0)
          ["calls"]=>
          int(2)
          ["called_functions"]=>
          array(0) {
          }
        }
        ["eat"]=>
        array(6) {
          ["memory_size"]=>
          int(0)
          ["blocks_count"]=>
          int(0)
          ["memory_size_inclusive"]=>
          int(%d)
          ["blocks_count_inclusive"]=>
          int(1)
          ["calls"]=>
          int(1)
          ["called_functions"]=>
          array(1) {
            ["str_repeat"]=>
            array(6) {
              ["memory_size"]=>
              int(%d)
              ["blocks_count"]=>
              int(1)
              ["memory_size_inclusive"]=>
              int(%d)
              ["blocks_count_inclusive"]=>
              int(1)
              ["calls"]=>
              int(1)
              ["called_functions"]=>
              array(0) {
              }
            }
          }
        }
      }
    }
  }
}
array(6) {
  ["memory_size"]=>
  int(%d)
  ["blocks_count"]=>
  int(%d)
  ["memory_size_inclusive"]=>
  int(%d)
  ["blocks_count_inclusive"]=>
  int(%d)
  ["calls"]=>
  int(1)
  ["called_functions"]=>
  array(1) {
    ["require %s002.php"]=>
    array(6) {
      ["memory_size"]=>
      int(%d)
      ["blocks_count"]=>
      int(10)
      ["memory_size_inclusive"]=>
      int(%d)
      ["blocks_count_inclusive"]=>
      int(14)
      ["calls"]=>
      int(1)
      ["called_functions"]=>
      array(5) {
        ["require %scommon.php"]=>
        array(6) {
          ["memory_size"]=>
          int(0)
          ["blocks_count"]=>
          int(0)
          ["memory_size_inclusive"]=>
          int(0)
          ["blocks_count_inclusive"]=>
          int(0)
          ["calls"]=>
          int(1)
          ["called_functions"]=>
          array(0) {
          }
        }
        ["memprof_dump_array"]=>
        array(6) {
          ["memory_size"]=>
          int(0)
          ["blocks_count"]=>
          int(0)
          ["memory_size_inclusive"]=>
          int(0)
          ["blocks_count_inclusive"]=>
          int(0)
          ["calls"]=>
          int(3)
          ["called_functions"]=>
          array(0) {
          }
        }
        ["eat"]=>
        array(6) {
          ["memory_size"]=>
          int(0)
          ["blocks_count"]=>
          int(0)
          ["memory_size_inclusive"]=>
          int(%d)
          ["blocks_count_inclusive"]=>
          int(2)
          ["calls"]=>
          int(2)
          ["called_functions"]=>
          array(1) {
            ["str_repeat"]=>
            array(6) {
              ["memory_size"]=>
              int(%d)
              ["blocks_count"]=>
              int(2)
              ["memory_size_inclusive"]=>
              int(%d)
              ["blocks_count_inclusive"]=>
              int(2)
              ["calls"]=>
              int(2)
              ["called_functions"]=>
              array(0) {
              }
            }
          }
        }
        ["var_dump"]=>
        array(6) {
          ["memory_size"]=>
          int(25)
          ["blocks_count"]=>
          int(1)
          ["memory_size_inclusive"]=>
          int(25)
          ["blocks_count_inclusive"]=>
          int(1)
          ["calls"]=>
          int(1)
          ["called_functions"]=>
          array(0) {
          }
        }
        ["Eater::eat"]=>
        array(6) {
          ["memory_size"]=>
          int(%d)
          ["blocks_count"]=>
          int(1)
          ["memory_size_inclusive"]=>
          int(%d)
          ["blocks_count_inclusive"]=>
          int(1)
          ["calls"]=>
          int(1)
          ["called_functions"]=>
          array(2) {
            ["eat"]=>
            array(6) {
              ["memory_size"]=>
              int(0)
              ["blocks_count"]=>
              int(0)
              ["memory_size_inclusive"]=>
              int(0)
              ["blocks_count_inclusive"]=>
              int(0)
              ["calls"]=>
              int(1)
              ["called_functions"]=>
              array(1) {
                ["str_repeat"]=>
                array(6) {
                  ["memory_size"]=>
                  int(0)
                  ["blocks_count"]=>
                  int(0)
                  ["memory_size_inclusive"]=>
                  int(0)
                  ["blocks_count_inclusive"]=>
                  int(0)
                  ["calls"]=>
                  int(1)
                  ["called_functions"]=>
                  array(0) {
                  }
                }
              }
            }
            ["str_repeat"]=>
            array(6) {
              ["memory_size"]=>
              int(0)
              ["blocks_count"]=>
              int(0)
              ["memory_size_inclusive"]=>
              int(0)
              ["blocks_count_inclusive"]=>
              int(0)
              ["calls"]=>
              int(1)
              ["called_functions"]=>
              array(0) {
              }
            }
          }
        }
      }
    }
  }
}
