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

#define DEFAULT_KEY_LENGTH LIBEPH5_MAXIMUM_KEY_LENGTH

static const uint8_t default_key[DEFAULT_KEY_LENGTH] = {
	0x28, 0x4f, 0x33, 0x1e, 0x1c, 0xda, 0xcb, 0x8d, 0x64, 0xb9, 0xa0, 0x4f, 0x63, 0x6d, 0x03, 0xe6,
	0xc9, 0xd5, 0x3c, 0x45, 0x8e, 0x93, 0xc3, 0x22, 0xa0, 0xaa, 0xce, 0x24, 0x61, 0x51, 0x25, 0xb1,
	0xcc, 0x00, 0x79, 0xc7, 0x4b, 0x12, 0xdc, 0xc2, 0xb2, 0x80, 0x97, 0x90, 0x5d, 0x9b, 0xcf, 0x82,
	0x1b, 0x3e, 0xef, 0xbd, 0x9e, 0x1a, 0x9f, 0x4f, 0xf8, 0x5e, 0x18, 0x64, 0x75, 0xf7, 0x57, 0xfa,
	0x4a, 0x0e, 0x01, 0xa2, 0xa9, 0x8b, 0xea, 0x34, 0x8e, 0xf7, 0x95, 0xd0, 0xb3, 0xb9, 0xcc, 0x05,
	0xd2, 0x6d, 0x6c, 0xd4, 0x84, 0x66, 0x0c, 0x66, 0x26, 0x43, 0xf9, 0x65, 0x82, 0x4d, 0xef, 0x57,
	0x05, 0x8d, 0x95, 0x59, 0x10, 0xa3, 0xb7, 0x43, 0xe2, 0xa9, 0x57, 0x03, 0xe7, 0x66, 0x6a, 0x6a,
	0xee, 0x1a, 0x81, 0x65, 0xc2, 0xc5, 0xf3, 0x18, 0xcc, 0xf3, 0xc6, 0xcd, 0x90, 0x4c, 0xcf, 0x59,
	0x38, 0x9e, 0x78, 0x1a, 0x4a, 0xc7, 0xe2, 0x84, 0x5d, 0x57, 0x60, 0xab, 0x88, 0x8d, 0xef, 0xd3,
	0x46, 0x99, 0x86, 0x0c, 0xe4, 0xa3, 0x9a, 0x80, 0xed, 0xa0, 0x0a, 0x5f, 0xfd, 0x9b, 0x68, 0x97,
	0x1c, 0x2a, 0x11, 0xbc, 0x65, 0x2b, 0xa2, 0xd2, 0x14, 0xc1, 0x24, 0xde, 0x3f, 0x9d, 0x29, 0xba,
	0xd7, 0x14, 0x44, 0xab, 0xa8, 0x88, 0x0b, 0xc0, 0x18, 0x7c, 0x5f, 0x20, 0x28, 0x6e, 0xae, 0x97,
	0xb1, 0xd8, 0x86, 0x60, 0x3f, 0x5f, 0x37, 0x45, 0x6f, 0x1f, 0xfc, 0x76, 0x71, 0x35, 0x44, 0x88,
	0x30, 0x13, 0x2c, 0xd5, 0xb9, 0x47, 0xae, 0xd0, 0xab, 0xaa, 0xe5, 0x10, 0x6e, 0xc8, 0xc9, 0x85,
	0xd9, 0x38, 0x4f, 0x5f, 0xb2, 0x7a, 0x70, 0x17, 0x11, 0x3c, 0x6a, 0x98, 0x0c, 0xc5, 0x92, 0xa0,
	0x1c, 0xdb, 0xba, 0xc7, 0x70, 0x45, 0x97, 0xf6, 0x2a, 0x39, 0x7d, 0xda, 0x85, 0xdc, 0xbd, 0x88
};

static const size_t reversed_zig_zag[LIBEPH5_BLOCK_LENGTH - 1] = {
	1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40,
	48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29,
	22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47,
	55, 62, 63
};

#define COMPONENT_INDEX 0

LibEph5_result LibEph5_initialize(
	struct LibEph5_context *context,
	struct jpeg_common_struct *compressor,
	struct jvirt_barray_control **coefficient_arrays,
	size_t key_length,
	const uint8_t *key
) {
	if (key_length == 0) {
		key_length = DEFAULT_KEY_LENGTH;
		key = default_key;
	}

	// Read image properties

	context->container.compressor = compressor;
	context->container.coefficient_arrays = coefficient_arrays;

	jpeg_component_info *component;

	if (compressor->is_decompressor) {
		component = &((struct jpeg_decompress_struct *) compressor)->comp_info[COMPONENT_INDEX];
	} else {
		component = &((struct jpeg_compress_struct *) compressor)->comp_info[COMPONENT_INDEX];
	}

	struct jvirt_barray_control *coefficients = coefficient_arrays[COMPONENT_INDEX];

	context->container.component = component;
	context->container.coefficients = coefficients;

	JDIMENSION horizontal_blocks_count = (
		component->width_in_blocks + component->h_samp_factor - 1
	) / component->h_samp_factor * component->h_samp_factor; // LibJPEG ignores padding blocks as the standard requires

	JDIMENSION vertical_blocks_count = (
		component->height_in_blocks + component->v_samp_factor - 1
	) / component->v_samp_factor * component->v_samp_factor;

	if (
		vertical_blocks_count != 0 &&
		SIZE_MAX / horizontal_blocks_count / vertical_blocks_count / LIBEPH5_BLOCK_LENGTH == 0
	) {
		return LibEph5_result_too_big_image;
	}

	context->container.horizontal_blocks_count = horizontal_blocks_count;
	context->container.vertical_blocks_count = vertical_blocks_count;

	size_t coefficients_count = horizontal_blocks_count * vertical_blocks_count * LIBEPH5_BLOCK_LENGTH;

	context->container.coefficients_count = coefficients_count;

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
	context->keystream = NULL;
	context->usable_coefficients = usable_coefficients;
	context->coefficients_payload = coefficients_payload;
	context->one_coefficients = one_coefficients;
	context->modified_coefficients = modified_coefficients;

	// Read coefficients

	size_t usable_coefficients_count = 0;
	size_t one_coefficients_count = 0;

	JBLOCKARRAY buffer;
	size_t i = 0;

	for (JDIMENSION y = 0; y < vertical_blocks_count; ++y) {
		buffer = compressor->mem->access_virt_barray(compressor, coefficients, y, 1, false);

		for (size_t x = 0; x < horizontal_blocks_count; ++x) {
			++i;

			for (size_t c = 0; c < LIBEPH5_BLOCK_LENGTH - 1; ++c) {
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

	context->container.usable_coefficients_count = usable_coefficients_count;
	context->container.one_coefficients_count = one_coefficients_count;

	// Analyze container

	context->container.guaranteed_capacity[0] = (usable_coefficients_count - one_coefficients_count) / 8;
	context->container.maximum_capacity[0] = usable_coefficients_count / 8;
	context->container.expected_capacity[0] = (usable_coefficients_count - one_coefficients_count / 2) / 8;

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

		context->container.guaranteed_capacity[i] = guaranteed_capacity;
		context->container.maximum_capacity[i] = maximum_capacity;
		context->container.expected_capacity[i] = expected_capacity;
	}

	// Generate permutation and keystream

	struct arcfour_ctx cipher_context;

	arcfour_set_key(&cipher_context, key_length, key);

	generate_permutation(&cipher_context, coefficients_count, permutation);

	size_t keystream_length = context->container.maximum_capacity[0];
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

void LibEph5_apply_changes(struct LibEph5_context *context) {
	struct jpeg_common_struct *compressor = context->container.compressor;

	struct jvirt_barray_control *coefficients = context->container.coefficients;

	JDIMENSION horizontal_blocks_count = context->container.horizontal_blocks_count;
	JDIMENSION vertical_blocks_count = context->container.vertical_blocks_count;

	uint8_t *modified_coefficients = context->modified_coefficients;

	JBLOCKARRAY buffer;
	size_t i = 0;

	for (JDIMENSION y = 0; y < vertical_blocks_count; ++y) {
		buffer = compressor->mem->access_virt_barray(compressor, coefficients, y, 1, true);

		for (size_t x = 0; x < horizontal_blocks_count; ++x) {
			++i;

			for (size_t c = 0; c < LIBEPH5_BLOCK_LENGTH - 1; ++c) {
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
}

void LibEph5_fix_dummy_blocks(struct LibEph5_context *context, struct jpeg_compress_struct *compressor) {
	jpeg_component_info *component = context->container.component;

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
	size_t coefficients_count = context->container.coefficients_count;

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
	memset(context->modified_coefficients, 0, context->container.coefficients_count / 8);
}

size_t LibEph5_extract(struct LibEph5_context *context, size_t data_length, uint8_t *data, int k) {
	size_t coefficients_count = context->container.coefficients_count;

	uint32_t *permutation = context->permutation;
	uint8_t *keystream = context->keystream;
	uint8_t *usable_coefficients = context->usable_coefficients;
	uint8_t *coefficients_payload = context->coefficients_payload;

	size_t extracted_length = 0;
	size_t coefficients_index = 0;

	if (k == 1) {
		for (; extracted_length < data_length; ++extracted_length) {
			int byte = 0;

			for (size_t i = 0; i < 8; ++i) {
				bool bit;

				while (true) {
					if (coefficients_index == coefficients_count) {
						goto end;
					}

					size_t index = permutation[coefficients_index++];

					if (usable_coefficients[index >> 3] >> (index & 0x7) & 1) {
						bit = coefficients_payload[index >> 3] >> (index & 0x7) & 1;

						break;
					}
				}

				byte |= bit << i;
			}

			data[extracted_length] = byte ^ keystream[extracted_length];
		}
	} else {
		int n = (1 << k) - 1;

		int byte = 0;
		int l = 0;

		while (extracted_length < data_length) {
			int bits = 0;

			for (int i = 0; i < n; ++i) {
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

				if (coefficients_payload[index >> 3] >> (index & 0x7) & 1) {
					bits ^= i + 1;
				}
			}

			byte |= bits << l;
			l += k;

			if (l >= 8) {
				data[extracted_length] = byte & 0xff ^ keystream[extracted_length];
				byte >>= 8;
				l -= 8;
				++extracted_length;
			}
		}
	}

	end: {
		return extracted_length;
	}
}
