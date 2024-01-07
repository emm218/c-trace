#include <errno.h>
#include <getopt.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "0.1"

#define DEFAULT_WIDTH	    640
#define DEFAULT_HEIGHT	    480
#define DEFAULT_SAMPLES	    30
#define DEFAULT_MAX_BOUNCES 2

typedef struct {
	unsigned char r, g, b;
} pixel;

void usage(FILE *);
void png_error_handler(png_structp, png_const_charp);
int write_png_init(long, long);

char *prog_name;

static int help_flag;
static int version_flag;
static png_structp png_ptr;
static png_infop info_ptr;
static jmp_buf jb;

static struct option long_opts[] = {
	{ "help", no_argument, &help_flag, 'h' },
	{ "version", no_argument, &version_flag, 'v' },
	{ "samples", required_argument, NULL, 's' },
	{ "geometry", required_argument, NULL, 'g' },
	{ "bounces", required_argument, NULL, 'b' },
	{ NULL, 0, NULL, 0 },
};

int
main(int argc, char **argv)
{
	char *cur, *end, *geom_str, *samples_str, *bounce_str;
	int c, opt_idx, samples, max_bounces;
	long x, y, width, height;
	pixel *row;
	FILE *world_file;

	prog_name = argv[0];
	samples_str = NULL;
	geom_str = NULL;
	opt_idx = 0;

	while ((c = getopt_long(argc, argv, "hvs:g:b:", long_opts, &opt_idx)) !=
	    -1) {
		if (c == 0) {
			if (long_opts[opt_idx].flag)
				continue;
			c = long_opts[opt_idx].val;
		}
		switch (c) {
		case 's':
			samples_str = optarg;
			break;
		case 'g':
			geom_str = optarg;
			break;
		case 'b':
			bounce_str = optarg;
			break;
		case 'h':
			help_flag = 1;
			break;
		case 'v':
			version_flag = 1;
			break;
		default:
			usage(stderr);
			return 1;
		}
	}

	if (help_flag) {
		printf("c-trace version " VERSION "\n\n");
		usage(stdout);
		return 0;
	}

	if (version_flag) {
		printf("c-trace version " VERSION "\n");
		return 0;
	}

	if (isatty(fileno(stdout))) {
		fprintf(stderr,
		    "%s: please redirect stdout to a file or another program\n",
		    argv[0]);
		return 1;
	}

	// parse geometry string
	if (!geom_str) {
		width = DEFAULT_WIDTH;
		height = DEFAULT_HEIGHT;
		goto parse_samples;
	}
	errno = 0;
	cur = geom_str;
	width = strtol(cur, &end, 10);
	if (end == cur || *end != 'x') {
		fprintf(stderr, "%s: invalid geometry string\n", argv[0]);
		goto fail;
	}
	cur = end + 1;
	height = strtol(cur, &end, 10);
	if (end == cur || *end) {
		fprintf(stderr, "%s: invalid geometry string\n", argv[0]);
		goto fail;
	}
	if (errno == ERANGE) {
		fprintf(stderr, "%s: width or height out of range\n", argv[0]);
		goto fail;
	}

	if (width <= 0) {
		fprintf(stderr, "%s: width must be greater than 0, got %ld\n",
		    argv[0], width);
		goto fail;
	}
	if (height <= 0) {
		fprintf(stderr, "%s: height must be greater than 0, got %ld\n",
		    argv[0], height);
		goto fail;
	}
parse_samples:
	if (!samples_str) {
		samples = DEFAULT_SAMPLES;
		goto parse_bounces;
	}
	errno = 0;
	samples = (int)strtol(samples_str, &end, 10);
	if (*end || end == samples_str) {
		fprintf(stderr, "%s: samples must be a number\n", argv[0]);
		goto fail;
	}
	if (errno == ERANGE) {
		fprintf(stderr, "%s: samples out of range\n", argv[0]);
		goto fail;
	}
	if (width <= 0) {
		fprintf(stderr, "%s: samples must be greater than 0\n",
		    argv[0]);
		goto fail;
	}
parse_bounces:
	if (!bounce_str) {
		max_bounces = DEFAULT_MAX_BOUNCES;
		goto done;
	}
	errno = 0;
	max_bounces = (int)strtol(bounce_str, &end, 10);
	if (*end || end == bounce_str) {
		fprintf(stderr, "%s: bounces must be a number\n", argv[0]);
		goto fail;
	}
	if (errno == ERANGE) {
		fprintf(stderr, "%s: bounces out of range\n", argv[0]);
		goto fail;
	}
	if (width <= 0) {
		fprintf(stderr, "%s: bounces must be greater than 0\n",
		    argv[0]);
		goto fail;
	}
done:
	errno = 0;
	if (optind == argc || strcmp(argv[optind], "-") == 0) {
		world_file = stdin;
	} else if ((world_file = fopen(argv[optind], "r")) == NULL) {
		perror(argv[0]);
		return 1;
	}

	if (world_file != stdin)
		fclose(world_file);

	if ((row = malloc(sizeof(*row) * width)) == NULL)
		return 1; // oh nooooo

	write_png_init(width, height);

	(void)max_bounces;
	(void)samples;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			row[x] = (pixel) { 0, 0, 0 };
		}
		png_write_row(png_ptr, (unsigned char *)row);
	}

	png_write_end(png_ptr, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	free(row);
	return 0;
fail:
	usage(stderr);
	return 1;
}

int
write_png_init(long width, long height)
{
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
	    png_error_handler, NULL);
	if (!png_ptr)
		return 4;
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		return 4;
	}

	if (setjmp(jb)) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return 2;
	}

	png_init_io(png_ptr, stdout);
	png_set_IHDR(png_ptr, info_ptr, width, height,
	    8, // bit depth
	    PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
	    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);
	return 0;
}

void
png_error_handler(png_structp png_ptr, png_const_charp msg)
{
	(void)png_ptr;

	fprintf(stderr, "%s: %s\n", prog_name, msg);
	longjmp(jb, 1);
}

void
usage(FILE *out)
{
	// clang-format off
fprintf(out,
"usage: c-trace [options] [file]\n"
"\n"
"OPTIONS:\n"
"  -h, --help\t\t\tshow help and exit\n"
"  -v, --version\t\t\tshow version info and exit\n"
"  -g, --geometry WIDTHxHEIGHT\toutput dimensions; default %dx%d\n"
"  -s, --samples SAMPLES\t\tnumber of samples per pixel; default %d\n"
"  -b, --bounces BOUNCES\t\tmaximum bounces to calculate; default %d\n"
"\n"
"if no file is provided or if file is '-', reads from standard input\n",
DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_SAMPLES, DEFAULT_MAX_BOUNCES);
	// clang-format on
}
