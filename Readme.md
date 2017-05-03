Cover 1.0.0
===========

C library and console line program for JPEG steganography. Implements two algorithms:

- [DDT](https://github.com/desudesutalk/desudesutalk) version of [F5](https://code.google.com/p/f5-steganography/).

- [Rang](https://github.com/Kleshni/Cover/Rang%20specification).

The program also supports extraction and replacement of raw DCT coefficients.

The library requires [LibJPEG 8, 9a or 9b](http://www.ijg.org/) and [Nettle](http://www.lysator.liu.se/~nisse/nettle/). The console line interface also needs [Imlib2](https://docs.enlightenment.org/api/imlib2/html/index.html) and [GnuTLS](https://gnutls.org/) and depends on the GNU LibC extensions.

[CMake](https://cmake.org/) is used as a build system and [Doxygen](https://www.stack.nl/~dimitri/doxygen/) is needed for documentation generation.

Build
-----

Build targets `library` and `tool`. On Linux: run `cmake .` to generate a makefile, and then `make library tool`.

Documentation
-------------

See the console line program [documentation](tool/Documentation.md).

The library documentation can be found in the public [header files](include). Build target `documentation` to compile HTML.

Install
-------

Execute target `install/strip`.

Links
-----

* [Source code](https://github.com/Kleshni/Cover/archive/master.zip).
* [Git repository](https://github.com/Kleshni/Cover.git).
* [Issue tracker](https://github.com/Kleshni/Cover/issues).
* Bitmessage: BM-FHMGLusCyAEjonpwAYdxzfcyBszP.
* Mail: [kleshni@protonmail.ch](mailto:kleshni@protonmail.ch).
