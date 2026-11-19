#pragma once
// FAILURE/FAILURE_V de libdebug.ranchu (goldfish): registra dónde falló y devuelve el valor.
#include <log/log.h>
#ifndef FAILURE_DEBUG_PREFIX
#define FAILURE_DEBUG_PREFIX "camara"
#endif
#define FAILURE(X) (ALOGE("%s:%s:%d failure", FAILURE_DEBUG_PREFIX, __func__, __LINE__), (X))
#define FAILURE_V(X, FMT, ...) \
    (ALOGE("%s:%s:%d failure: " FMT, FAILURE_DEBUG_PREFIX, __func__, __LINE__, __VA_ARGS__), (X))
