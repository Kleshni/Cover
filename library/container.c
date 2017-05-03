#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <cover/container.h>
#include <jpeglib.h>

bool Cover_container_read(struct Cover_container *context, struct jpeg_decompress_struct *decompressor) {
	context->decompressor = decompressor;

	jpeg_read_header(decompressor, true);

	if (decompressor->jpeg_color_space != JCS_YCbCr && decompressor->jpeg_color_space != JCS_GRAYSCALE) {
		return false;
	}

	if (decompressor->block_size * decompressor->block_size != COVER_CONTAINER_BLOCK_LENGTH) {
		return false;
	}

	context->coefficients_arrays = jpeg_read_coefficients(decompressor);

	jpeg_component_info *component = &decompressor->comp_info[COVER_CONTAINER_COMPONENT_INDEX];

	size_t width_in_blocks = (
		component->width_in_blocks + component->h_samp_factor - 1
	) / component->h_samp_factor * component->h_samp_factor; // LibJPEG ignores padding blocks as the standard requires

	size_t height_in_blocks = (
		component->height_in_blocks + component->v_samp_factor - 1
	) / component->v_samp_factor * component->v_samp_factor;

	if (height_in_blocks != 0 && SIZE_MAX / width_in_blocks / height_in_blocks < COVER_CONTAINER_BLOCK_LENGTH) {
		return false;
	}

	context->width_in_blocks = width_in_blocks;
	context->height_in_blocks = height_in_blocks;
	context->coefficients_count = width_in_blocks * height_in_blocks * COVER_CONTAINER_BLOCK_LENGTH;

	context->coefficients = context->coefficients_arrays[COVER_CONTAINER_COMPONENT_INDEX];

	return true;
}

void Cover_container_write(struct Cover_container *context, struct jpeg_compress_struct *compressor) {
	jpeg_copy_critical_parameters(context->decompressor, compressor);

	if (context->decompressor->progressive_mode) {
		jpeg_simple_progression(compressor);
	}

	compressor->optimize_coding = true;

	jpeg_write_coefficients(compressor, context->coefficients_arrays);

	// Fix dummy blocks

	jpeg_component_info *component = &context->decompressor->comp_info[COVER_CONTAINER_COMPONENT_INDEX];

	compressor->comp_info[COVER_CONTAINER_COMPONENT_INDEX].width_in_blocks = (
		component->width_in_blocks + component->MCU_width - 1
	) / component->MCU_width * component->MCU_width;

	compressor->comp_info[COVER_CONTAINER_COMPONENT_INDEX].height_in_blocks = (
		component->height_in_blocks + component->MCU_height - 1
	) / component->MCU_height * component->MCU_height;
}
