/*! 
  \file zlib.h interface of the `zlib`

  \mainpage ZLIB - General Purpose Compression Library
  version 1.3.0.1, August xxth, 2023

  Copyright (C) 1995-2023 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly    jloup@gzip.org  
  Mark Adler          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files https://tools.ietf.org/html/rfc1950
  (zlib format), [rfc1951](https://tools.ietf.org/html/rfc1951) (deflate format)
  and [rfc1952](https://tools.ietf.org/html/rfc1952) (gzip format).

  The 'zlib' compression library provides in-memory compression and
  decompression functions, including integrity checks of the uncompressed data.
  This version of the library supports only one compression method (deflation)
  but other algorithms will be added later and will have the same stream
  interface.

  Compression can be done in a single step if the buffers are large enough,
  or can be done by repeated calls of the compression function.  In the latter
  case, the application must provide more input and/or consume the output
  (providing more output space) before each call.

  The compressed data format used by default by the in-memory functions is
  the zlib format, which is a zlib wrapper documented in RFC 1950, wrapped
  around a deflate stream, which is itself documented in RFC 1951.

  The library also supports reading and writing files in gzip (.gz) format
  with an interface similar to that of stdio using the functions that start
  with "gz".  The gzip format is different from the zlib format.  gzip is a
  gzip wrapper, documented in RFC 1952, wrapped around a deflate stream.

  This library can optionally read and write gzip and raw deflate streams in
  memory as well.

  The zlib format was designed to be compact and fast for use in memory
  and on communications channels.  The gzip format was designed for single-
  file compression on file systems, has a larger header than zlib to maintain
  directory information, and uses a different, slower check method than zlib.

  The library does not install any signal handler.  The decoder checks
  the consistency of the compressed data, so the library should never crash
  even in the case of corrupted input.
*/

#ifndef ZLIB_H
#define ZLIB_H

#include "zconf.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \name Version information
@{ */
/// zlib version information string
#define ZLIB_VERSION "1.3.0.f-neacsum"  ///< Version information string
#define ZLIB_VERNUM 0x130f              ///< Complete version number
#define ZLIB_VER_MAJOR 1                ///< Major version number
#define ZLIB_VER_MINOR 3                ///< Minor version number
#define ZLIB_VER_REVISION 0             ///< Revision major number
#define ZLIB_VER_SUBREVISION f          ///< Revision minor number
///@}


/*!
  \name Memory management functions

  The opaque value provided by the application will be passed as the first
  parameter for calls of z_stream::zalloc and z_stream::zfree.  This can be
  useful for custom memory management.  The compression library attaches no
  meaning to the opaque value.
  \ingroup basic
@{
*/
typedef voidpf (*alloc_func)(voidpf opaque, uInt items, uInt size);
typedef void   (*free_func)(voidpf opaque, voidpf address);
///@}

struct internal_state;

/*!
  Compressed stream state information

  The application must update next_in and avail_in when avail_in has dropped
  to zero.  It must update next_out and avail_out when avail_out has dropped
  to zero.  The application must initialize #zalloc, #zfree and #opaque before
  calling the init function.  All other fields are set by the compression
  library and must not be updated by the application.

  The fields #total_in and #total_out can be used for statistics or progress
  reports.  After compression, #total_in holds the total size of the
  uncompressed data and may be saved for use by the decompressor (particularly
  if the decompressor wants to decompress everything in a single step).

  #zalloc must return Z_NULL if there is not enough memory for the object.
  If zlib is used in a multi-threaded application, #zalloc and #zfree must be
  thread safe.  In that case, zlib is thread-safe.  When #zalloc and #zfree are
  Z_NULL on entry to the initialization function, they are set to internal
  routines that use the standard library functions malloc() and free().

  On 16-bit systems, the functions #zalloc and #zfree must be able to allocate
  exactly 65536 bytes, but will not be required to allocate more than this if
  the symbol MAXSEG_64K is defined (see zconf.h).

  \warning On MSDOS, pointers returned by #zalloc for objects of exactly 65536
  bytes *must* have their offset normalized to zero.  The default allocation
  function provided by this library ensures this (see zutil.c).  To reduce
  memory requirements and avoid any allocation of 64K objects, at the expense
  of compression ratio, compile the library with `-DMAX_WBITS=14` (see zconf.h).

  \ingroup basic
*/
typedef struct z_stream_s {
    const Bytef *next_in;   ///< next input byte
    uInt     avail_in;      ///< number of bytes available at next_in
    uLong    total_in;      ///< total number of input bytes read so far

    Bytef    *next_out;     ///< next output byte will go here
    uInt     avail_out;     ///< remaining free space at next_out
    uLong    total_out;     ///< total number of bytes output so far

    const char *msg;        ///< last error message, NULL if no error
    struct internal_state *state; ///< not visible by applications

    alloc_func zalloc;      ///< used to allocate the internal state
    free_func  zfree;       ///< used to free the internal state
    voidpf     opaque;      ///< private data object passed to zalloc and zfree

    int     data_type;      ///< best guess about the data type: binary or text
                            /// for deflate, or the decoding state for inflate
    uLong   adler;          ///< Adler-32 or CRC-32 value of the uncompressed data
    uLong   reserved;       ///< reserved for future use
} z_stream;

typedef z_stream *z_streamp;

/*!
  gzip header information passed to and from zlib routines.  See RFC 1952
  for more details on the meanings of these fields.
*/
typedef struct gz_header_s {
    int     text;       ///< true if compressed data believed to be text
    uLong   time;       ///< modification time
    int     xflags;     ///< extra flags (not used when writing a gzip file)
    int     os;         ///< operating system
    Bytef   *extra;     ///< pointer to extra field or Z_NULL if none
    uInt    extra_len;  ///< extra field length (valid if extra != Z_NULL)
    uInt    extra_max;  ///< space at extra (only when reading header)
    Bytef   *name;      ///< pointer to zero-terminated file name or Z_NULL
    uInt    name_max;   ///< space at name (only when reading header)
    Bytef   *comment;   ///< pointer to zero-terminated comment or Z_NULL
    uInt    comm_max;   ///< space at comment (only when reading header)
    int     hcrc;       ///< true if there was or will be a header crc
    int     done;       /*!< true when done reading gzip header (not used
                             when writing a gzip file) */
} gz_header;

typedef gz_header *gz_headerp;


                        /* constants */

/*!
  \name Allowed flush values
  See deflate() and inflate() for details
@{
*/
#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
#define Z_BLOCK         5
#define Z_TREES         6
///@}

/*! 
 \name Return codes for the compression/decompression functions.
 Negative values are errors, positive values are used for special but normal events.
@{
*/
#define Z_OK            0     ///< Success
#define Z_STREAM_END    1     ///< End of input or output stream
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)   ///< Inconsistent stream state
#define Z_DATA_ERROR   (-3)   ///< Invalid input data
#define Z_MEM_ERROR    (-4)   ///< Out of memeory
#define Z_BUF_ERROR    (-5)   ///< Buffer full error
#define Z_VERSION_ERROR (-6)  ///< Library version mismatch
///@}

/// \name Compression levels
///@{
#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
///@}

/// \name Compression strategy; see deflateInit2() below for details
///@{
#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_RLE                 3
#define Z_FIXED               4
#define Z_DEFAULT_STRATEGY    0
///@}

/// \name Possible values of the data_type field for deflate()
///@{
#define Z_BINARY   0
#define Z_TEXT     1
#define Z_ASCII    Z_TEXT   /* for compatibility with 1.2.2 and earlier */
#define Z_UNKNOWN  2
///@}

/// The deflate compression method (the only one supported in this version)
#define Z_DEFLATED   8

/// for initializing zalloc, zfree, opaque
#define Z_NULL  0 

/// for compatibility with versions < 1.0.2
#define zlib_version zlibVersion()


/*!
  \defgroup basic Basic Functions

@{
*/

const char* ZEXPORT zlibVersion (void);

int ZEXPORT deflate(z_streamp strm, int flush);

int ZEXPORT deflateEnd(z_streamp strm);

int ZEXPORT inflate(z_streamp strm, int flush);

int ZEXPORT inflateEnd(z_streamp strm);
///@}

/*!
  \defgroup adv Advanced Functions
  
  These functions are needed only in some special applications.
@{
*/

int ZEXPORT deflateSetDictionary(z_streamp strm,
                                 const Bytef *dictionary,
                                 uInt  dictLength);

int ZEXPORT deflateGetDictionary(z_streamp strm,
                                 Bytef *dictionary,
                                 uInt  *dictLength);

int ZEXPORT deflateCopy(z_streamp dest,
                        z_streamp source);

int ZEXPORT deflateReset(z_streamp strm);

int ZEXPORT deflateParams(z_streamp strm,
                          int level,
                          int strategy);

int ZEXPORT deflateTune(z_streamp strm,
                        int good_length,
                        int max_lazy,
                        int nice_length,
                        int max_chain);

uLong ZEXPORT deflateBound(z_streamp strm,
                           uLong sourceLen);

int ZEXPORT deflatePending(z_streamp strm,
                           unsigned *pending,
                           int *bits);

int ZEXPORT deflatePrime(z_streamp strm,
                         int bits,
                         int value);

int ZEXPORT deflateSetHeader(z_streamp strm,
                             gz_headerp head);

int ZEXPORT inflateSetDictionary(z_streamp strm,
                                 const Bytef *dictionary,
                                 uInt  dictLength);

int ZEXPORT inflateGetDictionary(z_streamp strm,
                                 Bytef *dictionary,
                                 uInt  *dictLength);

int ZEXPORT inflateSync(z_streamp strm);

ZEXTERN int ZEXPORT inflateCopy(z_streamp dest,
                                z_streamp source);

int ZEXPORT inflateReset(z_streamp strm);

int ZEXPORT inflateReset2(z_streamp strm,
                          int windowBits);

int ZEXPORT inflatePrime(z_streamp strm,
                         int bits,
                         int value);

long ZEXPORT inflateMark(z_streamp strm);

int ZEXPORT inflateGetHeader(z_streamp strm,
                             gz_headerp head);

///  Input function used by inflateBack()
typedef unsigned (*in_func)(void*, const unsigned char* *);

/// Output function used by inflateBack()
typedef int (*out_func)(void*, unsigned char *, unsigned);

int ZEXPORT inflateBack(z_streamp strm,
                        in_func in, void* in_desc,
                        out_func out, void* out_desc);

int ZEXPORT inflateBackEnd(z_streamp strm);

uLong ZEXPORT zlibCompileFlags(void);
///@}

#ifndef Z_SOLO

/*!
  \defgroup util Utility Functions
  
  The following utility functions are implemented on top of the basic
  stream-oriented functions.  To simplify the interface, some default options
  are assumed (compression level and memory usage, standard memory allocation
  functions).  The source code of these utility functions can be modified if
  you need special options.
@{
*/

int ZEXPORT compress(Bytef *dest,   uLongf *destLen,
                     const Bytef *source, uLong sourceLen);

int ZEXPORT compress2(Bytef *dest,   uLongf *destLen,
                              const Bytef *source, uLong sourceLen,
                              int level);

uLong ZEXPORT compressBound(uLong sourceLen);

int ZEXPORT uncompress(Bytef *dest,   uLongf *destLen,
                       const Bytef *source, uLong sourceLen);

int ZEXPORT uncompress2(Bytef *dest,   uLongf *destLen,
                        const Bytef *source, uLong *sourceLen);

///@}

/*!
  \defgroup gzip gzip Functions

  This library supports reading and writing files in gzip (.gz) format with
  an interface similar to that of stdio, using the functions that start with
  "gz".  The gzip format is different from the zlib format.  gzip is a gzip
  wrapper, documented in RFC 1952, wrapped around a deflate stream.
@{
*/

/*!
  Semi-opaque gzip file descriptor structure.
  
  Note that the real internal state is much larger than the exposed structure.
  This abbreviated structure exposes just enough for the gzgetc () macro.The
  user should not mess with these exposed elements, since their names or
  behavior could change in the future, perhaps even capriciously.They can
  only be used by the gzgetc () macro.You have been warned.
*/
struct gzFile_s {
  unsigned have;
  unsigned char* next;
  z_off64_t pos;
}; 

typedef struct gzFile_s *gzFile;

gzFile ZEXPORT gzdopen(int fd, const char *mode);

int ZEXPORT gzbuffer(gzFile file, unsigned size);

int ZEXPORT gzsetparams(gzFile file, int level, int strategy);

int ZEXPORT gzread(gzFile file, voidp buf, unsigned len);

z_size_t ZEXPORT gzfread(voidp buf, z_size_t size, z_size_t nitems,
                                 gzFile file);

int ZEXPORT gzwrite(gzFile file, voidpc buf, unsigned len);

z_size_t ZEXPORT gzfwrite(voidpc buf, z_size_t size,
                                  z_size_t nitems, gzFile file);

int ZEXPORTVA gzprintf(gzFile file, const char *format, ...);

int ZEXPORT gzputs(gzFile file, const char *s);

char* ZEXPORT gzgets(gzFile file, char *buf, int len);

int ZEXPORT gzputc(gzFile file, int c);

int ZEXPORT gzgetc(gzFile file);

int ZEXPORT gzungetc(int c, gzFile file);

int ZEXPORT gzflush(gzFile file, int flush);

int ZEXPORT gzrewind(gzFile file);

int ZEXPORT gzeof(gzFile file);

int ZEXPORT gzdirect(gzFile file);

int ZEXPORT gzclose(gzFile file);

int ZEXPORT gzclose_r(gzFile file);
int ZEXPORT gzclose_w(gzFile file);

const char * ZEXPORT gzerror(gzFile file, int *errnum);

ZEXTERN void ZEXPORT gzclearerr(gzFile file);
///@}

#endif /* !Z_SOLO */

                        /* checksum functions */

/*
     These functions are not related to compression but are exported
   anyway because they might be useful in applications using the compression
   library.
*/

uLong ZEXPORT adler32(uLong adler, const Bytef *buf, uInt len);
uLong ZEXPORT adler32_z(uLong adler, const Bytef *buf,
                        z_size_t len);

uLong ZEXPORT crc32(uLong crc, const Bytef *buf, uInt len);

uLong ZEXPORT crc32_z(uLong crc, const Bytef *buf,
                      z_size_t len);

/*
ZEXTERN uLong ZEXPORT crc32_combine_gen(z_off_t len2);

     Return the operator corresponding to length len2, to be used with
   crc32_combine_op().
*/

ZEXTERN uLong ZEXPORT crc32_combine_op(uLong crc1, uLong crc2, uLong op);


                        /* various hacks, don't look :) */

/* deflateInit and inflateInit are macros to allow checking the zlib version
 * and the compiler's view of z_stream:
 */
ZEXTERN int ZEXPORT deflateInit_(z_streamp strm, int level,
                                 const char *version, int stream_size);


int ZEXPORT inflateInit_(z_streamp strm,
                         const char *version, int stream_size);

int ZEXPORT deflateInit2_(z_streamp strm, int  level, int  method,
                          int windowBits, int memLevel,
                          int strategy, const char *version,
                          int stream_size);

int ZEXPORT inflateInit2_(z_streamp strm, int  windowBits,
                          const char *version, int stream_size);


int ZEXPORT inflateBackInit_(z_streamp strm, int windowBits,
                             unsigned char* window,
                             const char *version,
                             int stream_size);
#ifdef Z_PREFIX_SET
#  define z_deflateInit(strm, level) \
          deflateInit_((strm), (level), ZLIB_VERSION, (int)sizeof(z_stream))

/// Macro wrapper of inflateInit_() allows checking the zlib version and the compiler's view of z_stream
#  define z_inflateInit(strm) \
          inflateInit_((strm), ZLIB_VERSION, (int)sizeof(z_stream))
#  define z_deflateInit2(strm, level, method, windowBits, memLevel, strategy) \
          deflateInit2_((strm),(level),(method),(windowBits),(memLevel),\
                        (strategy), ZLIB_VERSION, (int)sizeof(z_stream))

/// Macro wrapper of inflateInit2_() allows checking the zlib version and the compiler's view of z_stream
#  define z_inflateInit2(strm, windowBits) \
          inflateInit2_((strm), (windowBits), ZLIB_VERSION, \
                        (int)sizeof(z_stream))

/// Macro wrapper of inflateBackInit_() allows checking the zlib version and the compiler's view of z_stream
#  define z_inflateBackInit(strm, windowBits, window) \
          inflateBackInit_((strm), (windowBits), (window), \
                           ZLIB_VERSION, (int)sizeof(z_stream))
#else
/// Macro wrapper of deflateInit_() allows checking the zlib version and the compiler's view of z_stream
/// \ingroup basic
#  define deflateInit(strm, level) \
          deflateInit_((strm), (level), ZLIB_VERSION, (int)sizeof(z_stream))

/// Macro wrapper of inflateInit_() allows checking the zlib version and the compiler's view of z_stream
/// \ingroup basic
#  define inflateInit(strm) \
          inflateInit_((strm), ZLIB_VERSION, (int)sizeof(z_stream))

/// Macro wrapper of deflateInit2_() allows checking the zlib version and the compiler's view of z_stream
#  define deflateInit2(strm, level, method, windowBits, memLevel, strategy) \
          deflateInit2_((strm),(level),(method),(windowBits),(memLevel),\
                        (strategy), ZLIB_VERSION, (int)sizeof(z_stream))

/// Macro wrapper of inflateInit2_() allows checking the zlib version and the compiler's view of z_stream
#  define inflateInit2(strm, windowBits) \
          inflateInit2_((strm), (windowBits), ZLIB_VERSION, \
                        (int)sizeof(z_stream))

/// Macro wrapper of inflateBackInit_() allows checking the zlib version and the compiler's view of z_stream
#  define inflateBackInit(strm, windowBits, window) \
          inflateBackInit_((strm), (windowBits), (window), \
                           ZLIB_VERSION, (int)sizeof(z_stream))
#endif

#ifndef Z_SOLO

/* gzgetc() macro and its supporting function and exposed data structure. */

ZEXTERN int ZEXPORT gzgetc_(gzFile file);       /* backward compatibility */
#ifdef Z_PREFIX_SET
#  undef z_gzgetc
#  define z_gzgetc(g) \
          ((g)->have ? ((g)->have--, (g)->pos++, *((g)->next)++) : (gzgetc)(g))
#else
#  define gzgetc(g) \
          ((g)->have ? ((g)->have--, (g)->pos++, *((g)->next)++) : (gzgetc)(g))
#endif

/* provide 64-bit offset functions if _LARGEFILE64_SOURCE defined, and/or
 * change the regular functions to 64 bits if _FILE_OFFSET_BITS is 64 (if
 * both are true, the application gets the *64 functions, and the regular
 * functions are changed to 64 bits) -- in case these are set on systems
 * without large file support, _LFS64_LARGEFILE must also be true
 */
#ifdef Z_LARGE64
   ZEXTERN gzFile ZEXPORT gzopen64(const char *, const char *);
   ZEXTERN z_off64_t ZEXPORT gzseek64(gzFile, z_off64_t, int);
   ZEXTERN z_off64_t ZEXPORT gztell64(gzFile);
   ZEXTERN z_off64_t ZEXPORT gzoffset64(gzFile);
   ZEXTERN uLong ZEXPORT adler32_combine64(uLong, uLong, z_off64_t);
   ZEXTERN uLong ZEXPORT crc32_combine64(uLong, uLong, z_off64_t);
   ZEXTERN uLong ZEXPORT crc32_combine_gen64(z_off64_t);
#endif

#if !defined(ZLIB_INTERNAL) && defined(Z_WANT64)
#  ifdef Z_PREFIX_SET
#    define z_gzopen z_gzopen64
#    define z_gzseek z_gzseek64
#    define z_gztell z_gztell64
#    define z_gzoffset z_gzoffset64
#    define z_adler32_combine z_adler32_combine64
#    define z_crc32_combine z_crc32_combine64
#    define z_crc32_combine_gen z_crc32_combine_gen64
#  else
#    define gzopen gzopen64
#    define gzseek gzseek64
#    define gztell gztell64
#    define gzoffset gzoffset64
#    define adler32_combine adler32_combine64
#    define crc32_combine crc32_combine64
#    define crc32_combine_gen crc32_combine_gen64
#  endif
#  ifndef Z_LARGE64
     ZEXTERN gzFile ZEXPORT gzopen64(const char *, const char *);
     ZEXTERN z_off_t ZEXPORT gzseek64(gzFile, z_off_t, int);
     ZEXTERN z_off_t ZEXPORT gztell64(gzFile);
     ZEXTERN z_off_t ZEXPORT gzoffset64(gzFile);
     ZEXTERN uLong ZEXPORT adler32_combine64(uLong, uLong, z_off_t);
     ZEXTERN uLong ZEXPORT crc32_combine64(uLong, uLong, z_off_t);
     ZEXTERN uLong ZEXPORT crc32_combine_gen64(z_off_t);
#  endif
#else
   gzFile ZEXPORT gzopen(const char *, const char *);
   z_off_t ZEXPORT gzseek(gzFile, z_off_t, int);
   z_off_t ZEXPORT gztell(gzFile);
   z_off_t ZEXPORT gzoffset(gzFile);
   ZEXTERN uLong ZEXPORT adler32_combine(uLong, uLong, z_off_t);
   ZEXTERN uLong ZEXPORT crc32_combine(uLong, uLong, z_off_t);
   ZEXTERN uLong ZEXPORT crc32_combine_gen(z_off_t);
#endif

#else /* Z_SOLO */

   ZEXTERN uLong ZEXPORT adler32_combine(uLong, uLong, z_off_t);
   ZEXTERN uLong ZEXPORT crc32_combine(uLong, uLong, z_off_t);
   ZEXTERN uLong ZEXPORT crc32_combine_gen(z_off_t);

#endif /* !Z_SOLO */

/* undocumented functions */
ZEXTERN const char   * ZEXPORT zError(int);
ZEXTERN int            ZEXPORT inflateSyncPoint(z_streamp);
ZEXTERN const z_crc_t* ZEXPORT get_crc_table(void);
ZEXTERN int            ZEXPORT inflateUndermine(z_streamp, int);
ZEXTERN int            ZEXPORT inflateValidate(z_streamp, int);
ZEXTERN unsigned long  ZEXPORT inflateCodesUsed(z_streamp);
ZEXTERN int            ZEXPORT inflateResetKeep(z_streamp);
ZEXTERN int            ZEXPORT deflateResetKeep(z_streamp);
#if defined(_WIN32) && !defined(Z_SOLO)
ZEXTERN gzFile         ZEXPORT gzopen_w(const wchar_t *path,
                                        const char *mode);
#endif
#if defined(STDC) || defined(Z_HAVE_STDARG_H)
#  ifndef Z_SOLO
ZEXTERN int            ZEXPORTVA gzvprintf(gzFile file,
                                           const char *format,
                                           va_list va);
#  endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZLIB_H */
