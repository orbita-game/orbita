/* minimal freestanding string.h for the wasm32 build */
#pragma once
#include <stddef.h>
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* dst, int c, size_t n);
size_t strlen(const char* s);
