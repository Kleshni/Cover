#include <stddef.h>
#include <stdint.h>

#include <cover/rang.h>

static uint_fast32_t gaussian_blur_tripple(uint_fast32_t left, uint_fast32_t central, uint_fast32_t right) {
	uint_fast32_t result = 0xff000000;

	for (size_t i = 0; i < 3; ++i) {
		result |= ((uint_fast32_t) 129 + (central & 0xff) * 255 + ((left & 0xff) + (right & 0xff)) * 2) / 259 << 8 * i;

		left >>= 8;
		central >>= 8;
		right >>= 8;
	}

	return result;
}

#define FOREGROUND_SHARE (2. / 255)
#define BACKGROUND_SHARE (1 - FOREGROUND_SHARE)

static uint_fast32_t merge_colours(uint_fast32_t background, uint_fast32_t foreground) {
	uint_fast32_t result = 0xff000000;

	for (size_t i = 0; i < 3; ++i) {
		result |= (uint_fast32_t) (
			(foreground & 0xff) * FOREGROUND_SHARE +
			(background & 0xff) * BACKGROUND_SHARE +
			.0001
		) << 8 * i;

		background >>= 8;
		foreground >>= 8;
	}

	return result;
}

void Cover_Rang_modify_image(
	size_t width,
	size_t height,
	uint32_t *data,
	uint32_t *blured_column,
	uint32_t *untouched_column
) {
	for (size_t x = 0; x < width; ++x) {
		uint_fast32_t previous = data[x];

		for (size_t y = 0; y < height; ++y) {
			uint_fast32_t current = data[width * y + x];
			uint_fast32_t next = data[width * (y == height - 1 ? height - 1 : y + 1) + x];

			uint_fast32_t blured = gaussian_blur_tripple(previous, current, next);

			data[width * y + x] = blured;

			if (x == 0) {
				blured_column[y] = blured;
			} else {
				uint_fast32_t previous_blured = data[width * y + x - 1];
				uint_fast32_t final = gaussian_blur_tripple(blured_column[y], previous_blured, blured);

				data[width * y + x - 1] = merge_colours(untouched_column[y], final);

				blured_column[y] = previous_blured;
			}

			untouched_column[y] = current;

			previous = current;
		}
	}

	for (size_t y = 0; y < height; ++y) {
		uint_fast32_t previous_blured = data[width * y + width - 1];
		uint_fast32_t final = gaussian_blur_tripple(blured_column[y], previous_blured, previous_blured);

		data[width * y + width - 1] = merge_colours(untouched_column[y], final);
	}
}
