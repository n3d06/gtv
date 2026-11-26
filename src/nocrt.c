/* Thay cho CRT: trình biên dịch có thể tự sinh lời gọi memset/memcpy. */
#include <string.h>
#include <intrin.h>

#pragma function(memset)
void *memset(void *dst, int c, size_t n)
{
    __stosb((unsigned char *)dst, (unsigned char)c, n);
    return dst;
}

#pragma function(memcpy)
void *memcpy(void *dst, const void *src, size_t n)
{
    __movsb((unsigned char *)dst, (const unsigned char *)src, n);
    return dst;
}
