/*
 * Convert binary file into a C #define
 *
 * Yes, this would have been a perl one-liner, but I didn't want
 * to include compile-time dependency for perl...
 */

#include <stdio.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	char *source;
	char *name;
	int first = 1;
	int c;
	int i;

/* Get & check parameters */
	source = argv[1];
	name = argv[2];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <source> [<NAME>]\n", argv[0]);
		return 1;
	}

	if (argc < 3) name = source;

/* Try to open the source file */
	if ((fp = fopen(source, "r")) == NULL) {
		perror("Couldn't open source file");
		return 1;
	}

/* Convert */
	printf("/* Automatically generated from %s */\n\n"
		"#define %s { \\\n", source, name);

	do {
		for (i = 0; i < 16; i++) {
			if ((c = fgetc(fp)) == EOF) break;

			if (i == 0 && !first) printf(", \\\n");
			if (i > 0) printf(", ");

			printf("0x%02x", c);
			first = 0;
		}
	} while (c != EOF);

	printf("}\n\n");
	fclose(fp);
	return 0;
}

