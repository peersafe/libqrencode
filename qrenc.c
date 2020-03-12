/**
 * qrencode - QR Code encoder
 *
 * QR Code encoding tool
 * Copyright (C) 2006-2017 Kentaro Fukuchi <kentaro@fukuchi.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#if HAVE_PNG
#include <png.h>
#endif

#include "qrencode.h"

#define INCHES_PER_METER (100.0/2.54)

static int casesensitive = 1;
static int version = 0;
static int width = 128;
static int heigth = 128;
static int size = 3;
static int margin = -1;
static int max_pixel_size = 0; // 最大使用多少个像素表示二维码
static QRecLevel level = QR_ECLEVEL_M;
static QRencodeMode hint = QR_MODE_8;
static int verbose = 0;

static const struct option options[] = {
	{"help"         , no_argument      , NULL, 'h'},
	{"output"       , required_argument, NULL, 'o'},
	{"read-from"    , no_argument		, NULL, 'r'},
	{"width"    	, required_argument, NULL, 'w'},
	{"heigth"    	, required_argument, NULL, 'H'},
	{"level"        , required_argument, NULL, 'l'},
	{"size"         , required_argument, NULL, 's'},
	{"symversion"   , required_argument, NULL, 'v'},
	{"max_pixel_size"   , optional_argument, NULL, 'P'},
	{"verbose"      , no_argument      , &verbose, 1},
	{NULL, 0, NULL, 0}
};

static char *optstring = "ho:r:w:H:l:s:v:P:";

static void usage(int help, int longopt, int status)
{
	FILE *out = status ? stderr : stdout;
	fprintf(out,
"qrencode version %s\n"
"Copyright (C) 2006-2017 Kentaro Fukuchi\n", QRcode_APIVersionString());
	if(help) {
		if(longopt) {
			fprintf(out,
"Usage: qrencode [OPTION]... [STRING]\n"
"Encode input data in a QR Code and save as a PNG or EPS image.\n\n"
"  -h, --help   display the help message. -h displays only the help of short\n"
"               options.\n\n"
"  -o FILENAME, --output=FILENAME\n"
"               write image to FILENAME. If '-' is specified, the result\n"
"               will be output to standard output. If -S is given, structured\n"
"               symbols are written to FILENAME-01.png, FILENAME-02.png, ...\n"
"               (suffix is removed from FILENAME, if specified)\n\n"
"  -r FILENAME, --read-from=FILENAME\n"
"               read input data from FILENAME.\n\n"
"  -w NUMBER, --width=NUMBER\n"
"               specify width\n\n"
"  -H NUMBER, --heigth=NUMBER\n"
"               specify heigth\n\n"
"  -s NUMBER, --size=NUMBER\n"
"               specify module size in dots (pixels). (default=3)\n\n"
"  -l {LMQH}, --level={LMQH}\n"
"               specify error correction level from L (lowest) to H (highest).\n"
"               (default=L)\n\n"
"  -P NUMBER, --max_pixel_size=NUMBER\n"
"               using max numbers of pixel represents a pixel of QRCode.\n"
"               (default=0)\n\n"
"  -v NUMBER, --symversion=NUMBER\n"
"               specify the minimum version of the symbol. See SYMBOL VERSIONS\n"
"               for more information. (default=auto)\n\n"
			);
		} else {
			fprintf(out,
"Usage: qrencode [OPTION]... [STRING]\n"
"Encode input data in a QR Code and save as a PNG or EPS image.\n\n"
"  -h           display this message.\n"
"  --help       display the usage of long options.\n"
"  -o FILENAME  write image to FILENAME. If '-' is specified, the result\n"
"               will be output to standard output. If -S is given, structured\n"
"               symbols are written to FILENAME-01.png, FILENAME-02.png, ...\n"
"               (suffix is removed from FILENAME, if specified)\n"
"  -r FILENAME, --read-from=FILENAME\n"
"               read input data from FILENAME.\n\n"
"  -w NUMBER, --width=NUMBER\n"
"               specify width\n\n"
"  -H NUMBER, --heigth=NUMBER\n"
"               specify heigth\n\n"
"  -s NUMBER, --size=NUMBER\n"
"               specify module size in dots (pixels). (default=3)\n\n"
"  -l {LMQH}, --level={LMQH}\n"
"               specify error correction level from L (lowest) to H (highest).\n"
"               (default=L)\n\n"
"  -P NUMBER, --max_pixel_size=NUMBER\n"
"               using max numbers of pixel represents a pixel of QRCode.\n"
"               (default=0)\n\n"
"  -v NUMBER, --symversion=NUMBER\n"
"               specify the minimum version of the symbol. See SYMBOL VERSIONS\n"
"               for more information. (default=auto)\n\n"
"  [STRING]     input data. If it is not specified, data will be taken from\n"
"               standard input.\n\n"
"  Try \"qrencode --help\" for more options.\n"
			);
		}
	}
}


#define MAX_DATA_SIZE (7090 * 2) /* timed by the safty factor 2 */
static unsigned char data_buffer[MAX_DATA_SIZE];
static unsigned char *readFile(FILE *fp, int *length)
{
	int ret;

	ret = fread(data_buffer, 1, MAX_DATA_SIZE, fp);
	if(ret == 0) {
		fprintf(stderr, "No input data.\n");
		exit(EXIT_FAILURE);
	}
	if(feof(fp) == 0) {
		fprintf(stderr, "Input data is too large.\n");
		exit(EXIT_FAILURE);
	}

	data_buffer[ret] = '\0';
	*length = ret;

	return data_buffer;
}

static int writeFile(FILE* fp, const char* data, int size) {
	int ret;
	ret = fwrite(data, 1, size, fp);
	if(ret == 0) {
		fprintf(stderr, "write data failure.\n");
		exit(EXIT_FAILURE);
	}
	return ret;
}

static FILE *openFile(const char *outfile)
{
	FILE *fp;

	if(outfile == NULL || (outfile[0] == '-' && outfile[1] == '\0')) {
		fp = stdout;
	} else {
		fp = fopen(outfile, "wb");
		if(fp == NULL) {
			fprintf(stderr, "Failed to create file: %s\n", outfile);
			perror(NULL);
			exit(EXIT_FAILURE);
		}
	}

	return fp;
}

struct point
{
	int x;
	int y;
};

struct point make_point(int x, int y) {
	struct point p;
	p.x = x;
	p.y = y;
	return p;
}

//////////////////////////////////////////

struct Rect
{
	int width;
	int heigth;
	struct point origin;
};

struct Rect make_rect(int width, int heigth) {
	struct Rect drawRect;
	drawRect.width = width;
	drawRect.heigth = heigth;

	return drawRect;
}

////////////////////////////////////////////////

struct BitContent 
{
	int width;
	int heigth;
	int bytes_per_pixel;
	char* data;
};

int bitcontent_size(const struct BitContent* ctx) {
    int padding = (4 - ((ctx->width * 3) % 4)) % 4;
    return ctx->heigth * (ctx->width *  ctx->bytes_per_pixel + padding);
}

struct BitContent make_bitcontent(int width, int heigth, int bytes_per_pixel) {
	struct BitContent ctx;
	int size = 0;
	ctx.width = width;
	ctx.heigth = heigth;
	ctx.bytes_per_pixel = bytes_per_pixel;
	size = bitcontent_size(&ctx);
	ctx.data = malloc(size);
	memset(ctx.data, 0xff, size);
	return ctx;
}

void releaes_bitcontent(struct BitContent* ctx) {
	free(ctx->data);
}

void addRect(struct BitContent* ctx, const struct Rect* rect) {

	int width_pixel = rect->width;
    int heigth_pixel = rect->heigth;
    struct point p = rect->origin;
	char* data = ctx->data;
    unsigned char *startRect = &data[p.y * ctx->width * ctx->bytes_per_pixel + p.x * ctx->bytes_per_pixel];

    for (int h = 0; h < heigth_pixel; h++)
    {
        for (int w = 0; w < width_pixel; w++)
        {
			for (int i = 0; i < ctx->bytes_per_pixel; i++) {
				startRect[w * ctx->bytes_per_pixel + i] = 0x00;
			}
		}
        startRect = &data[(p.y + h + 1) * ctx->width * ctx->bytes_per_pixel + p.x * ctx->bytes_per_pixel];
    }

}

static void SaveQR(const QRcode * qrcode, const char *outfile)
{
	FILE *fp = NULL;
	int margin = 0;
	int pixel_size = 1;
	int qr_width = qrcode->width;
	if (width % qr_width) {
		pixel_size = width / qr_width;
		if(max_pixel_size > 0 && max_pixel_size < pixel_size) {
			pixel_size = max_pixel_size;
		}
		margin = (width - qr_width * pixel_size) / 2;
	}

	fprintf(stdout, "pixel size = %d,margin = %d\n",
		pixel_size, margin);

	struct BitContent ctx = make_bitcontent(width, heigth, size);
	struct Rect drawRect = make_rect(pixel_size, pixel_size);
	unsigned char* qr_data = qrcode->data;
	for (int y = 0; y < qr_width; y++) {
		for (int x = 0; x < qr_width; x++) {
			if (*qr_data & 1) {
				drawRect.origin = make_point(margin + x * pixel_size, margin + y * pixel_size);
				addRect(&ctx, &drawRect);
			}
			qr_data++;
		}
	}

	fp = openFile(outfile);
	writeFile(fp, ctx.data, bitcontent_size(&ctx));
	releaes_bitcontent(&ctx);
}

int main(int argc, char **argv)
{
	int opt, lindex = -1;
	char *outfile = NULL, *infile = NULL;
	unsigned char *intext = NULL;
	int length = 0;
	FILE *fp;

	while((opt = getopt_long(argc, argv, optstring, options, &lindex)) != -1) {
		switch(opt) {
			case 'h':
				if(lindex == 0) {
					usage(1, 1, EXIT_SUCCESS);
				} else {
					usage(1, 0, EXIT_SUCCESS);
				}
				exit(EXIT_SUCCESS);
			case 'o':
				outfile = optarg;
				break;
			case 'r':
				infile = optarg;
				break;
			case 's':
				size = atoi(optarg);
				if(size <= 0) {
					fprintf(stderr, "Invalid size: %d\n", size);
					exit(EXIT_FAILURE);
				}
				break;
			case 'w':
				width = atoi(optarg);
				if(size <= 0) {
					fprintf(stderr, "Invalid width: %d\n", width);
					exit(EXIT_FAILURE);
				}
				break;
			case 'H':
				heigth = atoi(optarg);
				if(size <= 0) {
					fprintf(stderr, "Invalid heigth: %d\n", heigth);
					exit(EXIT_FAILURE);
				}
				break;
			case 'v':
				version = atoi(optarg);
				if(version < 0) {
					fprintf(stderr, "Invalid version: %d\n", version);
					exit(EXIT_FAILURE);
				}
				break;
			case 'P':
				max_pixel_size= atoi(optarg);
				if(max_pixel_size < 0) {
					fprintf(stderr, "Invalid pixel size: %d\n", max_pixel_size);
					exit(EXIT_FAILURE);
				}
				break;
			case 'l':
				switch(*optarg) {
					case 'l':
					case 'L':
						level = QR_ECLEVEL_L;
						break;
					case 'm':
					case 'M':
						level = QR_ECLEVEL_M;
						break;
					case 'q':
					case 'Q':
						level = QR_ECLEVEL_Q;
						break;
					case 'h':
					case 'H':
						level = QR_ECLEVEL_H;
						break;
					default:
						fprintf(stderr, "Invalid level: %s\n", optarg);
						exit(EXIT_FAILURE);
				}
				break;
			case 'V':
				usage(0, 0, EXIT_SUCCESS);
				exit(EXIT_SUCCESS);
			case 0:
				break;
			default:
				fprintf(stderr, "Try \"qrencode --help\" for more information.\n");
				exit(EXIT_FAILURE);
		}
	}

	if(argc == 1) {
		usage(1, 0, EXIT_FAILURE);
		exit(EXIT_FAILURE);
	}

	if(outfile == NULL) {
		fprintf(stderr, "No output filename is given.\n");
		exit(EXIT_FAILURE);
	}

	if(optind < argc) {
		intext = (unsigned char *)argv[optind];
		length = strlen((char *)intext);
	}

	if(intext == NULL) {
		fp = infile == NULL ? stdin : fopen(infile,"r");
		if(fp == 0) {
			fprintf(stderr, "Can not read input file %s.\n", infile);
			exit(EXIT_FAILURE);
		}
		intext = readFile(fp,&length);

	}

	//fprintf(stdout, "out=%s\nw=%d\nh=%d\ns=%d\nlevel=%d\nv=%d\nstr=%s\n",
	// 	outfile, width, heigth, size, level, version, intext);

	QRcode *code;
	code = QRcode_encodeString((char *)intext, version, level, hint, casesensitive);
	if(code) {
		SaveQR(code, outfile);
	} else {
		fprintf(stderr, "QRcode_encodeString failed.\n");
	}
	return 0;
}
