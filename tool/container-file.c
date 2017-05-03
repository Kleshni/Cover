#include <stdbool.h>
#include <setjmp.h>
#include <stdio.h>

#include <cover/container.h>
#include <jpeglib.h>

#include "container-file.h"

static void error_exit(j_common_ptr compressor) {
	char error_message[JMSG_LENGTH_MAX];

	compressor->err->format_message(compressor, error_message);

	fprintf(stderr, "LibJPEG error: %s\n", error_message);

	longjmp(compressor->client_data, 1);
}

static void emit_message(j_common_ptr compressor, int level) {
	if (level == -1) {
		char error_message[JMSG_LENGTH_MAX];

		compressor->err->format_message(compressor, error_message);

		fprintf(stderr, "LibJPEG warning: %s\n", error_message);
	}
}

bool container_file_initialize(struct container_file *context, const char *name) {
	FILE *file = fopen(name, "rb");

	if (file == NULL) {
		perror("LibC error");

		goto error_file;
	}

	jpeg_std_error(&context->error_manager);
	context->error_manager.error_exit = error_exit;
	context->error_manager.emit_message = emit_message;

	context->decompressor.err = &context->error_manager;
	context->decompressor.client_data = context->catch;

	if (setjmp(context->catch) != 0) {
		goto error_decompressor;
	}

	jpeg_create_decompress(&context->decompressor);
	jpeg_stdio_src(&context->decompressor, file);

	if (!Cover_container_read(&context->container, &context->decompressor)) {
		fputs("Incompatible image\n", stderr);

		goto error_container;
	}

	if (fclose(file) == EOF) {
		perror("LibC error");

		file = NULL;

		goto error_close;
	}

	file = NULL;

	return true;

	error_close: ;
	error_container: ;
	error_decompressor: jpeg_destroy_decompress(&context->decompressor);

	if (file != NULL && fclose(file) == EOF) {
		perror("LibC error");
	}

	error_file: ;

	return false;
}

void container_file_destroy(struct container_file *context) {
	jpeg_destroy_decompress(&context->decompressor);
}

bool container_file_write(struct container_file *context, const char *name) {
	bool result = false;

	FILE *file = fopen(name, "wb");

	if (file == NULL) {
		perror("LibC error");

		goto error_file;
	}

	struct jpeg_compress_struct compressor;

	compressor.err = &context->error_manager;
	compressor.client_data = context->catch;

	if (setjmp(context->catch) != 0) {
		goto error_compressor;
	}

	jpeg_create_compress(&compressor);
	jpeg_stdio_dest(&compressor, file);

	Cover_container_write(&context->container, &compressor);

	jpeg_finish_compress(&compressor);

	result = true;

	error_compressor: jpeg_destroy_compress(&compressor);

	if (fclose(file) == EOF) {
		perror("LibC error");

		result = false;
	}

	error_file: ;

	return result;
}
