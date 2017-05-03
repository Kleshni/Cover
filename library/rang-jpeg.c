#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <cover/rang.h>
#include <cover/container.h>
#include <jpeglib.h>
#include <nettle/salsa20.h>

static void decode_coefficients(struct Cover_Rang *context) {
	struct jpeg_decompress_struct *decompressor = context->clear->decompressor;
	struct jvirt_barray_control *coefficients = context->clear->coefficients;

	bool compare = context->modified != NULL;

	struct jpeg_decompress_struct *modified_decompressor;
	struct jvirt_barray_control *modified_coefficients;

	if (compare) {
		modified_decompressor = context->modified->decompressor;
		modified_coefficients = context->modified->coefficients;
	}

	size_t width_in_blocks = context->clear->width_in_blocks;
	size_t height_in_blocks = context->clear->height_in_blocks;

	size_t set_count = 0;
	size_t usable_count = 0;

	uint8_t *payload = context->payload;
	uint32_t *usable = context->usable;
	uint8_t *direction = context->direction;

	size_t i = 0;

	for (JDIMENSION y = 0; y < height_in_blocks; ++y) {
		JBLOCKARRAY buffer = decompressor->mem->access_virt_barray(
			(struct jpeg_common_struct *) decompressor,
			coefficients, y, 1, false
		);

		JBLOCKARRAY modified_buffer;

		if (compare) {
			modified_buffer = modified_decompressor->mem->access_virt_barray(
				(struct jpeg_common_struct *) modified_decompressor,
				modified_coefficients, y, 1, false
			);
		}

		for (size_t x = 0; x < width_in_blocks; ++x) {
			for (size_t c = 0; c < COVER_CONTAINER_BLOCK_LENGTH; ++c) {
				JCOEF coefficient = buffer[0][x][c];

				bool payload_bit = coefficient % 2 != 0;

				set_count += payload_bit;

				payload[i / 8] |= payload_bit << i % 8;

				if (compare) {
					JCOEF modified_coefficient = modified_buffer[0][x][c];

					if (modified_coefficient != coefficient) {
						usable[usable_count] = i;

						++usable_count;
					}

					direction[i / 8] |= (modified_coefficient > coefficient) << i % 8;
				}

				++i;
			}
		}
	}

	context->set_count = set_count;
	context->usable_count = usable_count;
}

static const uint8_t strings_seed[SALSA20_256_KEY_SIZE];
static const uint8_t randomization_nonce[SALSA20_NONCE_SIZE];

bool Cover_Rang_initialize(
	struct Cover_Rang *context,
	struct Cover_container *clear,
	struct Cover_container *modified,
	const uint8_t *entropy
) {
	context->clear = clear;
	context->modified = modified;

	salsa20_256_set_key(&context->strings_PRNG, strings_seed);

	if (modified != NULL) {
		salsa20_256_set_key(&context->randomization_PRNG, entropy);
		salsa20_set_nonce(&context->randomization_PRNG, randomization_nonce);
	}

	context->bit_array_length = clear->coefficients_count / 8;

	context->payload = calloc(context->bit_array_length == 0 ? 1 : context->bit_array_length, 1);

	if (context->payload == NULL) {
		goto error_payload;
	}

	context->usable = NULL;
	context->direction = NULL;
	context->changes = NULL;

	if (modified != NULL) {
		if (SIZE_MAX / 4 < clear->coefficients_count) {
			goto error_usable;
		}

		context->usable = malloc(4 * (clear->coefficients_count == 0 ? 1 : clear->coefficients_count));

		if (context->usable == NULL) {
			goto error_usable;
		}

		context->direction = calloc(context->bit_array_length == 0 ? 1 : context->bit_array_length, 1);

		if (context->direction == NULL) {
			goto error_direction;
		}

		context->changes = malloc(context->bit_array_length == 0 ? 1 : context->bit_array_length);

		if (context->changes == NULL) {
			goto error_changes;
		}
	}

	decode_coefficients(context);

	return true;

	free(context->changes);
	error_changes: free(context->direction);
	error_direction: free(context->usable);
	error_usable: free(context->payload);
	error_payload: ;

	return false;
}

void Cover_Rang_destroy(struct Cover_Rang *context) {
	free(context->changes);
	free(context->direction);
	free(context->usable);
	free(context->payload);
}

size_t Cover_Rang_apply(struct Cover_Rang *context) {
	struct jpeg_decompress_struct *decompressor = context->clear->decompressor;
	struct jvirt_barray_control *coefficients = context->clear->coefficients;

	size_t width_in_blocks = context->clear->width_in_blocks;
	size_t height_in_blocks = context->clear->height_in_blocks;

	uint8_t *direction = context->direction;

	uint8_t *changes = context->changes;

	size_t changed_count = 0;
	size_t i = 0;

	for (JDIMENSION y = 0; y < height_in_blocks; ++y) {
		JBLOCKARRAY buffer = decompressor->mem->access_virt_barray(
			(struct jpeg_common_struct *) decompressor,
			coefficients, y, 1, true
		);

		for (size_t x = 0; x < width_in_blocks; ++x) {
			for (size_t c = 0; c < COVER_CONTAINER_BLOCK_LENGTH; ++c) {
				if ((changes[i / 8] >> i % 8 & 1) == 1) {
					JCOEF *coefficient = &buffer[0][x][c];

					if ((direction[i / 8] >> i % 8 & 1) == 1) {
						++*coefficient;
					} else {
						--*coefficient;
					}

					++changed_count;
				}

				++i;
			}
		}
	}

	return changed_count;
}

void Cover_Rang_extract(struct Cover_Rang *context, size_t length, uint8_t *data) {
	memset(data, 0, length);
	Cover_Rang_hash(length, data, &context->strings_PRNG, 0, context->clear->coefficients_count, context->payload);
}

#define SAMPLE_BUFFER_LENGTH SALSA20_BLOCK_SIZE

static const uint8_t zero_sample_buffer[SAMPLE_BUFFER_LENGTH];

static void generate_sample(struct salsa20_ctx *context, size_t needed_count, size_t count, uint32_t *sample) {
	uint8_t buffer[SAMPLE_BUFFER_LENGTH];
	size_t unused_count = count;

	size_t j = SAMPLE_BUFFER_LENGTH;

	for (size_t i = 0; i < needed_count; ++i) {
		if (j == SAMPLE_BUFFER_LENGTH) {
			salsa20_crypt(context, SAMPLE_BUFFER_LENGTH, buffer, zero_sample_buffer);

			j = 0;
		}

		uint_fast64_t index = i + (
			(uint_fast64_t) buffer[j] |
			(uint_fast64_t) buffer[j + 1] << 8 |
			(uint_fast64_t) buffer[j + 2] << 16 |
			(uint_fast64_t) buffer[j + 3] << 24 |
			(uint_fast64_t) buffer[j + 4] << 32 |
			(uint_fast64_t) buffer[j + 5] << 40 |
			(uint_fast64_t) buffer[j + 6] << 48 |
			(uint_fast64_t) buffer[j + 7] << 56
		) % unused_count;

		j += 8;
		--unused_count;

		uint_fast32_t temp = sample[index];

		sample[index] = sample[i];
		sample[i] = temp;
	}
}

int Cover_Rang_embed(struct Cover_Rang *context, size_t length, const uint8_t *data, size_t padding_bits_count) {
	int result = 1;

	if (SIZE_MAX / 8 < length) {
		goto error_allocation;
	}

	size_t width = 8 * length;

	size_t padding_blocks_count = (
		padding_bits_count / (8 * SALSA20_BLOCK_SIZE) +
		(padding_bits_count % (8 * SALSA20_BLOCK_SIZE) != 0)
	);

	if (
		length != 0 && SIZE_MAX / length - width < padding_bits_count ||
		SIZE_MAX / sizeof (uint8_t *) - width < padding_bits_count ||
		(SIZE_MAX - length) / SALSA20_BLOCK_SIZE < padding_blocks_count
	) {
		goto error_allocation;
	}

	size_t buffer_length = length * (width + padding_bits_count);
	size_t matrix_length = sizeof (uint8_t *) * (width + padding_bits_count);
	size_t vector_length = length + SALSA20_BLOCK_SIZE * padding_blocks_count;

	uint8_t *buffer = malloc(buffer_length == 0 ? 1 : buffer_length);

	if (buffer == NULL) {
		goto error_buffer;
	}

	uint8_t **matrix = malloc(matrix_length == 0 ? 1 : matrix_length);

	if (matrix == NULL) {
		goto error_matrix;
	}

	uint8_t *vector = malloc(vector_length == 0 ? 1 : vector_length);

	if (vector == NULL) {
		goto error_vector;
	}

	memcpy(vector, data, length);
	Cover_Rang_hash(length, vector, &context->strings_PRNG, 0, context->clear->coefficients_count, context->payload);

	memset(vector + length, 0, vector_length - length);

	salsa20_crypt(
		&context->randomization_PRNG,
		vector_length - length,
		vector + length,
		vector + length
	);

	for (size_t i = 0; i < width + padding_bits_count; ++i) {
		matrix[i] = buffer + length * i;
	}

	generate_sample(&context->randomization_PRNG, width + padding_bits_count, context->usable_count, context->usable);

	size_t used_padding = padding_bits_count;

	if (!Cover_Rang_unhash(length, &used_padding, vector, true, matrix, &context->strings_PRNG, context->usable)) {
		result = 2;

		goto error_unhash;
	}

	memset(context->changes, 0, context->bit_array_length);

	for (size_t i = 0; i < width + used_padding; ++i) {
		size_t index = context->usable[i];

		if ((vector[i / 8] >> i % 8 & 1) == 1) {
			context->changes[index / 8] |= 1 << index % 8;
		}
	}

	result = 0;

	error_unhash: free(vector);
	error_vector: free(matrix);
	error_matrix: free(buffer);
	error_buffer: ;
	error_allocation: ;

	return result;
}
