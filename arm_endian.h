// "License": Public Domain
// I, Mathias Panzenböck, place this file hereby into the public domain. Use it at your own risk for whatever you like.
// In case there are jurisdictions that don't support putting things in the public domain you can also consider it to
// be "dual licensed" under the BSD, MIT and Apache licenses, if you want to. This code is trivial anyway. Consider it
// an example on how to get the endian conversion functions on different platforms.
//
// Source: https://gist.github.com/panzi/6856583
//
#ifndef ARM_ENDIAN_H
#define ARM_ENDIAN_H

#include <machine/endian.h>

#if defined(_NEWLIB_VERSION)

/*
 * GNU ARM toolchain, and possibly other bare-metal toolchains
 * built on newlib. Tested with
 * (GNU Tools for ARM Embedded Processors 6-2017-q2-update
 */

#include <machine/endian.h>

#if BYTE_ORDER == LITTLE_ENDIAN

    #define htobe16(x) __bswap16(x)
    #define htole16(x) (x)
    #define be16toh(x) __bswap16(x)
    #define le16toh(x) (x)

    #define htobe32(x) __bswap32(x)
    #define htole32(x) (x)
    #define be32toh(x) __bswap32(x)
    #define le32toh(x) (x)

    #define htobe64(x) __bswap64(x)
    #define htole64(x) (x)
    #define be64toh(x) __bswap64(x)
    #define le64toh(x) (x)

#elif BYTE_ORDER == BIG_ENDIAN

    #define htobe16(x) (x)
    #define htole16(x) __bswap16(x)
    #define be16toh(x) (x)
    #define le16toh(x) __bswap16(x)

    #define htobe32(x) (x)
    #define htole32(x) __bswap32(x)
    #define be32toh(x) (x)
    #define le32toh(x) __bswap32(x)

    #define htobe64(x) (x)
    #define htole64(x) __bswap64(x)
    #define be64toh(x) (x)
    #define le64toh(x) __bswap64(x)

#else
    #error byte order not supported
#endif

#endif /* _NEWLIB_VERSION */
#endif /* ARM_ENDIAN_H */
