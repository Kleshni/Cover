#define LIBEPH5_MAXIMUM_K 7 // Must be positive integer <= 7
#define LIBEPH5_KEY_LENGTH ARCFOUR_MAX_KEY_SIZE

typedef char *LibEph5_result;

extern const LibEph5_result LibEph5_result_OK;
extern const LibEph5_result LibEph5_result_cant_allocate_memory;
extern const LibEph5_result LibEph5_result_too_big_image;
extern const LibEph5_result LibEph5_result_invalid_colour_space;
extern const LibEph5_result LibEph5_result_invalid_block_size;

struct LibEph5_context {
	struct jpeg_decompress_struct *decompressor;

	struct jvirt_barray_control **coefficient_arrays;
	jpeg_component_info *component;

	struct {
		JDIMENSION horizontal_blocks_count;
		JDIMENSION vertical_blocks_count;

		size_t coefficients_count;
		size_t usable_coefficients_count;
		size_t one_coefficients_count;

		size_t guaranteed_capacity[LIBEPH5_MAXIMUM_K];
		size_t maximum_capacity[LIBEPH5_MAXIMUM_K];
		size_t expected_capacity[LIBEPH5_MAXIMUM_K];
	} container_properties;

	uint32_t *permutation;
	uint8_t *keystream;
	uint8_t *usable_coefficients;
	uint8_t *coefficients_payload;
	uint8_t *one_coefficients;
	uint8_t *modified_coefficients;
};

extern LibEph5_result LibEph5_initialize(
	struct LibEph5_context *context,
	struct jpeg_decompress_struct *decompressor,
	const uint8_t *key
);

extern void LibEph5_write(struct LibEph5_context *context, struct jpeg_compress_struct *compressor);
extern void LibEph5_destroy(struct LibEph5_context *context);

extern size_t LibEph5_embed(struct LibEph5_context *context, size_t data_length, const uint8_t *data, int k);
extern void LibEph5_reset(struct LibEph5_context *context);
extern size_t LibEph5_extract(struct LibEph5_context *context, size_t data_length, uint8_t *data, int k);
