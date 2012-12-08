
#include "php.h"
#include <stdarg.h>

void stream_printf(php_stream * stream, const char * format, ...)
{
    char * buf;
    va_list ap;
    int len;

    va_start(ap, format);
    len = vspprintf(&buf, 0, format, ap);
    va_end(ap);

    php_stream_write(stream, buf, len);
}

size_t get_function_name(zend_execute_data * execute_data, char * buf, size_t buf_size)
{
    const char * function_name = NULL;
    const char * call_type = NULL;
    char * free_class_name = NULL;
    const char * class_name = NULL;
    size_t len;

    if (!execute_data) {
        return snprintf(buf, buf_size, "main");
    }

    function_name = get_active_function_name();

    if (function_name) {
        class_name = get_active_class_name(&call_type);
    } else {
        if (!execute_data->opline || execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
            function_name = "unknown";
        } else {
            switch (execute_data->opline->extended_value) {
                case ZEND_EVAL:
                    function_name = "eval";
                    break;
                case ZEND_INCLUDE:
                    function_name = "include";
                    break;
                case ZEND_REQUIRE:
                    function_name = "require";
                    break;
                case ZEND_INCLUDE_ONCE:
                    function_name = "include_once";
                    break;
                case ZEND_REQUIRE_ONCE:
                    function_name = "require_once";
                    break;
                default:
                    function_name = "unknown"; 
                    break;
            }
        }

        call_type = NULL;
    }

    len = snprintf(buf, buf_size, "%s%s%s", class_name != NULL ? class_name : "", call_type != NULL ? call_type : "", function_name != NULL ? function_name : "");

    if (free_class_name) {
        efree((char*)free_class_name);
    }

    return len >= buf_size ? buf_size-1 : len;
}

