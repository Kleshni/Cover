#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>

#include <cover/container.h>
#include <cover/eph5.h>

#include "main.h"
#include "file.h"
#include "container-file.h"

static const char default_password[] = "desu";

static int main_extract(int argc, char **argv) {
	int result = EXIT_FAILURE;

	const char *password = default_password;

	const char *short_options = "p:";

	struct option long_options[] = {
		{"password", required_argument, NULL, 'p'},
		{0}
	};

	opterr = 0;

	while (true) {
		int option = getopt_long(argc, argv, short_options, long_options, NULL);

		if (option == -1) {
			break;
		} else if (option == 'p') {
			password = optarg;
		} else {
			fputs("Wrong option\n", stderr);

			goto error_command_line;
		}
	}

	if (argc - optind != 8) {
		fputs("Wrong number of file arguments\n", stderr);

		goto error_command_line;
	}

	struct container_file image;

	if (!container_file_initialize(&image, argv[optind])) {
		fputs("Can't read image\n", stderr);

		goto error_image;
	}

	struct Cover_container *container = &image.container;

	printf("Width in blocks: %zu\n", container->width_in_blocks);
	printf("Height in blocks: %zu\n", container->height_in_blocks);
	printf("Coefficients: %zu\n", container->coefficients_count);

	uint8_t key[COVER_EPH5_KEY_LENGTH];

	Cover_Eph5_expand_password(key, password);

	if (setjmp(image.catch) != 0) {
		fputs("Can't read coefficients\n", stderr);

		goto error_coefficients;
	}

	struct Cover_Eph5 Eph5;

	if (!Cover_Eph5_initialize(&Eph5, container, key, false)) {
		fputs("Can't allocate memory\n", stderr);

		goto error_Eph5;
	}

	printf("Usable coefficients: %zu\n", Eph5.usable_count);
	printf("One coefficients: %zu\n", Eph5.one_count);
	printf("Guaranteed capacity in bytes (for different k):\n");

	for (size_t i = 0; i < COVER_EPH5_MAXIMUM_K; ++i) {
		printf("%zu. %zu\n", i + 1, Eph5.guaranteed_capacity[i]);
	}

	printf("Maximum capacity:\n");

	for (size_t i = 0; i < COVER_EPH5_MAXIMUM_K; ++i) {
		printf("%zu. %zu\n", i + 1, Eph5.maximum_capacity[i]);
	}

	printf("Expected capacity:\n");

	for (size_t i = 0; i < COVER_EPH5_MAXIMUM_K; ++i) {
		printf("%zu. %zu\n", i + 1, Eph5.expected_capacity[i]);
	}

	printf("Extractable length:\n");

	for (size_t i = 0; i < COVER_EPH5_MAXIMUM_K; ++i) {
		printf("%zu. %zu\n", i + 1, Eph5.extractable_length[i]);
	}

	uint8_t *data[COVER_EPH5_MAXIMUM_K] = {0};

	for (size_t i = 0; i < COVER_EPH5_MAXIMUM_K; ++i) {
		data[i] = malloc(Eph5.extractable_length[i] == 0 ? 1 : Eph5.extractable_length[i]);

		if (data[i] == NULL) {
			perror("LibC error");

			goto error_data;
		}
	}

	Cover_Eph5_extract(&Eph5, data);

	for (size_t i = 0; i < COVER_EPH5_MAXIMUM_K; ++i) {
		if (!file_write(argv[optind + 1 + i], Eph5.extractable_length[i], data[i])) {
			fprintf(stderr, "Can't write result for k = %zu\n", i + 1);

			goto error_output;
		}
	}

	result = EXIT_SUCCESS;

	error_output: ;

	error_data: for (size_t i = 0; i < COVER_EPH5_MAXIMUM_K; ++i) {
		free(data[i]);
	}

	error_coefficients: Cover_Eph5_destroy(&Eph5);
	error_Eph5: container_file_destroy(&image);
	error_image: ;
	error_command_line: ;

	return result;
}

static int main_embed(int argc, char **argv) {
	int result = EXIT_FAILURE;

	int k = COVER_EPH5_MAXIMUM_K;
	bool analyze = false;
	bool fit = false;
	const char *password = default_password;

	const char *short_options = "k:afp:";

	struct option long_options[] = {
		{"k", required_argument, NULL, 'k'},
		{"analyze", no_argument, NULL, 'a'},
		{"fit", no_argument, NULL, 'f'},
		{"password", required_argument, NULL, 'p'},
		{0}
	};

	opterr = 0;

	while (true) {
		int option = getopt_long(argc, argv, short_options, long_options, NULL);

		if (option == -1) {
			break;
		} else if (option == 'k') {
			char *end;
			uintmax_t parsed = strtoumax(optarg, &end, 0);

			if (*optarg == '\0' || *end != '\0' || parsed == 0 || parsed > COVER_EPH5_MAXIMUM_K) {
				fputs("Wrong k value\n", stderr);

				goto error_command_line;
			}

			k = parsed;
			analyze = false;
		} else if (option == 'a') {
			analyze = true;
		} else if (option == 'f') {
			fit = true;
		} else if (option == 'p') {
			password = optarg;
		} else {
			fputs("Wrong option\n", stderr);

			goto error_command_line;
		}
	}

	if (argc - optind != 3) {
		fputs("Wrong number of file arguments\n", stderr);

		goto error_command_line;
	}

	struct container_file image;

	if (!container_file_initialize(&image, argv[optind + 1])) {
		fputs("Can't read image\n", stderr);

		goto error_image;
	}

	struct Cover_container *container = &image.container;

	uint8_t key[COVER_EPH5_KEY_LENGTH];

	Cover_Eph5_expand_password(key, password);

	if (setjmp(image.catch) != 0) {
		fputs("Can't read coefficients\n", stderr);

		goto error_coefficients;
	}

	struct Cover_Eph5 Eph5;

	if (!Cover_Eph5_initialize(&Eph5, container, key, true)) {
		fputs("Can't allocate memory\n", stderr);

		goto error_Eph5;
	}

	size_t length = Eph5.maximum_capacity[analyze || fit ? 0 : k - 1];
	uint8_t *data = malloc(length == 0 ? 1 : length);

	if (data == NULL) {
		perror("LibC error");

		goto error_data;
	}

	if (!file_read(&length, data, argv[optind], true)) {
		fputs("Can't read data\n", stderr);

		goto error_input;
	}

	if (analyze) {
		for (k = COVER_EPH5_MAXIMUM_K; k > 1; --k) {
			if (Eph5.expected_capacity[k - 1] >= length) {
				break;
			}
		}
	}

	size_t embedded_length;

	while (true) {
		embedded_length = Cover_Eph5_embed(&Eph5, length, data, k);

		if (embedded_length < length && fit && k > 1) {
			--k;
		} else {
			break;
		}
	}

	if (analyze || fit) {
		printf("Chosen k: %i\n", k);
	}

	if (embedded_length < length) {
		fputs("Too low capacity\n", stderr);

		goto error_embed;
	}

	if (setjmp(image.catch) != 0) {
		fputs("Can't write coefficients\n", stderr);

		goto error_apply;
	}

	size_t zeroed_count;
	size_t changed_count = Cover_Eph5_apply(&Eph5, &zeroed_count);

	printf("Changed coefficients: %zu\n", changed_count);
	printf("Including zeroed: %zu\n", zeroed_count);

	if (!container_file_write(&image, argv[optind + 2])) {
		fputs("Can't write result\n", stderr);

		goto error_output;
	}

	result = EXIT_SUCCESS;

	error_output: ;
	error_apply: ;
	error_embed: ;
	error_input: free(data);
	error_data: ;
	error_coefficients: Cover_Eph5_destroy(&Eph5);
	error_Eph5: container_file_destroy(&image);
	error_image: ;
	error_command_line: ;

	return result;
}

int main_eph5(int argc, char **argv) {
	if (argc < 2) {
		fputs("Command not specified\n", stderr);

		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "extract") == 0) {
		return main_extract(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "embed") == 0) {
		return main_embed(argc - 1, argv + 1);
	} else {
		fputs("Unknown command\n", stderr);

		return EXIT_FAILURE;
	}
}
