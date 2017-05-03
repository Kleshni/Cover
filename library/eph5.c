#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <cover/eph5.h>
#include <cover/container.h>
#include <jpeglib.h>
#include <nettle/pbkdf2.h>
#include <nettle/arcfour.h>

#include "eph5-tables.h"

void Cover_Eph5_expand_password(uint8_t *key, const char *password) {
	size_t length = strlen(password);

	pbkdf2_hmac_sha256(
		length, (const uint8_t *) password,
		1000,
		length, (const uint8_t *) password,
		COVER_EPH5_KEY_LENGTH, key
	);
}

static const size_t reversed_zig_zag[COVER_CONTAINER_BLOCK_LENGTH] = {
	0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40,
	48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29,
	22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47,
	55, 62, 63
};

static void decode_coefficients(struct Cover_Eph5 *context) {
	struct jpeg_decompress_struct *decompressor = context->image->decompressor;
	struct jvirt_barray_control *coefficients = context->image->coefficients;

	size_t width_in_blocks = context->image->width_in_blocks;
	size_t height_in_blocks = context->image->height_in_blocks;

	size_t usable_count = 0;
	size_t one_count = 0;

	uint8_t *payload = context->payload;
	uint8_t *usable = context->usable;
	uint8_t *one = context->one;

	size_t i = 0;

	for (JDIMENSION y = 0; y < height_in_blocks; ++y) {
		JBLOCKARRAY buffer = decompressor->mem->access_virt_barray(
			(struct jpeg_common_struct *) decompressor,
			coefficients, y, 1, false
		);

		for (size_t x = 0; x < width_in_blocks; ++x) {
			++i;

			for (size_t c = 1; c < COVER_CONTAINER_BLOCK_LENGTH; ++c) {
				JCOEF coefficient = buffer[0][x][reversed_zig_zag[c]]; // Although they were in the right order

				bool payload_bit = coefficient % 2 != 0 == coefficient >= 0;
				bool is_usable = coefficient != 0;
				bool is_one = coefficient == -1 || coefficient == 1;

				usable_count += is_usable;
				one_count += is_one;

				payload[i / 8] |= payload_bit << i % 8;
				usable[i / 8] |= is_usable << i % 8;
				one[i / 8] |= is_one << i % 8;

				++i;
			}
		}
	}

	context->usable_count = usable_count;
	context->one_count = one_count;
}

static void analyze(struct Cover_Eph5 *context) {
	context->guaranteed_capacity[0] = (context->usable_count - context->one_count) / 8;
	context->maximum_capacity[0] = context->usable_count / 8;
	context->expected_capacity[0] = (context->usable_count - context->one_count / 2) / 8;
	context->extractable_length[0] = context->maximum_capacity[0];

	for (size_t i = 1; i < COVER_EPH5_MAXIMUM_K; ++i) {
		int k = i + 1;
		int n = (1 << k) - 1;

		size_t guaranteed_capacity = (context->usable_count - context->one_count) / n * k / 8;
		size_t maximum_capacity = context->usable_count / n * k / 8;
		double expected_capacity = 0;

		double ratio = (double) context->one_count / context->usable_count;

		if (ratio != 1) {
			double m = n;
			double t = 1;

			size_t Eph5_magic_table_length = Eph5_magic_tables_lengths[k - 2];
			const double *Eph5_magic_table = Eph5_magic_tables[k - 2];

			for (size_t j = 0; j < Eph5_magic_table_length; ++j) {
				t *= Eph5_magic_table[j] * ratio;
				m += t;
			}

			m += t * ratio / (1 - ratio);

			expected_capacity = context->usable_count / m * k / 8;
		}

		if (expected_capacity < guaranteed_capacity) {
			expected_capacity = guaranteed_capacity;
		}

		if (expected_capacity > maximum_capacity) {
			expected_capacity = maximum_capacity;
		}

		context->guaranteed_capacity[i] = guaranteed_capacity;
		context->maximum_capacity[i] = maximum_capacity;
		context->expected_capacity[i] = expected_capacity;
		context->extractable_length[i] = maximum_capacity;
	}
}

#define PERMUTATION_BUFFER_LENGTH 8192

static const uint8_t zero_permutation_buffer[PERMUTATION_BUFFER_LENGTH];

static void generate_permutation(struct arcfour_ctx *context, size_t count, uint32_t *permutation) {
	for (size_t i = 0; i < count; ++i) {
		permutation[i] = i;
	}

	uint8_t buffer[PERMUTATION_BUFFER_LENGTH];
	size_t last_index = count;

	for (size_t i = 0; i < count; i += PERMUTATION_BUFFER_LENGTH / 4) {
		size_t buffer_length = PERMUTATION_BUFFER_LENGTH;

		if (count - i < PERMUTATION_BUFFER_LENGTH / 4) {
			buffer_length = (count - i) * 4;
		}

		arcfour_crypt(context, buffer_length, buffer, zero_permutation_buffer);

		for (size_t j = 0; j < buffer_length; j += 4) {
			uint_fast32_t index = (
				(uint_fast32_t) buffer[j] << 24 |
				(uint_fast32_t) buffer[j + 1] << 16 |
				(uint_fast32_t) buffer[j + 2] << 8 |
				(uint_fast32_t) buffer[j + 3]
			);

			if ((index >> 31 & 1) == 1) {
				index = index ^ 0xffffffff;
				index %= last_index;
				index = last_index - 1 - index;
			} else {
				index %= last_index;
			}

			--last_index;

			uint_fast32_t temp = permutation[index];

			permutation[index] = permutation[last_index];
			permutation[last_index] = temp;
		}
	}
}

bool Cover_Eph5_initialize(
	struct Cover_Eph5 *context,
	struct Cover_container *image,
	const uint8_t *key,
	bool writable
) {
	context->image = image;

	context->bit_array_length = image->coefficients_count / 8;

	context->payload = calloc(context->bit_array_length == 0 ? 1 : context->bit_array_length, 1);

	if (context->payload == NULL) {
		goto error_payload;
	}

	context->usable = calloc(context->bit_array_length == 0 ? 1 : context->bit_array_length, 1);

	if (context->usable == NULL) {
		goto error_usable;
	}

	context->one = calloc(context->bit_array_length == 0 ? 1 : context->bit_array_length, 1);

	if (context->one == NULL) {
		goto error_one;
	}

	context->changes = NULL;

	if (writable) {
		context->changes = malloc(context->bit_array_length == 0 ? 1 : context->bit_array_length);

		if (context->changes == NULL) {
			goto error_changes;
		}
	}

	if (SIZE_MAX / 4 < image->coefficients_count) {
		goto error_permutation;
	}

	context->permutation = malloc(4 * (image->coefficients_count == 0 ? 1 : image->coefficients_count));

	if (context->permutation == NULL) {
		goto error_permutation;
	}

	context->keystream = NULL;

	decode_coefficients(context);

	analyze(context);

	context->keystream = calloc(context->usable_count == 0 ? 1 : context->usable_count / 8, 1);

	if (context->keystream == NULL) {
		goto error_keystream;
	}

	struct arcfour_ctx cipher;

	arcfour_set_key(&cipher, COVER_EPH5_KEY_LENGTH, key);

	generate_permutation(&cipher, image->coefficients_count, context->permutation);

	arcfour_crypt(&cipher, context->usable_count / 8, context->keystream, context->keystream);

	return true;

	free(context->keystream);
	error_keystream: free(context->permutation);
	error_permutation: free(context->changes);
	error_changes: free(context->one);
	error_one: free(context->usable);
	error_usable: free(context->payload);
	error_payload: ;

	return false;
}

void Cover_Eph5_destroy(struct Cover_Eph5 *context) {
	free(context->keystream);
	free(context->permutation);
	free(context->changes);
	free(context->one);
	free(context->usable);
	free(context->payload);
}

size_t Cover_Eph5_apply(struct Cover_Eph5 *context, size_t *zeroed_count) {
	struct jpeg_decompress_struct *decompressor = context->image->decompressor;
	struct jvirt_barray_control *coefficients = context->image->coefficients;

	size_t width_in_blocks = context->image->width_in_blocks;
	size_t height_in_blocks = context->image->height_in_blocks;

	uint8_t *changes = context->changes;

	*zeroed_count = 0;

	size_t changed_count = 0;
	size_t i = 0;

	for (JDIMENSION y = 0; y < height_in_blocks; ++y) {
		JBLOCKARRAY buffer = decompressor->mem->access_virt_barray(
			(struct jpeg_common_struct *) decompressor,
			coefficients, y, 1, true
		);

		for (size_t x = 0; x < width_in_blocks; ++x) {
			++i;

			for (size_t c = 1; c < COVER_CONTAINER_BLOCK_LENGTH; ++c) {
				if ((changes[i / 8] >> i % 8 & 1) == 1) {
					JCOEF *coefficient = &buffer[0][x][reversed_zig_zag[c]];

					if (*coefficient > 0) {
						--*coefficient;
					} else {
						++*coefficient;
					}

					*zeroed_count += *coefficient == 0;

					++changed_count;
				}

				++i;
			}
		}
	}

	return changed_count;
}

void Cover_Eph5_extract(struct Cover_Eph5 *context, uint8_t **data) {
	size_t extracted_length = 0;
	int byte = 0;
	int bit_position = 0;

	size_t extracted_lengths[COVER_EPH5_MAXIMUM_K - 1] = {0};
	int bytes[COVER_EPH5_MAXIMUM_K - 1] = {0};
	int bit_positions[COVER_EPH5_MAXIMUM_K - 1] = {0};
	int bits[COVER_EPH5_MAXIMUM_K - 1] = {0};
	int bit_masks[COVER_EPH5_MAXIMUM_K - 1] = {0};

	for (size_t i = 0; i < context->image->coefficients_count; ++i) {
		size_t index = context->permutation[i];

		if ((context->usable[index / 8] >> index % 8 & 1) == 0) {
			continue;
		}

		int payload_bit = context->payload[index / 8] >> index % 8 & 1;

		byte |= payload_bit << bit_position;
		++bit_position;

		if (bit_position == 8) {
			data[0][extracted_length] = byte ^ context->keystream[extracted_length];
			byte = 0;
			bit_position = 0;
			++extracted_length;
		}

		for (size_t j = 0; j < COVER_EPH5_MAXIMUM_K - 1; ++j) {
			++bit_masks[j];

			if (payload_bit == 1) {
				bits[j] ^= bit_masks[j];
			}

			if (bit_masks[j] == (4 << j) - 1) {
				bytes[j] |= bits[j] << bit_positions[j];
				bit_positions[j] += j + 2;

				if (bit_positions[j] >= 8) {
					data[j + 1][extracted_lengths[j]] = bytes[j] & 0xff ^ context->keystream[extracted_lengths[j]];
					bytes[j] >>= 8;
					bit_positions[j] -= 8;
					++extracted_lengths[j];
				}

				bits[j] = 0;
				bit_masks[j] = 0;
			}
		}
	}
}

size_t Cover_Eph5_embed(struct Cover_Eph5 *context, size_t length, const uint8_t *data, int k) {
	memset(context->changes, 0, context->bit_array_length);

	size_t embedded_length = 0;

	size_t coefficient_index = 0;

	if (k == 1) {
		for (; embedded_length < length; ++embedded_length) {
			int byte = data[embedded_length] ^ context->keystream[embedded_length];

			for (int i = 0; i < 8; ++i) {
				int bit = byte >> i & 1;

				bool keep = true;

				while (keep) {
					if (coefficient_index == context->image->coefficients_count) {
						goto out_1;
					}

					size_t index = context->permutation[coefficient_index];

					++coefficient_index;

					if ((context->usable[index / 8] >> index % 8 & 1) == 1) {
						keep = false;

						if ((context->payload[index / 8] >> index % 8 & 1) != bit) {
							context->changes[index / 8] |= 1 << index % 8;

							keep = context->one[index / 8] >> index % 8 & 1;
						}
					}
				}
			}
		}

		out_1: ;
	} else {
		int n = (1 << k) - 1;

		size_t data_index = 0;

		int byte = 0;
		int l = 0;
		int e = 0;

		while (embedded_length < length) {
			if (l < k) {
				byte |= (data[data_index] ^ context->keystream[data_index]) << l;
				++data_index;

				l += data_index == length ? 7 + k : 8;
			}

			int bits = byte & n;

			int block_length = 0;
			size_t indexes[(1 << COVER_EPH5_MAXIMUM_K) - 1];
			bool payload_bits[(1 << COVER_EPH5_MAXIMUM_K) - 1];

			while (true) {
				for (; block_length < n; ++block_length) {
					size_t index;

					while (true) {
						if (coefficient_index == context->image->coefficients_count) {
							goto out_high;
						}

						index = context->permutation[coefficient_index];
						++coefficient_index;

						if ((context->usable[index / 8] >> index % 8 & 1) == 1) {
							break;
						}
					}

					indexes[block_length] = index;
					payload_bits[block_length] = context->payload[index / 8] >> index % 8 & 1;

					if (payload_bits[block_length]) {
						bits ^= block_length + 1;
					}
				}

				if (bits == 0) {
					break;
				}

				size_t index = indexes[bits - 1];

				context->changes[index / 8] |= 1 << index % 8;

				if ((context->one[index / 8] >> index % 8 & 1) == 1) {
					int i = bits;

					if (payload_bits[bits - 1]) {
						bits = 0;
					}

					for (; i < block_length; ++i) {
						if (payload_bits[i]) {
							bits ^= i ^ i + 1;
						}

						indexes[i - 1] = indexes[i];
						payload_bits[i - 1] = payload_bits[i];
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
				++embedded_length;
				e -= 8;
			}
		}

		out_high: ;
	}

	return embedded_length;
}
