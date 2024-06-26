/*!
  \file gzlib.c Functions common to reading and writing gzip files
  
  Copyright (C) 2004-2019 Mark Adler
  For conditions of distribution and use, see copyright notice in zlib.h
*/

#include "gzguts.h"

#if defined(_WIN32) && !defined(__BORLANDC__)
#  define LSEEK _lseeki64
#else
#if defined(_LARGEFILE64_SOURCE) && _LFS64_LARGEFILE-0
#  define LSEEK lseek64
#else
#  define LSEEK lseek
#endif
#endif

/* Local functions */
static void gz_reset (gz_statep);
static gzFile gz_open (const void *, int, const char *);

#if defined UNDER_CE

/* Map the Windows error number in ERROR to a locale-dependent error message
   string and return a pointer to it.  Typically, the values for ERROR come
   from GetLastError.

   The string pointed to shall not be modified by the application, but may be
   overwritten by a subsequent call to gz_strwinerror

   The gz_strwinerror function does not change the current setting of
   GetLastError. */
char ZLIB_INTERNAL *gz_strwinerror (DWORD error)
{
    static char buf[1024];

    wchar_t *msgbuf;
    DWORD lasterr = GetLastError();
    DWORD chars = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        error,
        0, /* Default language */
        (LPVOID)&msgbuf,
        0,
        NULL);
    if (chars != 0) {
        /* If there is an \r\n appended, zap it.  */
        if (chars >= 2
            && msgbuf[chars - 2] == '\r' && msgbuf[chars - 1] == '\n') {
            chars -= 2;
            msgbuf[chars] = 0;
        }

        if (chars > sizeof (buf) - 1) {
            chars = sizeof (buf) - 1;
            msgbuf[chars] = 0;
        }

        wcstombs(buf, msgbuf, chars + 1);
        LocalFree(msgbuf);
    }
    else {
        sprintf(buf, "unknown win32 error (%ld)", error);
    }

    SetLastError(lasterr);
    return buf;
}

#endif /* UNDER_CE */

/* Reset gzip file state */
static void gz_reset (gz_statep state)
{
    state->x.have = 0;              /* no output data available */
    if (state->mode == GZ_READ) {   /* for reading ... */
        state->eof = 0;             /* not at end of file */
        state->past = 0;            /* have not read past end yet */
        state->how = LOOK;          /* look for gzip header */
    }
    else                            /* for writing ... */
        state->reset = 0;           /* no deflateReset pending */
    state->seek = 0;                /* no seek request pending */
    gz_error(state, Z_OK, NULL);    /* clear error */
    state->x.pos = 0;               /* no uncompressed data yet */
    state->strm.avail_in = 0;       /* no input data yet */
}

/* Open a gzip file either by name or file descriptor. */
gzFile gz_open (const void* path, int fd, const char* mode)
{
    gz_statep state;
    z_size_t len;
    int oflag;
#ifdef O_CLOEXEC
    int cloexec = 0;
#endif
#ifdef O_EXCL
    int exclusive = 0;
#endif

    /* check input */
    if (path == NULL)
        return NULL;

    /* allocate gzFile structure to return */
    state = (gz_statep)malloc(sizeof(gz_state));
    if (state == NULL)
        return NULL;
    state->size = 0;            /* no buffers allocated yet */
    state->want = GZBUFSIZE;    /* requested buffer size */
    state->msg = NULL;          /* no error message yet */

    /* interpret mode */
    state->mode = GZ_NONE;
    state->level = Z_DEFAULT_COMPRESSION;
    state->strategy = Z_DEFAULT_STRATEGY;
    state->direct = 0;
    while (*mode) {
        if (*mode >= '0' && *mode <= '9')
            state->level = *mode - '0';
        else
            switch (*mode) {
            case 'r':
                state->mode = GZ_READ;
                break;
#ifndef NO_GZCOMPRESS
            case 'w':
                state->mode = GZ_WRITE;
                break;
            case 'a':
                state->mode = GZ_APPEND;
                break;
#endif
            case '+':       /* can't read and write at the same time */
                free(state);
                return NULL;
            case 'b':       /* ignore -- will request binary anyway */
                break;
#ifdef O_CLOEXEC
            case 'e':
                cloexec = 1;
                break;
#endif
#ifdef O_EXCL
            case 'x':
                exclusive = 1;
                break;
#endif
            case 'f':
                state->strategy = Z_FILTERED;
                break;
            case 'h':
                state->strategy = Z_HUFFMAN_ONLY;
                break;
            case 'R':
                state->strategy = Z_RLE;
                break;
            case 'F':
                state->strategy = Z_FIXED;
                break;
            case 'T':
                state->direct = 1;
                break;
            default:        /* could consider as an error, but just ignore */
                ;
            }
        mode++;
    }

    /* must provide an "r", "w", or "a" */
    if (state->mode == GZ_NONE) {
        free(state);
        return NULL;
    }

    /* can't force transparent read */
    if (state->mode == GZ_READ) {
        if (state->direct) {
            free(state);
            return NULL;
        }
        state->direct = 1;      /* for empty file */
    }

    /* save the path name for error messages */
#ifdef WIDECHAR
    if (fd == -2) {
        len = wcstombs(NULL, path, 0);
        if (len == (z_size_t)-1)
            len = 0;
    }
    else
#endif
        len = strlen((const char *)path);
    state->path = (char *)malloc(len + 1);
    if (state->path == NULL) {
        free(state);
        return NULL;
    }
#ifdef WIDECHAR
    if (fd == -2)
        if (len)
            wcstombs(state->path, path, len + 1);
        else
            *(state->path) = 0;
    else
#endif
#if !defined(NO_snprintf) && !defined(NO_vsnprintf)
        (void)snprintf(state->path, len + 1, "%s", (const char *)path);
#else
        strcpy(state->path, path);
#endif

    /* compute the flags for open() */
    oflag =
#ifdef O_LARGEFILE
        O_LARGEFILE |
#endif
#ifdef O_BINARY
        O_BINARY |
#endif
#ifdef O_CLOEXEC
        (cloexec ? O_CLOEXEC : 0) |
#endif
        (state->mode == GZ_READ ?
         O_RDONLY :
         (O_WRONLY | O_CREAT |
#ifdef O_EXCL
          (exclusive ? O_EXCL : 0) |
#endif
          (state->mode == GZ_WRITE ?
           O_TRUNC :
           O_APPEND)));

    /* open the file with the appropriate flags (or just use fd) */
    state->fd = fd > -1 ? fd : (
#ifdef WIDECHAR
        fd == -2 ? _wopen(path, oflag, 0666) :
#endif
        open((const char *)path, oflag, 0666));
    if (state->fd == -1) {
        free(state->path);
        free(state);
        return NULL;
    }
    if (state->mode == GZ_APPEND) {
        LSEEK(state->fd, 0, SEEK_END);  /* so gzoffset() is correct */
        state->mode = GZ_WRITE;         /* simplify later checks */
    }

    /* save the current position for rewinding (only if reading) */
    if (state->mode == GZ_READ) {
        state->start = LSEEK(state->fd, 0, SEEK_CUR);
        if (state->start == -1) state->start = 0;
    }

    /* initialize stream */
    gz_reset(state);

    /* return stream */
    return (gzFile)state;
}

/*!
  Open the gzip (.gz) file at path for reading and decompressing, or
  compressing and writing.
  
  The mode parameter is as in fopen ("rb" or "wb")
  but can also include a compression level ("wb9") or a strategy: 'f' for
  filtered data as in "wb6f", 'h' for Huffman-only compression as in "wb1h",
  'R' for run-length encoding as in "wb1R", or 'F' for fixed code compression
  as in "wb9F".  (See the description of deflateInit2 for more information
  about the strategy parameter.)  'T' will request transparent writing or
  appending with no compression and not using the gzip format.

    "a" can be used instead of "w" to request that the gzip stream that will
  be written be appended to the file.  "+" will result in an error, since
  reading and writing to the same gzip file is not supported.  The addition of
  "x" when writing will create the file exclusively, which fails if the file
  already exists.  On systems that support it, the addition of "e" when
  reading or writing will set the flag to close the file on an execve() call.

    These functions, as well as gzip, will read and decode a sequence of gzip
  streams in a file.  The append function of gzopen() can be used to create
  such a file.  (Also see gzflush() for another way to do this.)  When
  appending, gzopen does not test whether the file begins with a gzip stream,
  nor does it look for the end of the gzip streams to begin appending.  gzopen
  will simply append a gzip stream to the existing file.

    gzopen can be used to read a file which is not in gzip format; in this
  case gzread will directly read from the file without decompression.  When
  reading, this will be detected automatically by looking for the magic two-
  byte gzip header.

  \return NULL if the file could not be opened, if there was
  insufficient memory to allocate the gzFile state, or if an invalid mode was
  specified (an 'r', 'w', or 'a' was not provided, or '+' was provided).
  errno can be checked to determine if the reason gzopen failed was that the
  file could not be opened.
*/
gzFile ZEXPORT gzopen (const char* path, const char* mode)
{
    return gz_open(path, -1, mode);
}

/* -- see zlib.h -- */
gzFile ZEXPORT gzopen64 (const char* path, const char* mode)
{
    return gz_open(path, -1, mode);
}

/*!
  Associate a gzFile with the file descriptor fd.  File descriptors are
  obtained from calls like open, dup, creat, pipe or fileno (if the file has
  been previously opened with fopen).  The mode parameter is as in gzopen.

  The next call of gzclose on the returned gzFile will also close the file
  descriptor fd, just like fclose(fdopen(fd, mode)) closes the file descriptor
  fd.  If you want to keep fd open, use fd = dup(fd_keep); gz = gzdopen(fd,
  mode);.  The duplicated descriptor should be saved to avoid a leak, since
  gzdopen does not close fd if it fails.  If you are using fileno() to get the
  file descriptor from a FILE *, then you will have to use dup() to avoid
  double-close()ing the file descriptor.  Both gzclose() and fclose() will
  close the associated file descriptor, so they need to have different file
  descriptors.

  gzdopen returns NULL if there was insufficient memory to allocate the
  gzFile state, if an invalid mode was specified (an 'r', 'w', or 'a' was not
  provided, or '+' was provided), or if fd is -1.  The file descriptor is not
  used until the next gz* read, write, seek, or close operation, so gzdopen
  will not detect if fd is invalid (unless fd is -1).
*/
gzFile ZEXPORT gzdopen (int fd, const char* mode)
{
    char *path;         /* identifier for error messages */
    gzFile gz;

    if (fd == -1 || (path = (char *)malloc(7 + 3 * sizeof(int))) == NULL)
        return NULL;
#if !defined(NO_snprintf) && !defined(NO_vsnprintf)
    (void)snprintf(path, 7 + 3 * sizeof(int), "<fd:%d>", fd);
#else
    sprintf(path, "<fd:%d>", fd);   /* for debugging */
#endif
    gz = gz_open(path, fd, mode);
    free(path);
    return gz;
}

/* -- see zlib.h -- */
#ifdef WIDECHAR
gzFile ZEXPORT gzopen_w (const wchar_t* path, const char* mode)
{
    return gz_open(path, -2, mode);
}
#endif

/*!
  Set the internal buffer size used by this library's functions for file to
  size.
  
  The default buffer size is 8192 bytes.  This function must be called
  after gzopen() or gzdopen(), and before any other calls that read or write
  the file.  The buffer memory allocation is always deferred to the first read
  or write.  Three times that size in buffer space is allocated.  A larger
  buffer size of, for example, 64K or 128K bytes will noticeably increase the
  speed of decompression (reading).

  The new buffer size also affects the maximum length for gzprintf().

  \return 0 on success, or -1 on failure, such as being called too late.
*/
int ZEXPORT gzbuffer (gzFile file, unsigned size)
{
    gz_statep state;

    /* get internal structure and check integrity */
    if (file == NULL)
        return -1;
    state = (gz_statep)file;
    if (state->mode != GZ_READ && state->mode != GZ_WRITE)
        return -1;

    /* make sure we haven't already allocated memory */
    if (state->size != 0)
        return -1;

    /* check and set requested size */
    if ((size << 1) < size)
        return -1;              /* need to be able to double it */
    if (size < 8)
        size = 8;               /* needed to behave well with flushing */
    state->want = size;
    return 0;
}

/*!
  Rewind file.
  
  This function is supported only for reading.

  gzrewind(file) is equivalent to (int)gzseek(file, 0L, SEEK_SET).
*/
int ZEXPORT gzrewind(gzFile file)
{
    gz_statep state;

    /* get internal structure */
    if (file == NULL)
        return -1;
    state = (gz_statep)file;

    /* check that we're reading and that there's no error */
    if (state->mode != GZ_READ ||
            (state->err != Z_OK && state->err != Z_BUF_ERROR))
        return -1;

    /* back up and start over */
    if (LSEEK(state->fd, state->start, SEEK_SET) == -1)
        return -1;
    gz_reset(state);
    return 0;
}

/*!
  Set the starting position to offset relative to whence for the next gzread
  or gzwrite on file.  The offset represents a number of bytes in the
  uncompressed data stream.  The whence parameter is defined as in lseek(2);
  the value SEEK_END is not supported.

  If the file is opened for reading, this function is emulated but can be
  extremely slow.  If the file is opened for writing, only forward seeks are
  supported; gzseek64 then compresses a sequence of zeroes up to the new
  starting position.

  \return the resulting offset location as measured in bytes from
  the beginning of the uncompressed stream, or -1 in case of error, in
  particular if the file is opened for writing and the new starting position
  would be before the current position.
*/
z_off64_t ZEXPORT gzseek64 (gzFile file, z_off64_t offset, int whence)
{
    unsigned n;
    z_off64_t ret;
    gz_statep state;

    /* get internal structure and check integrity */
    if (file == NULL)
        return -1;
    state = (gz_statep)file;
    if (state->mode != GZ_READ && state->mode != GZ_WRITE)
        return -1;

    /* check that there's no error */
    if (state->err != Z_OK && state->err != Z_BUF_ERROR)
        return -1;

    /* can only seek from start or relative to current position */
    if (whence != SEEK_SET && whence != SEEK_CUR)
        return -1;

    /* normalize offset to a SEEK_CUR specification */
    if (whence == SEEK_SET)
        offset -= state->x.pos;
    else if (state->seek)
        offset += state->skip;
    state->seek = 0;

    /* if within raw area while reading, just go there */
    if (state->mode == GZ_READ && state->how == COPY &&
            state->x.pos + offset >= 0) {
        ret = LSEEK(state->fd, offset - (z_off64_t)state->x.have, SEEK_CUR);
        if (ret == -1)
            return -1;
        state->x.have = 0;
        state->eof = 0;
        state->past = 0;
        state->seek = 0;
        gz_error(state, Z_OK, NULL);
        state->strm.avail_in = 0;
        state->x.pos += offset;
        return state->x.pos;
    }

    /* calculate skip amount, rewinding if needed for back seek when reading */
    if (offset < 0) {
        if (state->mode != GZ_READ)         /* writing -- can't go backwards */
            return -1;
        offset += state->x.pos;
        if (offset < 0)                     /* before start of file! */
            return -1;
        if (gzrewind(file) == -1)           /* rewind, then skip to offset */
            return -1;
    }

    /* if reading, skip what's in output buffer (one less gzgetc() check) */
    if (state->mode == GZ_READ) {
        n = GT_OFF(state->x.have) || (z_off64_t)state->x.have > offset ?
            (unsigned)offset : state->x.have;
        state->x.have -= n;
        state->x.next += n;
        state->x.pos += n;
        offset -= n;
    }

    /* request skip (if not zero) */
    if (offset) {
        state->seek = 1;
        state->skip = offset;
    }
    return state->x.pos + offset;
}

/*!
  Set the starting position to offset relative to whence for the next gzread
  or gzwrite on file.  The offset represents a number of bytes in the
  uncompressed data stream.  The whence parameter is defined as in lseek(2);
  the value SEEK_END is not supported.

  If the file is opened for reading, this function is emulated but can be
  extremely slow.  If the file is opened for writing, only forward seeks are
  supported; gzseek then compresses a sequence of zeroes up to the new
  starting position.

  gzseek returns the resulting offset location as measured in bytes from
  the beginning of the uncompressed stream, or -1 in case of error, in
  particular if the file is opened for writing and the new starting position
  would be before the current position.
*/
z_off_t ZEXPORT gzseek (gzFile file, z_off_t offset, int whence)
{
    z_off64_t ret;

    ret = gzseek64(file, (z_off64_t)offset, whence);
    return ret == (z_off_t)ret ? (z_off_t)ret : -1;
}

/*!
  Return the starting position for the next gzread or gzwrite on file.
  This position represents a number of bytes in the uncompressed data stream,
  and is zero when starting, even if appending or reading a gzip stream from
  the middle of a file using gzdopen().

  gztell64(file) is equivalent to gzseek64(file, 0L, SEEK_CUR)
*/
z_off64_t ZEXPORT gztell64 (gzFile file)
{
    gz_statep state;

    /* get internal structure and check integrity */
    if (file == NULL)
        return -1;
    state = (gz_statep)file;
    if (state->mode != GZ_READ && state->mode != GZ_WRITE)
        return -1;

    /* return position */
    return state->x.pos + (state->seek ? state->skip : 0);
}

/*!
  Return the starting position for the next gzread or gzwrite on file.
  This position represents a number of bytes in the uncompressed data stream,
  and is zero when starting, even if appending or reading a gzip stream from
  the middle of a file using gzdopen().

  gztell(file) is equivalent to gzseek(file, 0L, SEEK_CUR)
*/
z_off_t ZEXPORT gztell( gzFile file)
{
    z_off64_t ret;

    ret = gztell64(file);
    return ret == (z_off_t)ret ? (z_off_t)ret : -1;
}

/*!
  Return the current compressed (actual) read or write offset of file.

  This offset includes the count of bytes that precede the gzip stream, for example
  when appending or when using gzdopen() for reading.  When reading, the
  offset does not include as yet unused buffered input.  This information can
  be used for a progress indicator.  On error, gzoffset64() returns -1.
*/
z_off64_t ZEXPORT gzoffset64(gzFile file)
{
    z_off64_t offset;
    gz_statep state;

    /* get internal structure and check integrity */
    if (file == NULL)
        return -1;
    state = (gz_statep)file;
    if (state->mode != GZ_READ && state->mode != GZ_WRITE)
        return -1;

    /* compute and return effective offset in file */
    offset = LSEEK(state->fd, 0, SEEK_CUR);
    if (offset == -1)
        return -1;
    if (state->mode == GZ_READ)             /* reading */
        offset -= state->strm.avail_in;     /* don't count buffered input */
    return offset;
}

/*!
  Return the current compressed (actual) read or write offset of file.
  
  This offset includes the count of bytes that precede the gzip stream, for example
  when appending or when using gzdopen() for reading.  When reading, the
  offset does not include as yet unused buffered input.  This information can
  be used for a progress indicator.  On error, gzoffset() returns -1.
*/
z_off_t ZEXPORT gzoffset (gzFile file)
{
    z_off64_t ret;

    ret = gzoffset64(file);
    return ret == (z_off_t)ret ? (z_off_t)ret : -1;
}

/*!
  Return true (1) if the end-of-file indicator for file has been set while
  reading, false (0) otherwise.

  Note that the end-of-file indicator is set only if the read tried to go past
  the end of the input, but came up short. Therefore, just like feof(), gzeof()
  may return false even if there is no more data to read, in the event that the
  last read request was for the exact number of bytes remaining in the input file.
  This will happen if the input file size is an exact multiple of the buffer size.

  If gzeof() returns true, then the read functions will return no more data,
  unless the end-of-file indicator is reset by gzclearerr() and the input file
  has grown since the previous end of file was detected.
*/
int ZEXPORT gzeof (gzFile file)
{
    gz_statep state;

    /* get internal structure and check integrity */
    if (file == NULL)
        return 0;
    state = (gz_statep)file;
    if (state->mode != GZ_READ && state->mode != GZ_WRITE)
        return 0;

    /* return end-of-file state */
    return state->mode == GZ_READ ? state->past : 0;
}

/*!
  Return the error message for the last error which occurred on file.

  errnum is set to zlib error number.  If an error occurred in the file system
  and not in the compression library, errnum is set to Z_ERRNO and the
  application may consult errno to get the exact error code.

  The application must not modify the returned string.  Future calls to
  this function may invalidate the previously returned string.  If file is
  closed, then the string previously returned by gzerror will no longer be
  available.

  gzerror() should be used to distinguish errors from end-of-file for those
  functions above that do not distinguish those cases in their return values.
*/
const char* ZEXPORT gzerror (gzFile file, int* errnum)
{
    gz_statep state;

    /* get internal structure and check integrity */
    if (file == NULL)
        return NULL;
    state = (gz_statep)file;
    if (state->mode != GZ_READ && state->mode != GZ_WRITE)
        return NULL;

    /* return error information */
    if (errnum != NULL)
        *errnum = state->err;
    return state->err == Z_MEM_ERROR ? "out of memory" :
                                       (state->msg == NULL ? "" : state->msg);
}

/*!
  Clear the error and end-of-file flags for file.
  
  This is analogous to the clearerr() function in stdio.  This is useful for 
  continuing to read a gzip file that is being written concurrently.
*/
void ZEXPORT gzclearerr (gzFile file)
{
    gz_statep state;

    /* get internal structure and check integrity */
    if (file == NULL)
        return;
    state = (gz_statep)file;
    if (state->mode != GZ_READ && state->mode != GZ_WRITE)
        return;

    /* clear error and end-of-file */
    if (state->mode == GZ_READ) {
        state->eof = 0;
        state->past = 0;
    }
    gz_error(state, Z_OK, NULL);
}

/*!
  Create an error message in allocated memory and set state->err and
  state->msg accordingly.  Free any previous error message already there.  Do
  not try to free or allocate space if the error is Z_MEM_ERROR (out of
  memory).  Simply save the error message as a static string.  If there is an
  allocation failure constructing the error message, then convert the error to
  out of memory.
*/
void ZLIB_INTERNAL gz_error (gz_statep state, int err, const char* msg)
{
    /* free previously allocated message and clear */
    if (state->msg != NULL) {
        if (state->err != Z_MEM_ERROR)
            free(state->msg);
        state->msg = NULL;
    }

    /* if fatal, set state->x.have to 0 so that the gzgetc() macro fails */
    if (err != Z_OK && err != Z_BUF_ERROR)
        state->x.have = 0;

    /* set error code, and if no message, then done */
    state->err = err;
    if (msg == NULL)
        return;

    /* for an out of memory error, return literal string when requested */
    if (err == Z_MEM_ERROR)
        return;

    /* construct error message with path */
    if ((state->msg = (char *)malloc(strlen(state->path) + strlen(msg) + 3)) ==
            NULL) {
        state->err = Z_MEM_ERROR;
        return;
    }
#if !defined(NO_snprintf) && !defined(NO_vsnprintf)
    (void)snprintf(state->msg, strlen(state->path) + strlen(msg) + 3,
                   "%s%s%s", state->path, ": ", msg);
#else
    strcpy(state->msg, state->path);
    strcat(state->msg, ": ");
    strcat(state->msg, msg);
#endif
}

#ifndef INT_MAX
/* portably return maximum value for an int (when limits.h presumed not
   available) -- we need to do this to cover cases where 2's complement not
   used, since C standard permits 1's complement and sign-bit representations,
   otherwise we could just use ((unsigned)-1) >> 1 */
unsigned ZLIB_INTERNAL gz_intmax()
{
    unsigned p, q;

    p = 1;
    do {
        q = p;
        p <<= 1;
        p++;
    } while (p > q);
    return q >> 1;
}
#endif
