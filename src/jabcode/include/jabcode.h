/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file jabcode.h
 * @brief Main libjabcode header
 */

#ifndef _JAB_H
#define _JAB_H

#define VERSION "1.0.0"
#define BUILD_DATE __DATE__

#define MAX_SYMBOL_NUMBER       61
#define MAX_COLOR_NUMBER        256
#define MAX_SIZE_ENCODING_MODE  256
#define JAB_ENCODING_MODES      6
#define ENC_MAX                 1000000
#define NUMBER_OF_MASK_PATTERNS	8

#define DEFAULT_COLOR_NUMBER 	8
#define DEFAULT_SYMBOL_NUMBER 	1
#define DEFAULT_MODULE_SIZE		12

#define DISTANCE_TO_BORDER                  4
#define MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS 16

#define BITMAP_BITS_PER_PIXEL	32
#define BITMAP_BITS_PER_CHANNEL	8
#define BITMAP_CHANNEL_COUNT	4

#define	JAB_SUCCESS		1
#define	JAB_FAILURE		0

#define NORMAL_DECODE		0
#define COMPATIBLE_DECODE	1

#define VERSION2SIZE(x)		(x * 4 + 17)
#define MAX(a,b) 			({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b;})
#define MIN(a,b) 			({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b;})

#define JAB_REPORT_ERROR(x)	{ printf("JABCode Error: "); printf x; printf("\n"); }
#define JAB_REPORT_INFO(x)	{ printf("JABCode Info: "); printf x; printf("\n"); }

typedef unsigned char 		jab_byte;
typedef char 				jab_char;
typedef unsigned char 		jab_boolean;
typedef int 				jab_int32;
typedef unsigned int 		jab_uint32;
typedef short 				jab_int16;
typedef unsigned short 		jab_uint16;
typedef long long 			jab_int64;
typedef unsigned long long	jab_uint64;
typedef float				jab_float;
typedef double              jab_double;

/**
 * @brief 2-dimensional integer vector
*/
typedef struct {
	jab_int32	x;
	jab_int32	y;
}jab_vector2d;

/**
 * @brief Data structure
*/
typedef struct {
	jab_int32	length;
	jab_char	data[];
}jab_data;

/**
 * @brief Code bitmap
*/
typedef struct {
   jab_int32	width;
   jab_int32	height;
   jab_int32	bits_per_pixel;
   jab_int32	bits_per_channel;
   jab_int32	channel_count;
   jab_byte		pixel[];
}jab_bitmap;

/**
 * @brief Symbol parameters
*/
typedef struct {
	jab_int32		index;
	jab_vector2d	side_size;
	jab_byte*		data_map;				///< Encoded data (data + error correction bits)
	jab_data*		encoded_metadata;		///< Encoded metadata (metadata + error correction bits)
	jab_byte*		matrix;
}jab_symbol;

/**
 * @brief Encode parameters
*/
typedef struct {
	jab_int32		color_number;
	jab_int32		symbol_number;
	jab_int32		module_size;
	jab_int32		master_symbol_width;
	jab_int32		master_symbol_height;
	jab_byte*		palette;				///< Palette holding used module colors in format RGB
	jab_vector2d*	symbol_versions;
	jab_byte* 		ecc_levels;
	jab_int32*		symbol_positions;
    jab_int32*      docked_symbol;

	jab_symbol*		symbols;				///< Pointer to internal representation of JAB Code symbols
	jab_bitmap*		bitmap;
}jab_encode;


extern jab_encode* createEncode(jab_int32 color_number, jab_int32 symbol_number);
extern void destroyEncode(jab_encode* enc);
extern jab_boolean generateJABCode(jab_encode* enc, jab_data* data);
extern jab_data* decodeJABCode(jab_bitmap* bitmap, jab_int32 mode);
extern jab_boolean saveImage(jab_bitmap* bitmap, jab_char* filename);
extern jab_bitmap* readImage(jab_char* filename);
extern void reportError(jab_char* message);

#endif
