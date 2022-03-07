#include "function.h"

bad_function_call::bad_function_call(const char* x) : runtime_error(x) {};