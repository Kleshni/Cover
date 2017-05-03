#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "main.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fputs("Subprogram not specified\n", stderr);

		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "container") == 0) {
		return main_container(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "eph5") == 0) {
		return main_eph5(argc - 1, argv + 1);
	} else if (strcmp(argv[1], "rang") == 0) {
		return main_rang(argc - 1, argv + 1);
	} else {
		fputs("Unknown subprogram\n", stderr);

		return EXIT_FAILURE;
	}
}
