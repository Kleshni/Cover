#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <setjmp.h>
#include <stdbool.h>

#include <jpeglib.h>
#include <nettle/arcfour.h>
#include <nettle/pbkdf2.h>
#include <eph5.h>

typedef char *result;

const result result_OK = NULL;

static jmp_buf catch;
static struct jpeg_error_mgr error_manager;
static char LibJPEG_error_message[JMSG_LENGTH_MAX];

static void error_exit(j_common_ptr compressor) {
	compressor->err->format_message(compressor, LibJPEG_error_message);

	longjmp(catch, 1);
}

static void emit_message(j_common_ptr compressor, int level) {}

int main(int argc, char **argv) {
	// Initialize LibJPEG error manager

	jpeg_std_error(&error_manager);
	error_manager.error_exit = error_exit;
	error_manager.emit_message = emit_message;

	return EXIT_SUCCESS;
}

uint8_t key[LIBEPH5_KEY_LENGTH];

size_t data_length;
uint8_t *data;

size_t horizontal_blocks_count;
size_t vertical_blocks_count;
size_t coefficients_count;
size_t usable_coefficients_count;
size_t one_coefficients_count;
size_t *guaranteed_capacity;
size_t *maximum_capacity;
size_t *expected_capacity;

void *globals[17] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	key,
	&data_length,
	&data,
	&horizontal_blocks_count,
	&vertical_blocks_count,
	&coefficients_count,
	&usable_coefficients_count,
	&one_coefficients_count,
	&guaranteed_capacity,
	&maximum_capacity,
	&expected_capacity
};

void *export_globals(void) {
	globals[0] = result_OK;
	globals[1] = LibEph5_result_invalid_colour_space;
	globals[2] = LibEph5_result_invalid_block_size;
	globals[3] = LibEph5_result_cant_allocate_memory;
	globals[4] = LibEph5_result_too_big_image;

	return globals;
}

static struct jpeg_decompress_struct decompressor;

static struct LibEph5_context context;

result load(void) {
	// Set error manager

	decompressor.err = &error_manager;

	if (setjmp(catch)) {
		jpeg_destroy_decompress(&decompressor);

		return LibJPEG_error_message;
	}

	// Open container

	jpeg_create_decompress(&decompressor);
	jpeg_mem_src(&decompressor, (unsigned char *) data, data_length);

	// Initialize context

	if (setjmp(catch)) {
		LibEph5_destroy(&context);
		jpeg_destroy_decompress(&decompressor);

		return LibJPEG_error_message;
	}

	LibEph5_result returned = LibEph5_initialize(&context, &decompressor, key);

	if (returned != LibEph5_result_OK) {
		jpeg_destroy_decompress(&decompressor);

		return returned;
	}

	// Share container properties

	horizontal_blocks_count = context.container_properties.horizontal_blocks_count;
	vertical_blocks_count = context.container_properties.vertical_blocks_count;
	coefficients_count = context.container_properties.coefficients_count;
	usable_coefficients_count = context.container_properties.usable_coefficients_count;
	one_coefficients_count = context.container_properties.one_coefficients_count;
	guaranteed_capacity = context.container_properties.guaranteed_capacity;
	maximum_capacity = context.container_properties.maximum_capacity;
	expected_capacity = context.container_properties.expected_capacity;

	return result_OK;
}

result save(void) {
	// Set error manager

	struct jpeg_compress_struct compressor;

	compressor.err = &error_manager;

	if (setjmp(catch)) {
		jpeg_destroy_compress(&compressor);

		return LibJPEG_error_message;
	}

	// Initialize compression structure

	unsigned long new_data_length = data_length;

	jpeg_create_compress(&compressor);
	jpeg_mem_dest(&compressor, &data, &new_data_length);
	LibEph5_write(&context, &compressor);
	jpeg_finish_compress(&compressor);

	data_length = new_data_length;

	// Clean up

	jpeg_destroy_compress(&compressor);

	return result_OK;
}

void destroy(void) {
	LibEph5_destroy(&context);

	jpeg_destroy_decompress(&decompressor);
}

size_t embed(int k) {
	return LibEph5_embed(&context, data_length, data, k);
}

void reset(void) {
	LibEph5_reset(&context);
}

size_t extract(int k) {
	return LibEph5_extract(&context, data_length, data, k);
}
