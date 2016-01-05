Eph5.js
=======

[DDT](https://github.com/desudesutalk/desudesutalk) version of the [F5](https://code.google.com/p/f5-steganography/) steganography algorithm for Javascript.

`Eph5.Raw(memory)`
------------------

Constructor of a raw interface to the C library. By default it allocates 16 MiB of memory. This value can be changed with the argument (in bytes).

Some methods can throw errors. In that case, the only supported action is to call `load` with another image. If the error is `Eph5.cantAllocateMemoryError`, the coder is broken and can't be used anymore.

### `load(image, key)`

Loads the image, which is an instance of `Uint8Array`. The key length can be found in the C library documentation.

Returns an object with the container properties.

The class can be used to process multiple images. To replace the current image, call this method again.

### `save()`

Returns the image as a `Uint8Array`. The only usable method after this function is `load`.

### `embed(data, k)`

Embeds the data (`Uint8Array`), using the specified k value (an integer from 1 to `LIBEPH5_MAXIMUM_K` from the C library).

Returns the number of the embedded bytes.

### `reset()`

Undoes the changes made by the `embed` method.

### `extract(k)`

Extracts the data from the image.

`Eph5.Simple(memory)`
---------------------

Constructor of a simple coder. By default it allocates 16 MiB of memory and increases this value when needed.

Its methods can throw errors.

### `embed(data, image, key)`

Embeds the data into the image using the key. The key length can be found in the C library documentation.

Returns an object with the container properties, the used k value, the number of the embedded bytes and the resulting image.

### `extract(image, key)`

Extracts data from the image using the key.

Returns a map from k values to the extracted data.
