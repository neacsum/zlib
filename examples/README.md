This directory contains examples of the use of **zlib** and other relevant programs and documentation.

### `enough.c` - Calculation and justification of ENOUGH parameter in `inftrees.h`
Calculates the maximum table space used in inflate tree construction over all possible Huffman codes

### `fitblk.c` - Compress just enough input to nearly fill a requested output size
**zlib** isn't designed to do this, but `fitblk` does it anyway.

### `gun.c` - Uncompress a gzip file
Illustrates the use of inflateBack() for high speed file-to-file decompression using call-back functions. Is approximately twice as fast as `gzip -d`. Also provides Unix uncompress functionality, again twice as fast.

### `gzappend.c` - Append to a gzip file
Illustrates the use of the `Z_BLOCK` flush parameter for `inflate()`. Illustrates the use of `deflatePrime()` to start at any bit.

### `gzjoin.c` - Join gzip files without recalculating the CRC or recompressing
Illustrates the use of the Z_BLOCK flush parameter for inflate(). Illustrates the use of `crc32_combine()`.

### `gzlog.c`, `gzlog.h` - Efficiently and robustly maintain a message log file in gzip format
Illustrates use of raw deflate, `Z_PARTIAL_FLUSH`, `deflatePrime()`, and `deflateSetDictionary()`. Illustrates use of a gzip header extra field.

### `gznorm.c` - Normalize a gzip file by combining members into a single member
Demonstrates how to concatenate deflate streams using `Z_BLOCK`.

### `zlib_how.html` - Painfully comprehensive description of `zpipe.c` (see below)
Describes in excruciating detail the use of `deflate()` and `inflate()`.

### `zpipe.c` - Reads and writes zlib streams from stdin to stdout
Illustrates the proper use of `deflate()` and `inflate()`. Deeply commented in `zlib_how.html` (see above).

### `zran.c`, `zran.h` -  Index a zlib or gzip stream and randomly access it
Illustrates the use of `Z_BLOCK`, `inflatePrime()`, and `inflateSetDictionary()` to provide random access.
