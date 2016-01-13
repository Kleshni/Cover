LibEph5
=======

The library used by the Eph5 program. See its documentation.

`eph5.h`
--------

The main header file. Requires `stdint.h`, `stddef.h`, `jpeglib.h` and `nettle/arcfour.h` to be already included.

`LibEph5_initialize`
--------------------

``` C
LibEph5_result LibEph5_initialize(
	struct LibEph5_context *context,
	struct jpeg_decompress_struct *decompressor,
	const uint8_t *key
);
```

Initializes the context.

Requires the unitialized context, a LibJPEG decompressor structure with an opened image and a key of `LIBEPH5_KEY_LENGTH` bytes.

Can return `LibEph5_result_OK` or an error message. In that case the context remains unitialized. Errors from LibJPEG are not handled in this function, so `LibEph5_destroy` must be called manually.

The returned error message may be `LibEph5_result_cant_allocate_memory` with a correct `errno` set, `LibEph5_result_too_big_image` on machines, where `size_t` is <= 32 bits long, or one of `LibEph5_result_invalid_colour_space` and `LibEph5_result_invalid_block_size`. All errors are constant pointers.

`LibEph5_context.container_properties`
--------------------------------------

A public member of the `LibEph5_context` structure.

### `horizontal_blocks_count` and `vertical_blocks_count`

``` C
JDIMENSION horizontal_blocks_count;
JDIMENSION vertical_blocks_count;

```

The image dimensions in blocks.

### `coefficients_count`, `usable_coefficients_count` and `one_coefficients_count`

``` C
size_t coefficients_count;
size_t usable_coefficients_count;
size_t one_coefficients_count;
```

Counts of the coefficients, the non-zero AC coefficients and the coefficients with the absolute value of 1.

### `guaranteed_capacity` and `maximum_capacity`

``` C
size_t guaranteed_capacity[LIBEPH5_MAXIMUM_K];
size_t maximum_capacity[LIBEPH5_MAXIMUM_K];
```

The minimum and the maximum possible image capacity for different k values. Indexes = k - 1.

The arrays are non-increasing.

### `expected_capacity`

``` C
size_t expected_capacity[LIBEPH5_MAXIMUM_K];
```

Expectation of the image capacity. Is >= the minimum and <= the maximum estimations.

### `extractable_length`

``` C
size_t extractable_length[LIBEPH5_MAXIMUM_K];
```

The lengths of the data, which can be extracted from the image.

`LibEph5_write`
---------------

``` C
void LibEph5_write(struct LibEph5_context *context, struct jpeg_compress_struct *compressor);
```

Writes the resulting image using the compressor structure. The decompressor passed to `LibEph5_initialize` must remain unmodified until call to this function.

LibJPEG errors are not handled, and they can break the whole process.

The only supported action after this function is `LibEph5_destroy`.

`LibEph5_destroy`
-----------------

``` C
void LibEph5_destroy(struct LibEph5_context *context);
```

Destroys the context.

`LibEph5_embed`
---------------

``` C
size_t LibEph5_embed(struct LibEph5_context *context, size_t data_length, const uint8_t *data, int k);
```

Embeds the data into the image. `k` must be from 1 to `LIBEPH5_MAXIMUM_K`. `LibEph5_write` should be called after the embedding.

Returns the number of the embedded bytes.

`LibEph5_reset`
---------------

``` C
void LibEph5_reset(struct LibEph5_context *context);
```

Discards the changes made by `LibEph5_embed` if called before `LibEph5_write`.

`LibEph5_extract`
-----------------

``` C
void LibEph5_extract(struct LibEph5_context *context, uint8_t **data);
```

Extracts the data. The `data` argument must point to an array of `LIBEPH5_MAXIMUM_K` pointers to buffers of sufficient sizes.
