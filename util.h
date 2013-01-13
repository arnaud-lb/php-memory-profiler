
void stream_printf(php_stream * stream, const char * format, ...);
void stream_write_word(php_stream * stream, zend_uintptr_t word);

size_t get_function_name(zend_execute_data * execute_data, char * buf, size_t buf_size);

