/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file decoder.h
 * @brief Decoder header
 */

#ifndef JABCODE_DECODER_H
#define JABCODE_DECODER_H

#define DECODE_METADATA_FAILED -1
#define FATAL_ERROR -2	//e.g. out of memory

#define MASTER_METADATA_X	6
#define MASTER_METADATA_Y	1

#define MASTER_METADATA_PART1_LENGTH 6			//master metadata part 1 encoded length
#define MASTER_METADATA_PART2_LENGTH 38			//master metadata part 2 encoded length
#define MASTER_METADATA_PART1_MODULE_NUMBER 4	//the number of modules used to encode master metadata part 1

/**
 * @brief The positions of the first 32 color palette modules in slave symbol
*/
static const jab_vector2d slave_palette_position[32] =
		{	{4, 5}, {4, 6}, {4, 7}, {4, 8}, {4, 9}, {4, 10}, {4, 11}, {4, 12},
			{5, 12}, {5, 11}, {5, 10}, {5, 9}, {5, 8}, {5, 7}, {5, 6}, {5, 5},
			{6, 5}, {6, 6}, {6, 7}, {6, 8}, {6, 9}, {6, 10}, {6, 11}, {6, 12},
			{7, 12}, {7, 11}, {7, 10}, {7, 9}, {7, 8}, {7, 7}, {7, 6}, {7, 5}
		};

/**
 * @brief Decoding tables
*/
static const jab_byte jab_decoding_table_upper[27]   = {32, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90};
static const jab_byte jab_decoding_table_lower[27]   = {32, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122};
static const jab_byte jab_decoding_table_numeric[13] = {32, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 44, 46};
static const jab_byte jab_decoding_table_punct[16]   = {33, 34, 36, 37, 38, 39, 40, 41, 44, 45, 46, 47, 58, 59, 63, 64};
static const jab_byte jab_decoding_table_mixed[32]   = {35, 42, 43, 60, 61, 62, 91, 92, 93, 94, 95, 96, 123, 124, 125, 126, 9, 10, 13, 0, 0, 0, 0, 164, 167, 196, 214, 220, 223, 228, 246, 252};
static const jab_byte jab_decoding_table_alphanumeric[63] = {32, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85,
															 86, 87, 88, 89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122};

/**
 * @brief Encoding mode
*/
typedef enum {
	None = -1,
	Upper = 0,
	Lower,
	Numeric,
	Punct,
	Mixed,
	Alphanumeric,
	Byte,
	ECI,
	FNC1
}jab_encode_mode;

extern jab_int32 decodeMaster(jab_bitmap* matrix, jab_decoded_symbol* symbol);
extern jab_int32 decodeSlave(jab_bitmap* matrix, jab_decoded_symbol* symbol);
extern jab_data* decodeData(jab_data* bits);
extern void deinterleaveData(jab_data* data);
extern void getNextMetadataModuleInMaster(jab_int32 matrix_height, jab_int32 matrix_width, jab_int32 next_module_count, jab_int32* x, jab_int32* y);
extern void demaskSymbol(jab_data* data, jab_byte* data_map, jab_vector2d symbol_size, jab_int32 mask_type, jab_int32 color_number);
extern jab_int32 readColorPaletteInMaster(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_int32* module_count, jab_int32* x, jab_int32* y);
extern jab_int32 readColorPaletteInSlave(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map);

#endif
