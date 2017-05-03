#ifndef CONTAINER_FILE_H
	#define CONTAINER_FILE_H

	#include <stdbool.h>
	#include <setjmp.h>
	#include <stdio.h>

	#include <cover/container.h>
	#include <jpeglib.h>

	struct container_file {
		struct Cover_container container;

		jmp_buf catch;

		struct jpeg_error_mgr error_manager;
		struct jpeg_decompress_struct decompressor;
	};

	bool container_file_initialize(struct container_file *context, const char *name);
	void container_file_destroy(struct container_file *context);
	bool container_file_write(struct container_file *context, const char *name);
#endif
