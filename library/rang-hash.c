#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cover/rang.h>
#include <nettle/salsa20.h>

void Cover_Rang_hash(
	size_t length,
	uint8_t *hash,
	const struct salsa20_ctx *context,
	uint_fast32_t start,
	uint_fast32_t count,
	const uint8_t *bits
) {
	for (size_t i = 0; i < count; ++i) {
		uint_fast32_t index = start + i;

		if ((bits[index / 8] >> index % 8 & 1) == 1) {
			Cover_Rang_xor_string(length, hash, context, index);
		}
	}
}

static size_t LUP_decompose(size_t length, size_t height, uint8_t **matrix, uint32_t *indexes, size_t i) {
	size_t width = 8 * length;

	for (; i < width; ++i) {
		size_t index = i / 8;
		size_t shift = i % 8;

		uint8_t *pivot;

		size_t j = i;

		for (; j < height; ++j) {
			if ((matrix[j][index] >> shift & 1) == 1) {
				pivot = matrix[j];
				matrix[j] = matrix[i];
				matrix[i] = pivot;

				uint_fast32_t temp = indexes[j];

				indexes[j] = indexes[i];
				indexes[i] = temp;

				break;
			}
		}

		if (j == height) {
			break;
		}

		for (++j; j < height; ++j) {
			uint8_t *row = matrix[j];

			if ((row[index] >> shift & 1) == 1) {
				row[index] ^= pivot[index] & 0xfe << shift;

				for (size_t k = index + 1; k < length; ++k) {
					row[k] ^= pivot[k];
				}
			}
		}
	}

	return i;
}

static bool LUP_adapt_row(size_t length, uint8_t **matrix, size_t i) {
	uint8_t *row = matrix[i];

	for (size_t j = 0; j < i; ++j) {
		size_t index = j / 8;
		size_t shift = j % 8;

		if ((row[index] >> shift & 1) == 1) {
			uint8_t *pivot = matrix[j];

			row[index] ^= pivot[index] & 0xfe << shift;

			for (size_t k = index + 1; k < length; ++k) {
				row[k] ^= pivot[k];
			}
		}
	}

	return (row[i / 8] >> i % 8 & 1) == 1;
}

static void LUP_divide_vector(size_t length, uint8_t *vector, uint8_t **matrix) {
	size_t width = 8 * length;

	for (size_t i = 0; i < width; ++i) {
		size_t index = i / 8;
		size_t shift = i % 8;

		if ((vector[index] >> shift & 1) == 1) {
			const uint8_t *row = matrix[i];

			vector[index] ^= row[index] & 0xfe << shift;

			for (size_t j = index + 1; j < length; ++j) {
				vector[j] ^= row[j];
			}
		}
	}

	for (size_t i = width; i > 0; ) {
		--i;

		size_t index = i / 8;
		size_t shift = i % 8;

		if ((vector[index] >> shift & 1) == 1) {
			const uint8_t *row = matrix[i];

			for (size_t j = 0; j < index; ++j) {
				vector[j] ^= row[j];
			}

			vector[index] ^= row[index] & 0xff >> 8 - shift;
		}
	}
}

bool Cover_Rang_unhash(
	size_t length,
	size_t *padding_bits_count,
	uint8_t *vector,
	bool full_padding,
	uint8_t **matrix,
	const struct salsa20_ctx *context,
	uint32_t *indexes
) {
	size_t width = 8 * length;

	for (size_t i = 0; i < width; ++i) {
		memset(matrix[i], 0, length);
		Cover_Rang_xor_string(length, matrix[i], context, indexes[i]);
	}

	size_t decomposed_height = LUP_decompose(length, width, matrix, indexes, 0);
	size_t added_count = 0;

	while (decomposed_height != width) {
		bool found = false;

		for (; added_count < *padding_bits_count && !found; ++added_count) {
			uint8_t *temp = matrix[decomposed_height];

			memset(matrix[width + added_count], 0, length);
			Cover_Rang_xor_string(length, matrix[width + added_count], context, indexes[width + added_count]);

			matrix[decomposed_height] = matrix[width + added_count];
			matrix[width + added_count] = temp;

			found = LUP_adapt_row(length, matrix, decomposed_height);

			if (found) {
				uint_fast32_t temp = indexes[decomposed_height];

				indexes[decomposed_height] = indexes[width + added_count];
				indexes[width + added_count] = temp;
			} else {
				matrix[width + added_count] = matrix[decomposed_height];
				matrix[decomposed_height] = temp;
			}
		}

		if (!found) {
			return false;
		}

		decomposed_height = LUP_decompose(length, width + added_count, matrix, indexes, decomposed_height);
	}

	if (!full_padding) {
		*padding_bits_count = added_count;
	}

	for (size_t i = 0; i < *padding_bits_count; ++i) {
		if ((vector[length + i / 8] >> i % 8 & 1) == 1) {
			Cover_Rang_xor_string(length, vector, context, indexes[width + i]);
		}
	}

	LUP_divide_vector(length, vector, matrix);

	return true;
}
