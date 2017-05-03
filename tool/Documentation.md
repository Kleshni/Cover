Cover 1.0.0
===========

Console line program for JPEG steganography. Implements two algorithms:

- [DDT](https://github.com/desudesutalk/desudesutalk) version of [F5](https://code.google.com/p/f5-steganography/).

- [Rang-JPEG](https://github.com/Kleshni/Cover/Rang%20specification).

And also supports extraction and replacement of raw DCT coefficients.

DCT coefficients
----------------

```
cover container read <image> <result>
```

Extracts raw DCT coefficients of the first (greyscale) colour component of an image. They are stored in a binary file as 2-byte signed integers in big-endian two's complement.

```
cover container replace <coefficients> <image> <result>
```

Replaces coefficients of an image. The new coefficients must be compatible with the original quantization table.

DDT F5
-------

```
cover eph5 extract <image> <result 1> ... <result 7>
```

Extracts data from an image using all seven `k` values. Options:
- `--password/-p <string>` - defaults to `desu`.

```
cover eph5 embed <data> <image> <result>
```

Embeds data into an image.

Options:
- `--k/-k <number>` - `k` value, an integer from 1 to 7. The default is 7;
- `--analyze/-a` - analyze the image and choose `k` automatically;
- `--fit/-f` - if the data size exceeds the capacity of the image, try lesser k values;
- `--password/-p <string>` - defaults to `desu`.

Note, that this version of F5 uses a weak encryption method and its permutation algorithm is not suitable for big images. And F5 is [completely broken](https://f5-steganography.googlecode.com/files/Breaking%20F5.pdf), anyway.

Rang
----

```
cover rang extract <image> <result>
```

Extracts data from an image.

Options:
- `--length/-l <number>` - the data length in bytes. The default is 1024.

```
cover rang embed <data> <clear> <modified> <result>
```

Embeds data into a clear image, using its modified version.

Options:
- `--entropy/-e <file>` - entropy source, the program reads first 32 bytes from this file;
- `--padding-bits-count/-p <number>` - count of padding bits, defaults to 24.

One possible way to create the clear and modified images:

```
cover rang modify <image> <clear> <modified>
```

This command converts an image from any format, supported by [Imlib2](https://docs.enlightenment.org/api/imlib2/html/index.html), into two JPEG images with unnoticable differences.

The absolute difference between corresponding coefficients in the clear and the modified images may exceed 1, the embedding command is interested only in the direction of the change.

Note, that the algorithm has two significant limitations:
- it can't embed huge files, because the embedding time grows as a cube of the data size. The practical limit is about 2 kiB;
- its security relies on indistinguishability of the embedded data from uniform randomness.

If the data is not random, it should be encrypted. For example:

```
openssl aes-256-cbc -nosalt -in <data> -out <result>
```

Encrypts data into a random-looking file without detectable headers. This command deterministically generates the initialization vector from the password, so it must be different every time.
