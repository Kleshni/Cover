LibEph5
=======

The library used by the Eph5 utility. See its documentation.

`eph5.h`
--------

The main header file. Requires `stdint.h`, `stddef.h`, `jpeglib.h` and `nettle/arcfour.h` to be already included.

`LibEph5_initialize`
--------------------

``` C
extern LibEph5_result LibEph5_initialize(
	struct LibEph5_context *context,
	struct jpeg_common_struct *compressor,
	struct jvirt_barray_control **coefficient_arrays,
	size_t key_length,
	const uint8_t *key
);
```

Initializes the context.

Requires the unitialized context, a LibJPEG compressor/decompressor structure with an opened image, the result of `jpeg_read_coefficients` on this compressor, length of a key (from `LIBEPH5_MINIMUM_KEY_LENGTH` to `LIBEPH5_MAXIMUM_KEY_LENGTH` or 0) and the key. If the length = 0, the default key is used (PBKDF 2 of "desu", parameters can be found in the CLI sources).

The image must be in the correct colour space (can be checked with the `LIBEPH5_CHECK_COLOUR_SPACE(space)` macro, which uses its argument twice) and have the rigth block size = `LIBEPH5_BLOCK_LENGTH`.

Can return `LibEph5_result_OK` or an error message. In that case the context remains unitialized. Errors in the LibJPEG functions are not handled, and `LibEph5_destroy` must be called manually.

The returned error message may be `LibEph5_result_cant_allocate_memory` with correct `errno` set or `LibEph5_result_too_big_image` on machines, where `size_t` is <= 32 bits long. Both errors are constant pointers.

`LibEph5_context.container`
---------------------------

A public member of the `LibEph5_context` structure.

### `struct jpeg_common_struct *compressor` and `struct jvirt_barray_control **coefficient_arrays`

The second and the third arguments of the initializer.

### `JDIMENSION horizontal_blocks_count` and `JDIMENSION vertical_blocks_count`

The image dimensions in blocks.

### `size_t coefficients_count`, `size_t usable_coefficients_count` and `size_t one_coefficients_count`

Counts of the coefficients, the non-zero coefficients and the coefficients with the absolute value of 1.

### `size_t guaranteed_capacity[LIBEPH5_MAXIMUM_K]` and `size_t maximum_capacity[LIBEPH5_MAXIMUM_K]`

The minimum and the maximum possible image capacity for different k values. Indexes = k - 1.

The arrays are non-increasing.

### `size_t expected_capacity[LIBEPH5_MAXIMUM_K];`

Expectation of the image capacity. Is >= the minimum and <= the maximum estimations.

`LibEph5_apply_changes`
-----------------------

``` C
extern void LibEph5_apply_changes(struct LibEph5_context *context);
```

Modifies the coefficients array to apply the changes planned by `LibEph5_embed`. LibJPEG errors are not handeled, and they can break the image.

Coefficients passed to `LibEph5_initialize` must remain unchanged until call to this function.

The only supported actions after this function are `LibEph5_fix_dummy_blocks` and `LibEph5_destroy`.

`LibEph5_fix_dummy_blocks`
--------------------------

``` C
extern void LibEph5_fix_dummy_blocks(struct LibEph5_context *context, struct jpeg_compress_struct *compressor);
```

This function must be called right after a call to `jpeg_write_coefficients`. Expected to be removed in next versions.

`LibEph5_destroy`
-----------------

``` C
extern void LibEph5_destroy(struct LibEph5_context *context);
```

Destroys the context.

`LibEph5_embed`
---------------

``` C
extern size_t LibEph5_embed(struct LibEph5_context *context, size_t data_length, const uint8_t *data, int k);
```

Embeds the data into the image. `k` must be from 1 to `LIBEPH5_MAXIMUM_K`. `LibEph5_apply_changes` should be called after the embedding.

Returns the number of the embedded bytes.

`LibEph5_reset`
---------------

``` C
extern void LibEph5_reset(struct LibEph5_context *context);
```

Discards the changes made by `LibEph5_embed` if called before `LibEph5_apply_changes`.

`LibEph5_extract`
-----------------

``` C
extern size_t LibEph5_extract(struct LibEph5_context *context, size_t data_length, uint8_t *data, int k);
```

Extracts the data.

Returns the number of the extracted bytes <= `data_length`.
