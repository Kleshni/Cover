Eph5
====

[DDT](https://github.com/desudesutalk/desudesutalk) version of the [F5](https://code.google.com/p/f5-steganography/) steganography algorithm for Linux.

Note, that this version of F5 uses a weak encryption method and its permutation algorithm is not suitable for big images.

Analyze container
-----------------

``` Shell
eph5 analyze <container>
```

Prints the steganographic properties of the image.

Embed data
----------

``` Shell
eph5 embed <data> <container> <result>
```

Options:

- `--k <k>`, `-k <k>` - k value, must be an integer from 1 to 7. The default is 7.
- `--analyze`, `-a` - analyze the container and choose the k automatically.
- `--fit`, `-f` - if the data size exceeds the capacity of the container, try lesser k values.
- `--password <password>`, `-p <password>` - password for the steganography and the encryption. Defaults to "desu".

Extract data
------------

``` Shell
eph5 extract <container> <data>
```

Options:

- `--k <k>`, `-k <k>`.
- `--password <password>`, `-p <password>`.