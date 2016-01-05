#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <jpeglib.h>
#include <nettle/arcfour.h>

#include "eph5.h"
#include "analyze.h"

const LibEph5_result LibEph5_result_OK = NULL;
const LibEph5_result LibEph5_result_cant_allocate_memory = "Can't allocate memory";
const LibEph5_result LibEph5_result_too_big_image = "Too big image";
const LibEph5_result LibEph5_result_invalid_colour_space = "Invalid colour space";
const LibEph5_result LibEph5_result_invalid_block_size = "Invalid block size";

#define PERMUTATION_BUFFER_SIZE 8192 // Must be a multiple of 4 and small enough for an allocation on the stack

static void generate_permutation(struct arcfour_ctx *cipher_context, size_t permutation_length, uint32_t *permutation) {
	for (size_t i = 0; i < permutation_length; ++i) {
		permutation[i] = i;
	}

	uint8_t buffer[PERMUTATION_BUFFER_SIZE];
	size_t last_index = permutation_length;

	for (size_t i = 0; i < permutation_length; i += PERMUTATION_BUFFER_SIZE / 4) {
		size_t buffer_length = PERMUTATION_BUFFER_SIZE;

		if (permutation_length - i < PERMUTATION_BUFFER_SIZE / 4) {
			buffer_length = (permutation_length - i) * 4;
		}

		memset(buffer, 0, buffer_length);
		arcfour_crypt(cipher_context, buffer_length, buffer, buffer);

		for (size_t j = 0; j < buffer_length; j += 4) {
			int32_t index = (
				buffer[j] << 24 |
				buffer[j + 1] << 16 |
				buffer[j + 2] << 8 |
				buffer[j + 3]
			);

			if (last_index < 0x80000000) {
				index %= (int32_t) last_index;
			}

			if (index < 0) {
				index += last_index;
			}

			--last_index;

			uint32_t temp = permutation[index];

			permutation[index] = permutation[last_index];
			permutation[last_index] = temp;
		}
	}
}

#define CHECK_COLOUR_SPACE(space) ((space) == JCS_YCbCr || (space) == JCS_GRAYSCALE)
#define BLOCK_LENGTH 64 // Must be != 0 and devisable by 8
#define COMPONENT_INDEX 0

static const size_t reversed_zig_zag[BLOCK_LENGTH - 1] = {
	1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40,
	48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29,
	22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47,
	55, 62, 63
};

LibEph5_result LibEph5_initialize(
	struct LibEph5_context *context,
	struct jpeg_decompress_struct *decompressor,
	const uint8_t *key
) {
	context->decompressor = decompressor;

	context->permutation = NULL;
	context->keystream = NULL;
	context->usable_coefficients = NULL;
	context->coefficients_payload = NULL;
	context->one_coefficients = NULL;
	context->modified_coefficients = NULL;

	// Read header

	jpeg_read_header(decompressor, true);

	if (!CHECK_COLOUR_SPACE(decompressor->jpeg_color_space)) {
		return LibEph5_result_invalid_colour_space;
	}

	if (decompressor->block_size * decompressor->block_size != BLOCK_LENGTH) {
		return LibEph5_result_invalid_block_size;
	}

	// Read coefficients

	struct jvirt_barray_control **coefficient_arrays = jpeg_read_coefficients(decompressor);

	context->coefficient_arrays = coefficient_arrays;

	// Get image properties

	jpeg_component_info *component = &decompressor->comp_info[COMPONENT_INDEX];

	context->component = component;

	JDIMENSION horizontal_blocks_count = (
		component->width_in_blocks + component->h_samp_factor - 1
	) / component->h_samp_factor * component->h_samp_factor; // LibJPEG ignores padding blocks as the standard requires

	JDIMENSION vertical_blocks_count = (
		component->height_in_blocks + component->v_samp_factor - 1
	) / component->v_samp_factor * component->v_samp_factor;

	if (
		vertical_blocks_count != 0 &&
		SIZE_MAX / horizontal_blocks_count / vertical_blocks_count / BLOCK_LENGTH == 0
	) {
		return LibEph5_result_too_big_image;
	}

	context->container_properties.horizontal_blocks_count = horizontal_blocks_count;
	context->container_properties.vertical_blocks_count = vertical_blocks_count;

	size_t coefficients_count = horizontal_blocks_count * vertical_blocks_count * BLOCK_LENGTH;

	context->container_properties.coefficients_count = coefficients_count;

	// Allocate arrays

	uint32_t *permutation = NULL;
	uint8_t *usable_coefficients = NULL;
	uint8_t *coefficients_payload = NULL;
	uint8_t *one_coefficients = NULL;
	uint8_t *modified_coefficients = NULL;

	if (coefficients_count != 0) {
		permutation = calloc(coefficients_count, sizeof *permutation);
		usable_coefficients = calloc(coefficients_count / 8, sizeof *usable_coefficients);
		coefficients_payload = calloc(coefficients_count / 8, sizeof *coefficients_payload);
		one_coefficients = calloc(coefficients_count / 8, sizeof *one_coefficients);
		modified_coefficients = calloc(coefficients_count / 8, sizeof *modified_coefficients);

		if (
			permutation == NULL ||
			usable_coefficients == NULL ||
			coefficients_payload == NULL ||
			one_coefficients == NULL ||
			modified_coefficients == NULL
		) {
			free(permutation);
			free(usable_coefficients);
			free(coefficients_payload);
			free(one_coefficients);
			free(modified_coefficients);

			return LibEph5_result_cant_allocate_memory;
		}
	}

	context->permutation = permutation;
	context->usable_coefficients = usable_coefficients;
	context->coefficients_payload = coefficients_payload;
	context->one_coefficients = one_coefficients;
	context->modified_coefficients = modified_coefficients;

	// Process coefficients

	struct jvirt_barray_control *coefficients = coefficient_arrays[COMPONENT_INDEX];

	size_t usable_coefficients_count = 0;
	size_t one_coefficients_count = 0;

	JBLOCKARRAY buffer;
	size_t i = 0;

	for (JDIMENSION y = 0; y < vertical_blocks_count; ++y) {
		buffer = decompressor->mem->access_virt_barray((struct jpeg_common_struct *) decompressor, coefficients, y, 1, false);

		for (size_t x = 0; x < horizontal_blocks_count; ++x) {
			++i;

			for (size_t c = 0; c < BLOCK_LENGTH - 1; ++c) {
				JCOEF coefficient = buffer[0][x][reversed_zig_zag[c]]; // Although they were in the right order

				bool is_usable = coefficient != 0;
				bool is_one = coefficient == -1 || coefficient == 1;

				usable_coefficients_count += is_usable;
				one_coefficients_count += is_one;

				bool payload = (coefficient % 2 != 0) == (coefficient > 0);

				usable_coefficients[i >> 3] |= is_usable << (i & 0x7);
				coefficients_payload[i >> 3] |= payload << (i & 0x7);
				one_coefficients[i >> 3] |= is_one << (i & 0x7);

				++i;
			}
		}
	}

	context->container_properties.usable_coefficients_count = usable_coefficients_count;
	context->container_properties.one_coefficients_count = one_coefficients_count;

	// Analyze container

	context->container_properties.guaranteed_capacity[0] = (usable_coefficients_count - one_coefficients_count) / 8;
	context->container_properties.maximum_capacity[0] = usable_coefficients_count / 8;
	context->container_properties.expected_capacity[0] = (usable_coefficients_count - one_coefficients_count / 2) / 8;

	context->container_properties.extractable_length[0] = context->container_properties.maximum_capacity[0];

	for (size_t i = 1; i < LIBEPH5_MAXIMUM_K; ++i) {
		int k = i + 1;
		int n = (1 << k) - 1;

		size_t guaranteed_capacity = (usable_coefficients_count - one_coefficients_count) / n * k / 8;
		size_t maximum_capacity = usable_coefficients_count / n * k / 8;
		size_t expected_capacity = 0;

		double ratio = (double) one_coefficients_count / usable_coefficients_count;

		if (ratio != 1.) {
			double m = n;
			double t = 1.;

			size_t magic_table_length = magic_tables_lengths[k - 2];
			const double *magic_table = magic_tables[k - 2];

			for (size_t j = 0; j < magic_table_length; ++j) {
				t *= magic_table[j] * ratio;
				m += t;
			}

			m += t * ratio / (1. - ratio);

			expected_capacity = usable_coefficients_count / m * k / 8.;
		}

		if (expected_capacity < guaranteed_capacity) {
			expected_capacity = guaranteed_capacity;
		}

		if (expected_capacity > maximum_capacity) {
			expected_capacity = maximum_capacity;
		}

		context->container_properties.guaranteed_capacity[i] = guaranteed_capacity;
		context->container_properties.maximum_capacity[i] = maximum_capacity;
		context->container_properties.expected_capacity[i] = expected_capacity;

		context->container_properties.extractable_length[i] = context->container_properties.maximum_capacity[i];
	}

	// Generate permutation and keystream

	struct arcfour_ctx cipher_context;

	arcfour_set_key(&cipher_context, LIBEPH5_KEY_LENGTH, key);

	generate_permutation(&cipher_context, coefficients_count, permutation);

	size_t keystream_length = context->container_properties.maximum_capacity[0];
	uint8_t *keystream = NULL;

	if (keystream_length != 0) {
		keystream = calloc(keystream_length, sizeof *keystream);

		if (keystream == NULL) {
			free(permutation);
			free(usable_coefficients);
			free(coefficients_payload);
			free(one_coefficients);
			free(modified_coefficients);

			return LibEph5_result_cant_allocate_memory;
		}
	}

	arcfour_crypt(&cipher_context, keystream_length, keystream, keystream);

	context->keystream = keystream;

	return LibEph5_result_OK;
}

void LibEph5_write(struct LibEph5_context *context, struct jpeg_compress_struct *compressor) {
	// Apply changes

	struct jpeg_decompress_struct *decompressor = context->decompressor;
	struct jvirt_barray_control *coefficients = context->coefficient_arrays[COMPONENT_INDEX];

	JDIMENSION horizontal_blocks_count = context->container_properties.horizontal_blocks_count;
	JDIMENSION vertical_blocks_count = context->container_properties.vertical_blocks_count;

	uint8_t *modified_coefficients = context->modified_coefficients;

	JBLOCKARRAY buffer;
	size_t i = 0;

	for (JDIMENSION y = 0; y < vertical_blocks_count; ++y) {
		buffer = decompressor->mem->access_virt_barray((struct jpeg_common_struct *) decompressor, coefficients, y, 1, true);

		for (size_t x = 0; x < horizontal_blocks_count; ++x) {
			++i;

			for (size_t c = 0; c < BLOCK_LENGTH - 1; ++c) {
				if ((modified_coefficients[i >> 3] >> (i & 0x7) & 1) != 0) {
					if (buffer[0][x][reversed_zig_zag[c]] > 0) {
						--buffer[0][x][reversed_zig_zag[c]];
					} else {
						++buffer[0][x][reversed_zig_zag[c]];
					}
				}

				++i;
			}
		}
	}

	// Write coefficients

	jpeg_copy_critical_parameters(decompressor, compressor);

	if (decompressor->progressive_mode) {
		jpeg_simple_progression(compressor);
	}

	compressor->optimize_coding = true;

	jpeg_write_coefficients(compressor, context->coefficient_arrays);

	// Fix dummy blocks

	jpeg_component_info *component = context->component;

	compressor->comp_info[COMPONENT_INDEX].width_in_blocks = (
		component->width_in_blocks + component->MCU_width - 1
	) / component->MCU_width * component->MCU_width;

	compressor->comp_info[COMPONENT_INDEX].height_in_blocks = (
		component->height_in_blocks + component->MCU_height - 1
	) / component->MCU_height * component->MCU_height;
}

void LibEph5_destroy(struct LibEph5_context *context) {
	free(context->permutation);
	free(context->keystream);
	free(context->usable_coefficients);
	free(context->coefficients_payload);
	free(context->one_coefficients);
	free(context->modified_coefficients);
}

size_t LibEph5_embed(struct LibEph5_context *context, size_t data_length, const uint8_t *data, int k) {
	size_t coefficients_count = context->container_properties.coefficients_count;

	uint32_t *permutation = context->permutation;
	uint8_t *keystream = context->keystream;
	uint8_t *usable_coefficients = context->usable_coefficients;
	uint8_t *coefficients_payload = context->coefficients_payload;
	uint8_t *one_coefficients = context->one_coefficients;
	uint8_t *modified_coefficients = context->modified_coefficients;

	size_t embedded_length = 0;
	size_t coefficients_index = 0;

	if (k == 1) {
		for (; embedded_length < data_length; ++embedded_length) {
			int byte = data[embedded_length] ^ keystream[embedded_length];

			for (int i = 0; i < 8; ++i) {
				int bit = byte >> i & 1;

				bool keep = true;

				while (keep) {
					if (coefficients_index == coefficients_count) {
						goto end;
					}

					size_t index = permutation[coefficients_index++];

					if (usable_coefficients[index >> 3] >> (index & 0x7) & 1) {
						keep = false;

						if ((coefficients_payload[index >> 3] >> (index & 0x7) & 1) != bit) {
							modified_coefficients[index >> 3] |= 1 << (index & 0x7);

							keep = one_coefficients[index >> 3] >> (index & 0x7) & 1;
						}
					}
				}
			}
		}
	} else {
		int n = (1 << k) - 1;

		size_t data_index = 0;

		int byte = 0;
		int l = 0;
		int e = 0;

		while (embedded_length < data_length) {
			if (l < k) {
				byte |= (data[data_index] ^ keystream[data_index]) << l;
				++data_index;
				l += data_index == data_length ? 7 + k : 8;
			}

			int bits = byte & n;

			int block_length = 0;
			size_t indexes[(1 << LIBEPH5_MAXIMUM_K) - 1];
			bool payloads[(1 << LIBEPH5_MAXIMUM_K) - 1];

			while (true) {
				for (; block_length < n; ++block_length) {
					size_t index;

					while (true) {
						if (coefficients_index == coefficients_count) {
							goto end;
						}

						index = permutation[coefficients_index++];

						if (usable_coefficients[index >> 3] >> (index & 0x7) & 1) {
							break;
						}
					}

					indexes[block_length] = index;
					payloads[block_length] = coefficients_payload[index >> 3] >> (index & 0x7) & 1;

					if (payloads[block_length]) {
						bits ^= block_length + 1;
					}
				}

				if (bits == 0) {
					break;
				}

				size_t index = indexes[bits - 1];

				modified_coefficients[index >> 3] |= 1 << (index & 0x7);

				if (one_coefficients[index >> 3] >> (index & 0x7) & 1) {
					int i = bits;

					if (payloads[bits - 1]) {
						bits = 0;
					}

					for (; i < block_length; ++i) {
						if (payloads[i]) {
							bits ^= i ^ i + 1;
						}

						indexes[i - 1] = indexes[i];
						payloads[i - 1] = payloads[i];
					}

					--block_length;
				} else {
					break;
				}
			}

			byte >>= k;
			l -= k;
			e += k;

			if (e >= 8) {
				embedded_length++;
				e -= 8;
			}
		}
	}

	end: {
		return embedded_length;
	}
}

void LibEph5_reset(struct LibEph5_context *context) {
	memset(context->modified_coefficients, 0, context->container_properties.coefficients_count / 8);
}

void LibEph5_extract(struct LibEph5_context *context, uint8_t **data) {
	size_t coefficients_count = context->container_properties.coefficients_count;

	uint32_t *permutation = context->permutation;
	uint8_t *keystream = context->keystream;
	uint8_t *usable_coefficients = context->usable_coefficients;
	uint8_t *coefficients_payload = context->coefficients_payload;

	size_t extracted_length = 0;
	int byte = 0;
	int bit_position = 0;

	size_t extracted_lengths[LIBEPH5_MAXIMUM_K - 1];
	int bytes[LIBEPH5_MAXIMUM_K - 1];
	int bit_positions[LIBEPH5_MAXIMUM_K - 1];
	int bits[LIBEPH5_MAXIMUM_K - 1];
	int bit_masks[LIBEPH5_MAXIMUM_K - 1];
	int ns[LIBEPH5_MAXIMUM_K - 1];

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K - 1; ++i) {
		extracted_lengths[i] = 0;
		bytes[i] = 0;
		bit_positions[i] = 0;
		bits[i] = 0;
		bit_masks[i] = 1;
		ns[i] = (1 << i + 2) - 1;
	}

	for (size_t i = 0; i < coefficients_count; ++i) {
		size_t index = permutation[i];

		if (!(usable_coefficients[index >> 3] >> (index & 0x7) & 1)) {
			continue;
		}

		int payload = coefficients_payload[index >> 3] >> (index & 0x7) & 1;

		byte |= payload << bit_position;
		++bit_position;

		if (bit_position == 8) {
			data[0][extracted_length] = byte ^ keystream[extracted_length];
			byte = 0;
			bit_position = 0;
			++extracted_length;
		}

		if (payload == 1) {
			for (size_t j = 0; j < LIBEPH5_MAXIMUM_K - 1; ++j) {
				bits[j] ^= bit_masks[j];
			}
		}

		for (size_t j = 0; j < LIBEPH5_MAXIMUM_K - 1; ++j) {
			++bit_masks[j];

			if (bit_masks[j] == ns[j] + 1) {
				bytes[j] |= bits[j] << bit_positions[j];
				bit_positions[j] += j + 2;

				if (bit_positions[j] >= 8) {
					data[j + 1][extracted_lengths[j]] = bytes[j] & 0xff ^ keystream[extracted_lengths[j]];
					bytes[j] >>= 8;
					bit_positions[j] -= 8;
					++extracted_lengths[j];
				}

				bits[j] = 0;
				bit_masks[j] = 1;
			}
		}
	}
}
