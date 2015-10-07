Eph5
====

[DDT](https://github.com/desudesutalk/desudesutalk) version of the [F5](https://code.google.com/p/f5-steganography/) steganography algorithm.

It consists of a [C library](Library), a [native utility](Console line interface) for Linux and a [Javascript library](Javascript library) compiled with [Emscripten](http://kripken.github.io/emscripten-site/).

See the description in the CLI [documentation](Console line interface/Documentation.md).

Build
-----

### Library

Install [LibJPEG 8 or 9a](http://www.ijg.org/) and [Nettle 3](http://www.lysator.liu.se/~nisse/nettle/) and run `CFLAGS=-O3 make` in the library directory.

### Console line interface

Build the library and then make:

``` Shell
CPATH=../Library/ LIBRARY_PATH=../Library/ CFLAGS=-O3 LDFLAGS=-O3 make
```

It requires GNU extensions `getopt_long` and `error`.

### Javascript library

Get the latest Emscripten (not from the Ubuntu repository) and the sources of [LibJPEG 9a](http://www.ijg.org/files/jpegsrc.v9a.tar.gz) and [Nettle 3](https://ftp.gnu.org/gnu/nettle/).

Place the sources into the `Javascript library` folder and rename the directories to `jpeg` and `nettle` (i. e. without version numbers). Then build LibJPEG in its directory:

``` Shell
emconfigure ./configure CFLAGS=-O3
cat ../jmorecfg.h >> jmorecfg.h
emmake make
```

Build Nettle:

``` Shell
emconfigure ./configure --build=x86
emmake make CFLAGS=-O3
```

Build LibEph5 in the `eph5` directory:

``` Shell
make clean
CPATH=..:../jpeg CFLAGS=-O3 emmake make
```

And make the Javascript library: `CFLAGS=-O3 LDFLAGS=-O3 make`.

You can replace `-O3` in the `CFLAGS` of LibJPEG and Nettle with `-Oz` to get a smaller code. `sed -i s/\\u00/\\x/g eph5.js` after the compilation also reduces the code size.

Links
-----

* [Source code](https://github.com/Kleshni/Eph5/archive/master.zip).
* [Git repository](https://github.com/Kleshni/Eph5.git).
* [Issue tracker](https://github.com/Kleshni/Eph5/issues).
* Bitmessage: BM-FHMGLusCyAEjonpwAYdxzfcyBszP.
* Mail: [biryuzovye.kleshni@yandex.ru](mailto:biryuzovye.kleshni@yandex.ru).
