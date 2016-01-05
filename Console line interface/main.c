#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>

#include <error.h>
#include <getopt.h>

#include <jpeglib.h>
#include <nettle/arcfour.h>
#include <nettle/pbkdf2.h>
#include <eph5.h>

typedef char *result;

static const result result_OK = NULL;

static jmp_buf catch;
static struct jpeg_error_mgr error_manager;
static char LibJPEG_error_message[JMSG_LENGTH_MAX];

static void error_exit(j_common_ptr compressor) {
	compressor->err->format_message(compressor, LibJPEG_error_message);
	longjmp(catch, 1);
}

static void emit_message(j_common_ptr compressor, int level) {
	if (level == -1) {
		compressor->err->format_message(compressor, LibJPEG_error_message);

		error(0, 0, "%s", LibJPEG_error_message);
	}
}

static const char default_password[] = "desu";

static result prepare_container(
	struct LibEph5_context *context,
	FILE **file,
	struct jpeg_decompress_struct *decompressor,
	const char *file_name,
	const char *password
) {
	// Set key

	uint8_t key[LIBEPH5_KEY_LENGTH];

	if (password == NULL) {
		password = default_password;
	}

	size_t password_length = strlen(password);

	pbkdf2_hmac_sha256(
		password_length, (uint8_t *) password,
		1000,
		password_length, (uint8_t *) password,
		LIBEPH5_KEY_LENGTH, key
	);

	// Open file

	*file = fopen(file_name, "rb");

	if (*file == NULL) {
		return strerror(errno);
	}

	// Set error manager

	decompressor->err = &error_manager;

	if (setjmp(catch)) {
		jpeg_destroy_decompress(decompressor);
		fclose(*file);

		return LibJPEG_error_message;
	}

	// Open container

	jpeg_create_decompress(decompressor);
	jpeg_stdio_src(decompressor, *file);

	// Initialize context

	if (setjmp(catch)) {
		LibEph5_destroy(context);
		jpeg_destroy_decompress(decompressor);
		fclose(*file);

		return LibJPEG_error_message;
	}

	LibEph5_result returned = LibEph5_initialize(context, decompressor, key);

	if (returned != LibEph5_result_OK) {
		jpeg_destroy_decompress(decompressor);
		fclose(*file);

		return returned;
	}

	return result_OK;
}

static result close_container(
	struct LibEph5_context *context,
	FILE **file,
	struct jpeg_decompress_struct *decompressor
) {
	LibEph5_destroy(context);

	jpeg_destroy_decompress(decompressor);

	if (fclose(*file) != 0) {
		return strerror(errno);
	}

	return result_OK;
}

static result write_result(struct LibEph5_context *context, const char *file_name) {
	// Open file

	FILE *file = fopen(file_name, "wb");

	if (file == NULL) {
		return strerror(errno);
	}

	// Set error manager

	struct jpeg_compress_struct compressor;

	compressor.err = &error_manager;

	if (setjmp(catch)) {
		jpeg_destroy_compress(&compressor);
		fclose(file);

		return LibJPEG_error_message;
	}

	// Write image

	jpeg_create_compress(&compressor);
	jpeg_stdio_dest(&compressor, file);
	LibEph5_write(context, &compressor);
	jpeg_finish_compress(&compressor);

	// Clean up

	jpeg_destroy_compress(&compressor);

	if (fclose(file) != 0) {
		return strerror(errno);
	}

	return result_OK;
}

static int analyze_main(int argc, char **argv) {
	// Parse command line

	opterr = 0;

	if (getopt(argc, argv, "") != -1) {
		error(1, 0, "Invalid option");
	}

	if (argc - optind != 1) {
		error(1, 0, "Missing or redundant file names");
	}

	const char *container_file_name = argv[optind];

	// Open container

	struct LibEph5_context context;
	FILE *container_file;
	struct jpeg_decompress_struct decompressor;

	result returned = prepare_container(&context, &container_file, &decompressor, container_file_name, NULL);

	if (returned != result_OK) {
		error(1, 0, "%s", returned);
	}

	// Print image properties

	printf("Coefficients: %zu\n", context.container_properties.coefficients_count);
	printf("Usable coefficients: %zu\n", context.container_properties.usable_coefficients_count);
	printf("One coefficients: %zu\n", context.container_properties.one_coefficients_count);
	printf("Guaranteed capacity in bytes (for different k):\n");

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K; ++i) {
		printf("%u. %zu\n", (unsigned int) i + 1, context.container_properties.guaranteed_capacity[i]);
	}

	printf("Maximum capacity:\n");

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K; ++i) {
		printf("%u. %zu\n", (unsigned int) i + 1, context.container_properties.maximum_capacity[i]);
	}

	printf("Expected capacity:\n");

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K; ++i) {
		printf("%u. %zu\n", (unsigned int) i + 1, context.container_properties.expected_capacity[i]);
	}

	printf("Extractable length:\n");

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K; ++i) {
		printf("%u. %zu\n", (unsigned int) i + 1, context.container_properties.extractable_length[i]);
	}

	// Close container

	returned = close_container(&context, &container_file, &decompressor);

	if (returned != result_OK) {
		error(1, 0, "%s", returned);
	}

	return EXIT_SUCCESS;
}

static int embed_main(int argc, char **argv) {
	int k = LIBEPH5_MAXIMUM_K;
	bool analyze = false;
	bool fit = false;
	const char *password = NULL;

	// Parse command line

	const char *short_options = "k:afp:";

	struct option long_options[] = {
		{"k", required_argument, NULL, 'k'},
		{"analyze", no_argument, NULL, 'a'},
		{"fit", no_argument, NULL, 'f'},
		{"password", required_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	opterr = 0;

	int option;

	while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch (option) {
			case 'k': {
				if (sscanf(optarg, "%i", &k) != 1 || k < 1 || k > LIBEPH5_MAXIMUM_K) {
					error(1, 0, "Invalid k");
				}

				analyze = false;
			} break;

			case 'a': {
				analyze = true;
			} break;

			case 'f': {
				fit = true;
			} break;

			case 'p': {
				password = optarg;
			} break;

			default: {
				error(1, 0, "Invalid option");
			}
		}
	}

	if (argc - optind != 3) {
		error(1, 0, "Missing or redundant file names");
	}

	const char *data_file_name = argv[optind];
	const char *container_file_name = argv[optind + 1];
	const char *result_file_name = argv[optind + 2];

	// Open container

	struct LibEph5_context context;
	FILE *container_file;
	struct jpeg_decompress_struct decompressor;

	result returned = prepare_container(&context, &container_file, &decompressor, container_file_name, password);

	if (returned != result_OK) {
		error(1, 0, "%s", returned);
	}

	// Read data

	size_t data_length = context.container_properties.maximum_capacity[(analyze || fit ? LIBEPH5_MAXIMUM_K : k) - 1];
	uint8_t *data = malloc(sizeof *data * data_length);

	if (data_length != 0 && data == NULL) {
		close_container(&context, &container_file, &decompressor);

		error(1, 0, "%s", strerror(errno));
	}

	FILE *data_file = fopen(data_file_name, "rb");

	if (data_file == NULL) {
		free(data);
		close_container(&context, &container_file, &decompressor);

		error(1, 0, "%s", strerror(errno));
	}

	data_length = fread(data, sizeof *data, data_length, data_file);

	if (fgetc(data_file) != EOF) {
		free(data);
		close_container(&context, &container_file, &decompressor);

		error(1, 0, "Container too small");
	}

	if (ferror(data_file) || fclose(data_file) != 0) {
		free(data);
		close_container(&context, &container_file, &decompressor);

		error(1, 0, "%s", strerror(errno));
	}

	// Embed

	if (analyze) {
		for (k = LIBEPH5_MAXIMUM_K; k > 1; --k) {
			if (context.container_properties.expected_capacity[k - 1] >= data_length) {
				break;
			}
		}
	}

	size_t embedded_length = 0;

	while (true) {
		embedded_length = LibEph5_embed(&context, data_length, data, k);

		if (fit && k > 1 && embedded_length < data_length) {
			--k;

			LibEph5_reset(&context);
		} else {
			break;
		}
	}

	free(data);

	if (analyze || fit) {
		printf("Used k: %i\n", k);
	}

	printf("Embedded bytes: %zu\n", embedded_length);

	if (embedded_length < data_length) {
		close_container(&context, &container_file, &decompressor);

		error(1, 0, "Container too small");
	}

	// Write result

	returned = write_result(&context, result_file_name);

	if (returned != result_OK) {
		close_container(&context, &container_file, &decompressor);

		error(1, 0, "%s", returned);
	}

	// Close container

	returned = close_container(&context, &container_file, &decompressor);

	if (returned != result_OK) {
		error(1, 0, "%s", returned);
	}

	return EXIT_SUCCESS;
}

static int extract_main(int argc, char **argv) {
	const char *password = NULL;

	// Parse command line

	const char *short_options = "p:";

	struct option long_options[] = {
		{"password", required_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	opterr = 0;

	int option;

	while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch (option) {
			case 'p': {
				password = optarg;
			} break;

			default: {
				error(1, 0, "Invalid option");
			}
		}
	}

	if (argc - optind != 8) {
		error(1, 0, "Missing or redundant file names");
	}

	const char *container_file_name = argv[optind];
	const char *data_file_names[LIBEPH5_MAXIMUM_K];

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K; ++i) {
		data_file_names[i] = argv[optind + 1 + i];
	}

	// Open container

	struct LibEph5_context context;
	FILE *container_file;
	struct jpeg_decompress_struct decompressor;

	result returned = prepare_container(&context, &container_file, &decompressor, container_file_name, password);

	if (returned != result_OK) {
		error(1, 0, "%s", returned);
	}

	// Extract

	size_t data_buffer_length = 0;

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K; ++i) {
		data_buffer_length += context.container_properties.extractable_length[i];
	}

	uint8_t *data_buffer = malloc(sizeof *data_buffer * data_buffer_length);

	if (data_buffer_length != 0 && data_buffer == NULL) {
		close_container(&context, &container_file, &decompressor);

		error(1, 0, "%s", strerror(errno));
	}

	uint8_t *data[LIBEPH5_MAXIMUM_K] = {data_buffer};

	for (size_t i = 1; i < LIBEPH5_MAXIMUM_K; ++i) {
		data[i] = data[i - 1] + context.container_properties.extractable_length[i - 1];
	}

	LibEph5_extract(&context, data);

	// Write data

	for (size_t i = 0; i < LIBEPH5_MAXIMUM_K; ++i) {
		FILE *data_file = fopen(data_file_names[i], "wb");

		if (data_file == NULL) {
			free(data_buffer);
			close_container(&context, &container_file, &decompressor);

			error(1, 0, "%s", strerror(errno));
		}

		fwrite(data[i], sizeof *data[i], context.container_properties.extractable_length[i], data_file);

		if (ferror(data_file) || fclose(data_file) != 0) {
			free(data_buffer);
			close_container(&context, &container_file, &decompressor);

			error(1, 0, "%s", strerror(errno));
		}
	}

	free(data_buffer);

	// Close container

	returned = close_container(&context, &container_file, &decompressor);

	if (returned != result_OK) {
		error(1, 0, "%s", returned);
	}

	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	// Initialize LibJPEG error manager

	jpeg_std_error(&error_manager);
	error_manager.error_exit = error_exit;
	error_manager.emit_message = emit_message;

	// Parse command line

	if (argc < 2) {
		error(1, 0, "Command not specified");
	}

	if (strcmp(argv[1], "analyze") == 0) {
		return analyze_main(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "embed") == 0) {
		return embed_main(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "extract") == 0) {
		return extract_main(argc - 1, argv + 1);
	} else {
		error(1, 0, "Unknown command");
	}
}
