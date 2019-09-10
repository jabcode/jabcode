/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file decoder.c
 * @brief Data decoding
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jabcode.h"
#include "detector.h"
#include "decoder.h"
#include "ldpc.h"
#include "encoder.h"

/**
 * @brief Copy 16-color sub-blocks of 64-color palette into 32-color blocks of 256-color palette and interpolate into 32 colors
 * @param palette the color palette
 * @param dst_offset the start offset in the destination palette
 * @param src_offset the start offset in the source palette
*/
void copyAndInterpolateSubblockFrom16To32(jab_byte* palette, jab_int32 dst_offset, jab_int32 src_offset)
{
	//copy
	memcpy(palette + dst_offset + 84, palette + src_offset + 36, 12);
	memcpy(palette + dst_offset + 60, palette + src_offset + 24, 12);
	memcpy(palette + dst_offset + 24, palette + src_offset + 12, 12);
	memcpy(palette + dst_offset + 0,  palette + src_offset + 0,  12);
	//interpolate
	for(jab_int32 j=0; j<12; j++)
	{
		jab_int32 sum = palette[dst_offset + 0 + j] + palette[dst_offset + 24 + j];
		palette[dst_offset + 12 + j] = (jab_byte)(sum / 2);
	}
	for(jab_int32 j=0; j<12; j++)
	{
		jab_int32 sum = palette[dst_offset + 24 + j] * 2 + palette[dst_offset + 60 + j];
		palette[dst_offset + 36 + j] = (jab_byte)(sum / 3);
		sum = palette[dst_offset + j] + palette[dst_offset + 60 + j] * 2;
		palette[dst_offset + 48 + j] = (jab_byte)(sum / 3);
	}
	for(jab_int32 j=0; j<12; j++)
	{
		jab_int32 sum = palette[dst_offset + 60 + j] + palette[dst_offset + 84 + j];
		palette[dst_offset + 72 + j] = (jab_byte)(sum / 2);
	}
}

/**
 * @brief Interpolate 64-color palette into 128-/256-color palette
 * @param palette the color palette
 * @param color_number the number of colors
*/
void interpolatePalette(jab_byte* palette, jab_int32 color_number)
{
	for(jab_int32 i=0; i<COLOR_PALETTE_NUMBER; i++)
	{
		jab_int32 offset = color_number * 3 * i;
		if(color_number == 128)											//each block includes 16 colors
		{																//block 1 remains the same
			memcpy(palette + offset + 336, palette + offset + 144, 48); //copy block 4 to block 8
			memcpy(palette + offset + 240, palette + offset + 96,  48); //copy block 3 to block 6
			memcpy(palette + offset + 96,  palette + offset + 48,  48); //copy block 2 to block 3

			//interpolate block 1 and block 3 to get block 2
			for(jab_int32 j=0; j<48; j++)
			{
				jab_int32 sum = palette[offset + 0 + j] + palette[offset + 96 + j];
				palette[offset + 48 + j] = (jab_byte)(sum / 2);
			}
			//interpolate block 3 and block 6 to get block 4 and block 5
			for(jab_int32 j=0; j<48; j++)
			{
				jab_int32 sum = palette[offset + 96 + j] * 2 + palette[offset + 240 + j];
				palette[offset + 144 + j] = (jab_byte)(sum / 3);
				sum = palette[offset + 96 + j] + palette[offset + 240 + j] * 2;
				palette[offset + 192 + j] = (jab_byte)(sum / 3);
			}
			//interpolate block 6 and block 8 to get block 7
			for(jab_int32 j=0; j<48; j++)
			{
				jab_int32 sum = palette[offset + 240 + j] + palette[offset + 336 + j];
				palette[offset + 288 + j] = (jab_byte)(sum / 2);
			}
		}
		else if(color_number == 256)									//each block includes 32 colors
		{
			//copy sub-block 4 to block 8 and interpolate 16 colors into 32 colors
			copyAndInterpolateSubblockFrom16To32(palette, offset + 672, offset + 144);
			//copy sub-block 3 to block 6 and interpolate 16 colors into 32 colors
			copyAndInterpolateSubblockFrom16To32(palette, offset + 480, offset + 96);
			//copy sub-block 2 to block 3 and interpolate 16 colors into 32 colors
			copyAndInterpolateSubblockFrom16To32(palette, offset + 192, offset + 48);
			//copy sub-block 1 to block 1 and interpolate 16 colors into 32 colors
			copyAndInterpolateSubblockFrom16To32(palette, offset + 0, offset + 0);

			//interpolate block 1 and block 3 to get block 2
			for(jab_int32 j=0; j<96; j++)
			{
				jab_int32 sum = palette[offset + 0 + j] + palette[offset + 192 + j];
				palette[offset + 96 + j] = (jab_byte)(sum / 2);
			}
			//interpolate block 3 and block 6 to get block 4 and block 5
			for(jab_int32 j=0; j<96; j++)
			{
				jab_int32 sum = palette[offset + 192 + j] * 2 + palette[offset + 480 + j];
				palette[offset + 288 + j] = (jab_byte)(sum / 3);
				sum = palette[offset + 192 + j] + palette[offset + 480 + j] * 2;
				palette[offset + 384 + j] = (jab_byte)(sum / 3);
			}
			//interpolate block 6 and block 8 to get block 7
			for(jab_int32 j=0; j<96; j++)
			{
				jab_int32 sum = palette[offset + 480 + j] + palette[offset + 672 + j];
				palette[offset + 576 + j] = (jab_byte)(sum / 2);
			}
		}
		else
			return;
	}
}

/**
 * @brief Write colors into color palettes
 * @param matrix the symbol matrix
 * @param symbol the master/slave symbol
 * @param p_index the color palette index
 * @param color_index the color index in color palette
 * @param x the x coordinate of the color module
 * @param y the y coordinate of the color module
*/
void writeColorPalette(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_int32 p_index, jab_int32 color_index, jab_int32 x, jab_int32 y)
{
	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
    jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;

	jab_int32 palette_offset = color_number * 3 * p_index;
	jab_int32 mtx_offset = y * mtx_bytes_per_row + x * mtx_bytes_per_pixel;
	symbol->palette[palette_offset + color_index*3 + 0]	= matrix->pixel[mtx_offset + 0];
	symbol->palette[palette_offset + color_index*3 + 1] = matrix->pixel[mtx_offset + 1];
	symbol->palette[palette_offset + color_index*3 + 2] = matrix->pixel[mtx_offset + 2];
}

/**
 * @brief Get the coordinates of the modules in finder/alignment patterns used for color palette
 * @param p_index the color palette index
 * @param matrix_width the matrix width
 * @param matrix_height the matrix height
 * @param p1 the coordinate of the first module
 * @param p2 the coordinate of the second module
*/
void getColorPalettePosInFP(jab_int32 p_index, jab_int32 matrix_width, jab_int32 matrix_height, jab_vector2d* p1, jab_vector2d* p2)
{
	switch(p_index)
	{
	case 0:
		p1->x = DISTANCE_TO_BORDER - 1;
		p1->y = DISTANCE_TO_BORDER - 1;
		p2->x = p1->x + 1;
		p2->y = p1->y;
		break;
	case 1:
		p1->x = matrix_width - DISTANCE_TO_BORDER;
		p1->y = DISTANCE_TO_BORDER - 1;
		p2->x = p1->x - 1;
		p2->y = p1->y;
		break;
	case 2:
		p1->x = matrix_width - DISTANCE_TO_BORDER;
		p1->y = matrix_height - DISTANCE_TO_BORDER;
		p2->x = p1->x - 1;
		p2->y = p1->y;
		break;
	case 3:
		p1->x = DISTANCE_TO_BORDER - 1;
		p1->y = matrix_height - DISTANCE_TO_BORDER;
		p2->x = p1->x + 1;
		p2->y = p1->y;
		break;
	}
}

/**
 * @brief Read the color palettes in master symbol
 * @param matrix the symbol matrix
 * @param symbol the master symbol
 * @param data_map the data module positions
 * @param module_count the start module index
 * @param x the x coordinate of the start module
 * @param y the y coordinate of the start module
 * @return JAB_SUCCESS | FATAL_ERROR
*/
jab_int32 readColorPaletteInMaster(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_int32* module_count, jab_int32* x, jab_int32* y)
{
	//allocate buffer for palette
	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	free(symbol->palette);
	symbol->palette = (jab_byte*)malloc(color_number * sizeof(jab_byte) * 3 * COLOR_PALETTE_NUMBER);
	if(symbol->palette == NULL)
	{
		reportError("Memory allocation for master palette failed");
		return FATAL_ERROR;
	}

	//read colors from finder patterns
	jab_int32 color_index;			//the color index number in color palette
	for(jab_int32 i=0; i<COLOR_PALETTE_NUMBER; i++)
	{
		jab_vector2d p1, p2;
		getColorPalettePosInFP(i, matrix->width, matrix->height, &p1, &p2);
		//color 0
		color_index = master_palette_placement_index[i][0] % color_number; //for 4-color and 8-color symbols
		writeColorPalette(matrix, symbol, i, color_index, p1.x, p1.y);
		//color 1
		color_index = master_palette_placement_index[i][1] % color_number; //for 4-color and 8-color symbols
		writeColorPalette(matrix, symbol, i, color_index, p2.x, p2.y);
	}

	//read colors from metadata
	jab_int32 color_counter = 2;	//the color counter
	while(color_counter < MIN(color_number, 64))
	{
		//color palette 0
		color_index = master_palette_placement_index[0][color_counter] % color_number; //for 4-color and 8-color symbols
		writeColorPalette(matrix, symbol, 0, color_index, *x, *y);
		//set data map
		data_map[(*y) * matrix->width + (*x)] = 1;
		//go to the next module
		(*module_count)++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, (*module_count), x, y);

		//color palette 1
		color_index = master_palette_placement_index[1][color_counter] % color_number; //for 4-color and 8-color symbols
		writeColorPalette(matrix, symbol, 1, color_index, *x, *y);
		//set data map
		data_map[(*y) * matrix->width + (*x)] = 1;
		//go to the next module
		(*module_count)++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, (*module_count), x, y);

		//color palette 2
		color_index = master_palette_placement_index[2][color_counter] % color_number; //for 4-color and 8-color symbols
		writeColorPalette(matrix, symbol, 2, color_index, *x, *y);
		//set data map
		data_map[(*y) * matrix->width + (*x)] = 1;
		//go to the next module
		(*module_count)++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, (*module_count), x, y);

		//color palette 3
		color_index = master_palette_placement_index[3][color_counter] % color_number; //for 4-color and 8-color symbols
		writeColorPalette(matrix, symbol, 3, color_index, *x, *y);
		//set data map
		data_map[(*y) * matrix->width + (*x)] = 1;
		//go to the next module
		(*module_count)++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, (*module_count), x, y);

		//next color
		color_counter++;
	}

	//interpolate the palette if there are more than 64 colors
	if(color_number > 64)
	{
		interpolatePalette(symbol->palette, color_number);
	}
	return JAB_SUCCESS;
}

/**
 * @brief Read the color palettes in master symbol
 * @param matrix the symbol matrix
 * @param symbol the slave symbol
 * @param data_map the data module positions
 * @return JAB_SUCCESS | FATAL_ERROR
*/
jab_int32 readColorPaletteInSlave(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map)
{
	//allocate buffer for palette
	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	free(symbol->palette);
	symbol->palette = (jab_byte*)malloc(color_number * sizeof(jab_byte) * 3 * COLOR_PALETTE_NUMBER);
    if(symbol->palette == NULL)
    {
		reportError("Memory allocation for slave palette failed");
		return FATAL_ERROR;
    }

    //read colors from alignment patterns
    jab_int32 color_index;			//the color index number in color palette
	for(jab_int32 i=0; i<COLOR_PALETTE_NUMBER; i++)
	{
		jab_vector2d p1, p2;
		getColorPalettePosInFP(i, matrix->width, matrix->height, &p1, &p2);
		//color 0
		color_index = slave_palette_placement_index[0] % color_number;
		writeColorPalette(matrix, symbol, i, color_index, p1.x, p1.y);
		//color 1
		color_index = slave_palette_placement_index[1] % color_number;
		writeColorPalette(matrix, symbol, i, color_index, p2.x, p2.y);
	}

	//read colors from metadata
	jab_int32 color_counter = 2;	//the color counter
	while(color_counter < MIN(color_number, 64))
	{
		jab_int32 px, py;

		//color palette 0
		px = slave_palette_position[color_counter-2].x;
		py = slave_palette_position[color_counter-2].y;
		color_index = slave_palette_placement_index[color_counter] % color_number;
		writeColorPalette(matrix, symbol, 0, color_index, px, py);
		//set data map
		data_map[py * matrix->width + px] = 1;

		//color palette 1
		px = matrix->width - 1 - slave_palette_position[color_counter-2].y;
		py = slave_palette_position[color_counter-2].x;
		color_index = slave_palette_placement_index[color_counter] % color_number;
		writeColorPalette(matrix, symbol, 1, color_index, px, py);
		//set data map
		data_map[py * matrix->width + px] = 1;

		//color palette 2
		px = matrix->width - 1 - slave_palette_position[color_counter-2].x;
		py = matrix->height - 1 - slave_palette_position[color_counter-2].y;
		color_index = slave_palette_placement_index[color_counter] % color_number;
		writeColorPalette(matrix, symbol, 2, color_index, px, py);
		//set data map
		data_map[py * matrix->width + px] = 1;

		//color palette 3
		px = slave_palette_position[color_counter-2].y;
		py = matrix->height - 1 - slave_palette_position[color_counter-2].x;
		color_index = slave_palette_placement_index[color_counter] % color_number;
		writeColorPalette(matrix, symbol, 3, color_index, px, py);
		//set data map
		data_map[py * matrix->width + px] = 1;

		//next color
		color_counter++;
	}

	//interpolate the palette if there are more than 64 colors
	if(color_number > 64)
	{
		interpolatePalette(symbol->palette, color_number);
	}
	return JAB_SUCCESS;
}

/**
 * @brief Calculate the color changing slopes for all color palettes
 * @param matrix the symbol matrix
 * @param palette the color palettes
 * @param color_number the number of module colors
 * @param cs the color changing slopes
*/
void calculateColorSlopes(jab_bitmap* matrix, jab_byte* palette, jab_int32 color_number, jab_float* cs)
{
	jab_float distx = matrix->width - 7;
	jab_float disty = matrix->height- 7;
	jab_float distd = sqrt(distx*distx + disty*disty);

	for(jab_int32 p=0; p<COLOR_PALETTE_NUMBER; p++)
	{
		jab_int32 px, py, pd;
		switch(p)
		{
		case 0:	//slopes for palette 0
			px = 1;//p1-p0
			py = 3;//p3-p0
			pd = 2;//p2-p0
			break;
		case 1:	//slopes for palette 1
			px = 0;//p0-p1
			py = 2;//p2-p1
			pd = 3;//p3-p1
			break;
		case 2:
			px = 3;
			py = 1;
			pd = 0;
			break;
		case 3:
			px = 2;
			py = 0;
			pd = 1;
			break;
		}

		for(jab_int32 i=0; i<color_number*3; i++)
		{
			jab_float sx = (jab_float)(palette[color_number*3*px + i] - palette[color_number*3*p + i]) / distx;
			jab_float sy = (jab_float)(palette[color_number*3*py + i] - palette[color_number*3*p + i]) / disty;
			jab_float sd = (jab_float)(palette[color_number*3*pd + i] - palette[color_number*3*p + i]) / distd;
			sx += (distx / distd) * sd; //x component of sd, sd*cos(theta)
			sy += (disty / distd) * sd; //y component of sd, sd*sin(theta)
			cs[p*(color_number*3)*2 + i*2 + 0] = sx;
			cs[p*(color_number*3)*2 + i*2 + 1] = sy;
		}
	}
}

/**
 * @brief Calibrate module color according to the color changing slopes
 * @param cs the color changing slopes
 * @param color_number the number of module colors
 * @param p_index the color palette index
 * @param color_index the color index in the palette
 * @param px the x coordinate of the palette
 * @param py the y coordinate of the palette
 * @param x the x coordinate of the module
 * @param y the y coordinate of the module
 * @param rgb the pixel value in RGB format
*/
void CaliColor(jab_float* cs, jab_int32 color_number, jab_int32 p_index, jab_int32 color_index, jab_int32 px, jab_int32 py, jab_int32 x, jab_int32 y, jab_byte* rgb)
{
	//calculate the distance between the module and the color palette
	jab_int32 distx, disty;
	distx = x - px;
	disty = y - py;
	switch(p_index)	//switch the sign of the distance
	{
	case 0:
		break;
	case 1:
		distx = -distx;
		break;
	case 2:
		disty = -disty;
		break;
	case 3:
		distx = -distx;
		disty = -disty;
		break;
	}
	//calculate the color difference in x and y directions
	jab_float rgb_dx[3], rgb_dy[3];
	for(jab_int32 i=0; i<3; i++)
	{
		jab_float cs_x = cs[p_index*(color_number*3)*2 + (color_index*3 + i)*2 + 0];
		jab_float cs_y = cs[p_index*(color_number*3)*2 + (color_index*3 + i)*2 + 1];
		rgb_dx[i] = distx * cs_x;
		rgb_dy[i] = disty * cs_y;
	}
	//calculate the color difference
	jab_float dist = DIST(px, py, x, y);
	jab_float cos_theta = fabs(distx) / dist;
	jab_float sin_theta = fabs(disty) / dist;
	jab_int32 dr = (jab_int32)(rgb_dx[0] * cos_theta + rgb_dy[0] * sin_theta);
	jab_int32 dg = (jab_int32)(rgb_dx[1] * cos_theta + rgb_dy[1] * sin_theta);
	jab_int32 db = (jab_int32)(rgb_dx[2] * cos_theta + rgb_dy[2] * sin_theta);
	//calibrate color
	rgb[0] = MIN(MAX((rgb[0] + dr), 0), 255);
	rgb[1] = MIN(MAX((rgb[1] + dg), 0), 255);
	rgb[2] = MIN(MAX((rgb[2] + db), 0), 255);
}

/**
 * @brief Get the index of the nearest color palette
 * @param matrix the symbol matrix
 * @param px the x coordinates of the color palettes
 * @param py the y coordinates of the color palettes
 * @param x the x coordinate of the module
 * @param y the y coordinate of the module
 * @return the index of the nearest color palette
*/
jab_int32 getNearestPalette(jab_bitmap* matrix, jab_int32 px[], jab_int32 py[], jab_int32 x, jab_int32 y)
{
	//get the palette coordinates
	px[0] = DISTANCE_TO_BORDER - 1;
	py[0] = DISTANCE_TO_BORDER - 1;
	px[1] = matrix->width - DISTANCE_TO_BORDER;
	py[1] = DISTANCE_TO_BORDER - 1;
	px[2] = matrix->width - DISTANCE_TO_BORDER;
	py[2] = matrix->height- DISTANCE_TO_BORDER;
	px[3] = DISTANCE_TO_BORDER - 1;
	py[3] = matrix->height- DISTANCE_TO_BORDER;

	//calculate the nearest palette
	jab_float min = DIST(0, 0, matrix->width, matrix->height);
	jab_int32 p_index = 0;
	for(jab_int32 i=0; i<COLOR_PALETTE_NUMBER; i++)
	{
		jab_float dist = DIST(x, y, px[i], py[i]);
		if(dist < min)
		{
			min = dist;
			p_index = i;
		}
	}
	return p_index;
}

/**
 * @brief Decode a module using hard decision
 * @param matrix the symbol matrix
 * @param palette the color palettes
 * @param color_number the number of module colors
 * @param cs the color changing slopes
 * @param x the x coordinate of the module
 * @param y the y coordinate of the module
 * @return the decoded value
*/
jab_byte decodeModuleHD(jab_bitmap* matrix, jab_byte* palette, jab_int32 color_number, jab_float* cs, jab_int32 x, jab_int32 y)
{
	jab_byte rgb[3];
	jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
    jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;
    jab_int32 mtx_offset = y * mtx_bytes_per_row + x * mtx_bytes_per_pixel;
	rgb[0] = matrix->pixel[mtx_offset + 0];
	rgb[1] = matrix->pixel[mtx_offset + 1];
	rgb[2] = matrix->pixel[mtx_offset + 2];

	//get the palette coordinate and the nearest palette
	jab_int32 px[COLOR_PALETTE_NUMBER], py[COLOR_PALETTE_NUMBER];
	jab_int32 p_index = getNearestPalette(matrix, px, py, x, y);

	jab_byte index1 = 0, index2 = 0;
	if(palette)
	{
		jab_int32 min1 = 255*255*3, min2 = 255*255*3;
		for(jab_int32 i=0; i<color_number; i++)
		{
			//calibrate color
			CaliColor(cs, color_number, p_index, i, px[p_index], py[p_index], x, y, rgb);
			//compare module color with palette
			jab_int32 diff = (palette[color_number*3*p_index + i*3 + 0] - rgb[0]) * (palette[color_number*3*p_index + i*3 + 0] - rgb[0]) +
							 (palette[color_number*3*p_index + i*3 + 1] - rgb[1]) * (palette[color_number*3*p_index + i*3 + 1] - rgb[1]) +
							 (palette[color_number*3*p_index + i*3 + 2] - rgb[2]) * (palette[color_number*3*p_index + i*3 + 2] - rgb[2]);
			if(diff < min1)
			{
				//copy min1 to min2
				min2 = min1;
				index2 = index1;
				//update min1
				min1 = diff;
				index1 = (jab_byte)i;
			}
			else if(diff < min2)
			{
				min2 = diff;
				index2 = (jab_byte)i;
			}
		}
		//if the minimum is close to the second minimum, do further match
		if(min1 * 1.5 > min2)
		{
			//printf("min1(%d) * 1.5 > min2(%d), %d %d %d", index1, index2, rgb[0], rgb[1], rgb[2]);
			jab_int32 rg = abs(rgb[0] - rgb[1]);
			jab_int32 rb = abs(rgb[0] - rgb[2]);
			jab_int32 gb = abs(rgb[1] - rgb[2]);

			jab_int32 c1rg = abs(palette[color_number*3*p_index + index1*3 + 0] - palette[color_number*3*p_index + index1*3 + 1]);
			jab_int32 c1rb = abs(palette[color_number*3*p_index + index1*3 + 0] - palette[color_number*3*p_index + index1*3 + 2]);
			jab_int32 c1gb = abs(palette[color_number*3*p_index + index1*3 + 1] - palette[color_number*3*p_index + index1*3 + 2]);
			jab_int32 diff1 = abs(rg - c1rg) + abs(rb - c1rb) + abs(gb - c1gb);

			jab_int32 c2rg = abs(palette[color_number*3*p_index + index2*3 + 0] - palette[color_number*3*p_index + index2*3 + 1]);
			jab_int32 c2rb = abs(palette[color_number*3*p_index + index2*3 + 0] - palette[color_number*3*p_index + index2*3 + 2]);
			jab_int32 c2gb = abs(palette[color_number*3*p_index + index2*3 + 1] - palette[color_number*3*p_index + index2*3 + 2]);
			jab_int32 diff2 = abs(rg - c2rg) + abs(rb - c2rb) + abs(gb - c2gb);

			if(diff2 < diff1)
				index1 = index2;
			//printf("final: %d\n", index1);
		}
	}
	else	//if no palette is available, decode the module as black/white
	{
		index1 = ((rgb[0] > 100 ? 1 : 0) + (rgb[1] > 100 ? 1 : 0) + (rgb[2] > 100 ? 1 : 0)) > 1 ? 1 : 0;
	}
	return index1;
}

/**
 * @brief Decode a module for PartI (Nc) of the metadata of master symbol
 * @param rgb the pixel value in RGB format
 * @return the decoded value
*/
jab_byte decodeModuleNc(jab_byte* rgb)
{
	jab_int32 ths_black = 80;
	jab_double ths_std = 0.08;
	//check black pixel
	if(rgb[0] < ths_black && rgb[1] < ths_black && rgb[2] < ths_black)
	{
		return 0;//000
	}
	//check color
	jab_double ave, var;
	getAveVar(rgb, &ave, &var);
	jab_double std = sqrt(var);	//standard deviation
	jab_byte min, mid, max;
	jab_int32 index_min, index_mid, index_max;
	getMinMax(rgb, &min, &mid, &max, &index_min, &index_mid, &index_max);
	std /= (jab_double)max;	//normalize std
	jab_byte bits[3];
	if(std > ths_std)
	{
		bits[index_max] = 1;
		bits[index_min] = 0;
		jab_double r1 = (jab_double)rgb[index_mid] / (jab_double)rgb[index_min];
		jab_double r2 = (jab_double)rgb[index_max] / (jab_double)rgb[index_mid];
		if(r1 > r2)
			bits[index_mid] = 1;
		else
			bits[index_mid] = 0;
	}
	else
	{
		return 7;//111
	}
	return ((bits[0] << 2) + (bits[1] << 1) + bits[2]);
}

/**
 * @brief Decode a module using soft decision
 * @param palette the color palette
 * @param color_number the number of colors
 * @param ths the pixel value thresholds
 * @param rp the reference pixel values
 * @param rgb the pixel value in RGB format
 * @param p the probability of the reliability of the decoded bits
 * @return the decoded value
*/
jab_byte decodeModule(jab_byte* palette, jab_int32 color_number, jab_float* ths, jab_float* rp, jab_byte* rgb, jab_float* p)
{
	jab_int32 vs[3] = {0};	//the number of variable colors for r, g, b channels
	switch(color_number)
	{
	case 2:
	case 4:
	case 8:
		vs[0] = 2;
		vs[1] = 2;
		vs[2] = 2;
		break;
	case 16:
		vs[0] = 4;
		vs[1] = 2;
		vs[2] = 2;
		break;
	case 32:
		vs[0] = 4;
		vs[1] = 4;
		vs[2] = 2;
		break;
	case 64:
		vs[0] = 4;
		vs[1] = 4;
		vs[2] = 4;
		break;
	case 128:
		vs[0] = 8;
		vs[1] = 4;
		vs[2] = 4;
		break;
	case 256:
		vs[0] = 8;
		vs[1] = 8;
		vs[2] = 4;
		break;
	}

	jab_byte index = 0;
	jab_float cp[3] = {0.0f};
	jab_byte cv[3] = {0};
	if(color_number < 16)
	{
		jab_int32 ths_offset = 0;
		for(jab_int32 ch=0; ch<3; ch++) //r, g, b channel
		{
			if(rgb[ch] < ths[ths_offset + 1])
			{
				cp[ch] = 1.0f - rgb[ch] / ths[ths_offset + 1];
				cv[ch] = 0;
			}
			else
			{
				cp[ch] = (rgb[ch] - ths[ths_offset + 1]) / (255.0f - ths[ths_offset + 1]);
				cv[ch] = 1;
			}
			//update offset for threshold
			ths_offset += vs[ch] + 1;
		}
		if(color_number == 2)			//2-color
		{
			index = (cv[0] + cv[1] + cv[2]) > 1 ? 1 : 0;
			p[0] = (cp[0] + cp[1] + cp[2]) / 3.0f;
		}
		else if(color_number == 4)		//4-color
		{
			index = cv[0] * vs[1] + cv[1];
			p[0] = cp[0];
			p[1] = cp[1];
		}
		else							//8-color
		{
			index = cv[0] * vs[1] * vs[2] + cv[1] * vs[2] + cv[2];
			p[0] = cp[0];
			p[1] = cp[1];
			p[2] = cp[2];
		}
	}
	else
	{
		jab_int32 ths_offset = 0;
		jab_int32 rp_offset = 0;
		for(jab_int32 ch=0; ch<3; ch++)			//r, g, b channels
		{
			for(jab_int32 i=0; i<vs[ch]; i++)	//variable colors in each channel
			{
				if(rgb[ch] >= ths[ths_offset + i] && rgb[ch] <= ths[ths_offset + i + 1])
				{
					cv[ch] = i;
					if(i == 0)
						cp[ch] = 1.0f - rgb[ch] / ths[ths_offset + i + 1];
					else if(i == vs[ch] - 1)
						cp[ch] = (rgb[ch] - ths[ths_offset + i]) / (255.0f - ths[ths_offset + i]);
					else
					{
						if(rgb[ch] <= rp[rp_offset + i - 1])
							cp[ch] = (rgb[ch] - ths[ths_offset + i]) / (rp[rp_offset + i - 1] - ths[ths_offset + i]);
						else
							cp[ch] = (ths[ths_offset + i + 1] - rgb[ch]) / (ths[ths_offset + i + 1] - rp[rp_offset + i - 1]);
					}
				}
			}
			//update offset for threshold and reference point
			ths_offset += vs[ch] + 1;
			rp_offset += vs[ch] - 2;
		}
		//get the palette index of c
		index = cv[0] * vs[1] * vs[2] + cv[1] * vs[2] + cv[2];
		//get probability for each bit
		jab_int32 bits_count = (jab_int32)(log(color_number) / log(2));
		for(jab_int32 i=0; i<bits_count; i++)
			p[i] = (cp[0] + cp[1] + cp[2]) / 3.0f;
	}
	return index;
}

/**
 * @brief Get the pixel value thresholds and reference points for each channel of the colors in the palette
 * @param palette the color palette
 * @param color_number the number of colors
 * @param palette_ths the pixel value thresholds
 * @param palette_rp the reference pixel values
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean getPaletteThreshold(jab_byte* palette, jab_int32 color_number, jab_float** palette_ths, jab_float** palette_rp)
{
	jab_int32 vs[3] = {0};	//the number of variable colors for r, g, b channels
	switch(color_number)
	{
	case 2:
	case 4:
	case 8:
		vs[0] = 2;
		vs[1] = 2;
		vs[2] = 2;
		break;
	case 16:
		vs[0] = 4;
		vs[1] = 2;
		vs[2] = 2;
		break;
	case 32:
		vs[0] = 4;
		vs[1] = 4;
		vs[2] = 2;
		break;
	case 64:
		vs[0] = 4;
		vs[1] = 4;
		vs[2] = 4;
		break;
	case 128:
		vs[0] = 8;
		vs[1] = 4;
		vs[2] = 4;
		break;
	case 256:
		vs[0] = 8;
		vs[1] = 8;
		vs[2] = 4;
		break;
	}

	jab_int32 ths_size = (vs[0] + 1) + (vs[1] + 1) + (vs[2] + 1); //the number of thresholds for all channels
	jab_int32 rp_size  = (vs[0] - 2) + (vs[1] - 2) + (vs[2] - 2); //the number of reference points for all channels

	(*palette_ths) = (jab_float*)malloc(sizeof(jab_float)*ths_size);
    if((*palette_ths) == NULL)
    {
		reportError("Memory allocation for palette thresholds failed");
		return JAB_FAILURE;
    }
    if(rp_size == 0)
	{
		(*palette_rp) = 0;
	}
	else
	{
		(*palette_rp) = (jab_float*)malloc(sizeof(jab_float)*rp_size);
		if((*palette_rp) == NULL)
		{
			reportError("Memory allocation for palette reference points failed");
			return JAB_FAILURE;
		}
	}

	if(color_number == 2)
	{
		jab_int32 ths_offset = 0;
		for(jab_int32 ch=0; ch<3; ch++)
		{
			(*palette_ths)[ths_offset + 0] = 0;
			(*palette_ths)[ths_offset + 1] = (jab_float)(palette[ch] + palette[3 + ch]) / 2.0f;
			(*palette_ths)[ths_offset + 2] = 255;
			//update offset for threshold and reference point
			ths_offset += vs[ch] + 1;
		}
	}
	else if(color_number == 4)
	{
		jab_int32 cpr0 = MAX(palette[0], palette[3]);
		jab_int32 cpr1 = MIN(palette[6], palette[9]);
		jab_int32 cpg0 = MAX(palette[1], palette[7]);
		jab_int32 cpg1 = MIN(palette[4], palette[10]);
		jab_int32 cpb0 = MAX(palette[8], palette[11]);
		jab_int32 cpb1 = MIN(palette[2], palette[5]);

		(*palette_ths)[0] = 0;
		(*palette_ths)[1] = (cpr0 + cpr1) / 2.0f;
		(*palette_ths)[2] = 255;
		(*palette_ths)[3] = 0;
		(*palette_ths)[4] = (cpg0 + cpg1) / 2.0f;
		(*palette_ths)[5] = 255;
		(*palette_ths)[6] = 0;
		(*palette_ths)[7] = (cpb0 + cpb1) / 2.0f;
		(*palette_ths)[8] = 255;
	}
	else if(color_number == 8)
	{
		jab_int32 cpr0 = MAX(MAX(MAX(palette[0], palette[3]), palette[6]), palette[9]);
		jab_int32 cpr1 = MIN(MIN(MIN(palette[12], palette[15]), palette[18]), palette[21]);
		jab_int32 cpg0 = MAX(MAX(MAX(palette[1], palette[4]), palette[13]), palette[16]);
		jab_int32 cpg1 = MIN(MIN(MIN(palette[7], palette[10]), palette[19]), palette[22]);
		jab_int32 cpb0 = MAX(MAX(MAX(palette[2], palette[8]), palette[14]), palette[20]);
		jab_int32 cpb1 = MIN(MIN(MIN(palette[5], palette[11]), palette[17]), palette[23]);

		(*palette_ths)[0] = 0;
		(*palette_ths)[1] = (cpr0 + cpr1) / 2.0f;
		(*palette_ths)[2] = 255;
		(*palette_ths)[3] = 0;
		(*palette_ths)[4] = (cpg0 + cpg1) / 2.0f;
		(*palette_ths)[5] = 255;
		(*palette_ths)[6] = 0;
		(*palette_ths)[7] = (cpb0 + cpb1) / 2.0f;
		(*palette_ths)[8] = 255;
	}
	else //more than 8 colors
	{
		//calculate critical points
		jab_int32 cps_size = (vs[0] - 1) * 2 + (vs[1] - 1) * 2 + (vs[2] - 1) * 2; //the number of critical points for all channels
		jab_int32* cps = (jab_int32 *)malloc(cps_size * sizeof(jab_int32));
		if(cps == NULL)
		{
			reportError("Memory allocation for critical points failed");
			return JAB_FAILURE;
		}
		jab_int32 cps_offset = 0;
		for(jab_int32 ch=0; ch<3; ch++)
		{
			jab_int32 block, step;
			switch(ch)
			{
			case 0:
				block = vs[1] * vs[2];	//block size for one channel value
				step = vs[0] * block;	//step size for next block
				break;
			case 1:
				block = vs[2];
				step = vs[1] * block;
				break;
			case 2:
				block = 1;
				step = vs[2];
				break;
			}
			jab_int32 cps_count = (vs[ch] - 1) * 2; //the number of critical points in one channel
			jab_int32 cps_index = 0;
			jab_int32 min = 255;
			jab_int32 max = 0;
			//calculate min and max for each possible pixel value in this channel
			for(jab_int32 i=0; i<vs[ch]; i++)
			{
				min = 255;
				max = 0;
				for(jab_int32 j=i*block; j<color_number; j+=step)
				{
					for(jab_int32 k=0; k<block; k++)
					{
						if(palette[3*(j+k) + ch] < min)
							min = palette[3*(j+k) + ch];
						if(palette[3*(j+k) + ch] > max)
							max = palette[3*(j+k) + ch];
					}
				}
				if(cps_index == 0)
				{
					cps[cps_offset + cps_index] = max;
					cps_index++;
				}
				else if(cps_index == cps_count - 1)
				{
					cps[cps_offset + cps_index] = min;
				}
				else
				{
					cps[cps_offset + cps_index] = min;
					cps[cps_offset + cps_index + 1] = max;
					cps_index += 2;
				}
			}
			cps_offset += cps_count;
		}

		//get thresholds and reference points
		cps_offset = 0;
		jab_int32 ths_offset = 0;
		jab_int32 rp_offset = 0;
		for(jab_int32 ch=0; ch<3; ch++)
		{
			//set the first threshold
			(*palette_ths)[ths_offset + 0] = 0;
			//set the other thresholds
			jab_int32 cps_index = 0;
			for(jab_int32 i=1; i<vs[ch]; i++)
			{
				(*palette_ths)[ths_offset + i] = (cps[cps_offset + cps_index] + cps[cps_offset + cps_index + 1]) / 2.0f;
				//set reference points
				if(i != vs[ch] - 1)
				{
					(*palette_rp)[rp_offset + i - 1] = (cps[cps_offset + cps_index + 1] + cps[cps_offset + cps_index + 2]) / 2.0f;
				}
				cps_index += 2;
			}
			//set the last threshold
			(*palette_ths)[ths_offset + vs[ch]] = 255;
			//update offset for threshold and reference point
			cps_offset += (vs[ch] - 1) * 2;
			ths_offset += vs[ch] + 1;
			rp_offset += vs[ch] - 2;
		}
		free(cps);
	}
	return JAB_SUCCESS;
}

/**
 * @brief Get the coordinate of the next metadata module in master symbol
 * @param matrix_height the height of the matrix
 * @param matrix_width the width of the matrix
 * @param next_module_count the index number of the next module
 * @param x the x coordinate of the current and the next module
 * @param y the y coordinate of the current and the next module
*/
void getNextMetadataModuleInMaster(jab_int32 matrix_height, jab_int32 matrix_width, jab_int32 next_module_count, jab_int32* x, jab_int32* y)
{
	if(next_module_count % 4 == 0 || next_module_count % 4 == 2)
	{
		(*y) = matrix_height - 1 - (*y);
	}
	if(next_module_count % 4 == 1 || next_module_count % 4 == 3)
	{
		(*x) = matrix_width -1 - (*x);
	}
	if(next_module_count % 4 == 0)
	{
        if( next_module_count <= 20 ||
           (next_module_count >= 44  && next_module_count <= 68)  ||
           (next_module_count >= 96  && next_module_count <= 124) ||
           (next_module_count >= 156 && next_module_count <= 172))
		{
			(*y) += 1;
		}
        else if((next_module_count > 20  && next_module_count < 44) ||
                (next_module_count > 68  && next_module_count < 96) ||
                (next_module_count > 124 && next_module_count < 156))
		{
			(*x) -= 1;
		}
	}
    if(next_module_count == 44 || next_module_count == 96 || next_module_count == 156)
    {
        jab_int32 tmp = (*x);
        (*x) = (*y);
        (*y) = tmp;
    }
}

/**
 * @brief Decode slave symbol metadata
 * @param host_symbol the host symbol
 * @param docked_position the docked position
 * @param data the data stream of the host symbol
 * @param offset the metadata start offset in the data stream
 * @return the read metadata bit length | DECODE_METADATA_FAILED
*/
jab_int32 decodeSlaveMetadata(jab_decoded_symbol* host_symbol, jab_int32 docked_position, jab_data* data, jab_int32 offset)
{
	//set metadata from host symbol
	host_symbol->slave_metadata[docked_position].Nc = host_symbol->metadata.Nc;
	host_symbol->slave_metadata[docked_position].mask_type = host_symbol->metadata.mask_type;
	host_symbol->slave_metadata[docked_position].docked_position = 0;

	//decode metadata
	jab_int32 index = offset;
	jab_uint32 SS, SE, V, E;

	//parse part1
	if(index < 0) return DECODE_METADATA_FAILED;
	SS = data->data[index--];//SS
	if(SS == 0)
	{
		host_symbol->slave_metadata[docked_position].side_version = host_symbol->metadata.side_version;
	}
	if(index < 0) return DECODE_METADATA_FAILED;
	SE = data->data[index--];//SE
	if(SE == 0)
	{
		host_symbol->slave_metadata[docked_position].ecl = host_symbol->metadata.ecl;
	}
	//decode part2 if it exists
	if(SS == 1)
	{
		if((index-4) < 0) return DECODE_METADATA_FAILED;
		V = 0;
		for(jab_int32 i=0; i<5; i++)
		{
			V += data->data[index--] << (4 - i);
		}
		jab_int32 side_version = V + 1;
		if(docked_position == 2 || docked_position == 3)
		{
			host_symbol->slave_metadata[docked_position].side_version.y = host_symbol->metadata.side_version.y;
			host_symbol->slave_metadata[docked_position].side_version.x = side_version;
		}
		else
		{
			host_symbol->slave_metadata[docked_position].side_version.x = host_symbol->metadata.side_version.x;
			host_symbol->slave_metadata[docked_position].side_version.y = side_version;
		}
	}
	if(SE == 1)
	{
		if((index-5) < 0) return DECODE_METADATA_FAILED;
		//get wc (the first half of E)
		E = 0;
		for(jab_int32 i=0; i<3; i++)
		{
			E += data->data[index--] << (2 - i);
		}
		host_symbol->slave_metadata[docked_position].ecl.x = E + 3;	//wc = E_part1 + 3
		//get wr (the second half of E)
		E = 0;
		for(jab_int32 i=0; i<3; i++)
		{
			E += data->data[index--] << (2 - i);
		}
		host_symbol->slave_metadata[docked_position].ecl.y = E + 4;	//wr = E_part2 + 4

		//check wc and wr
		jab_int32 wc = host_symbol->slave_metadata[docked_position].ecl.x;
		jab_int32 wr = host_symbol->slave_metadata[docked_position].ecl.y;
		if(wc >= wr)
		{
			reportError("Incorrect error correction parameter in slave metadata");
			return DECODE_METADATA_FAILED;
		}
	}
	return (offset - index);
}

/**
 * @brief Decode the encoded bits of Nc from the module color
 * @param module1_color the color of the first module
 * @param module2_color the color of the second module
 * @return the decoded bits
*/
jab_byte decodeNcModuleColor(jab_byte module1_color, jab_byte module2_color)
{
	for(jab_int32 i=0; i<8; i++)
	{
		if(module1_color == nc_color_encode_table[i][0] && module2_color == nc_color_encode_table[i][1])
			return i;
	}
	return 8; //if no match, return an invalid value
}

/**
 * @brief Decode the PartI of master symbol metadata
 * @param matrix the symbol matrix
 * @param symbol the master symbol
 * @param data_map the data module positions
 * @param module_count the index number of the next module
 * @param x the x coordinate of the current and the next module
 * @param y the y coordinate of the current and the next module
 * @return JAB_SUCCESS | JAB_FAILURE | DECODE_METADATA_FAILED
*/
jab_int32 decodeMasterMetadataPartI(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_int32* module_count, jab_int32* x, jab_int32* y)
{
	//decode Nc module color
	jab_byte module_color[MASTER_METADATA_PART1_MODULE_NUMBER];
	jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
    jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;
    jab_int32 mtx_offset;
	while((*module_count) < MASTER_METADATA_PART1_MODULE_NUMBER)
	{
		//decode bit out of the module at (x,y)
		mtx_offset = (*y) * mtx_bytes_per_row + (*x) * mtx_bytes_per_pixel;
		jab_byte rgb =  decodeModuleNc(&matrix->pixel[mtx_offset]);
		if(rgb != 0 && rgb != 3 && rgb != 6)
		{
#if TEST_MODE
		reportError("Invalid module color in primary metadata part 1 found");
#endif
			return DECODE_METADATA_FAILED;
		}
		module_color[*module_count] = rgb;
		//set data map
		data_map[(*y) * matrix->width + (*x)] = 1;
		//go to the next module
		(*module_count)++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, (*module_count), x, y);
	}

	//decode encoded Nc
	jab_byte bits[2];
	bits[0] = decodeNcModuleColor(module_color[0], module_color[1]);	//the first 3 bits
	bits[1] = decodeNcModuleColor(module_color[2], module_color[3]);	//the last 3 bits
	if(bits[0] > 7 || bits[1] > 7)
	{
#if TEST_MODE
		reportError("Invalid color combination in primary metadata part 1 found");
#endif
		return DECODE_METADATA_FAILED;
	}
	//set bits in part1
	jab_byte part1[MASTER_METADATA_PART1_LENGTH] = {0};			//6 encoded bits
	jab_int32 bit_count = 0;
	for(jab_int32 n=0; n<2; n++)
	{
		for(jab_int32 i=0; i<3; i++)
		{
			jab_byte bit = (bits[n] >> (2 - i)) & 0x01;
			part1[bit_count] = bit;
			bit_count++;
		}
	}

	//decode ldpc for part1
	if( !decodeLDPChd(part1, MASTER_METADATA_PART1_LENGTH, MASTER_METADATA_PART1_LENGTH > 36 ? 4 : 3, 0) )
	{
#if TEST_MODE
		reportError("LDPC decoding for master metadata part 1 failed");
#endif
		return JAB_FAILURE;
	}
	//parse part1
	symbol->metadata.Nc = (part1[0] << 2) + (part1[1] << 1) + part1[2];

	return JAB_SUCCESS;
}

/**
 * @brief Decode the PartII of master symbol metadata
 * @param matrix the symbol matrix
 * @param symbol the master symbol
 * @param data_map the data module positions
 * @param cs the color changing slopes
 * @param module_count the index number of the next module
 * @param x the x coordinate of the current and the next module
 * @param y the y coordinate of the current and the next module
 * @return JAB_SUCCESS | JAB_FAILURE | DECODE_METADATA_FAILED | FATAL_ERROR
*/
jab_int32 decodeMasterMetadataPartII(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_float* cs, jab_int32* module_count, jab_int32* x, jab_int32* y)
{
	jab_byte part2[MASTER_METADATA_PART2_LENGTH] = {0};			//38 encoded bits
	jab_int32 part2_bit_count = 0;
	jab_uint32 V, E;
	jab_uint32 V_length = 10, E_length = 6;

	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	jab_int32 bits_per_module = (jab_int32)(log(color_number) / log(2));

    //read part2
    while(part2_bit_count < MASTER_METADATA_PART2_LENGTH)
    {
		//decode bits out of the module at (x,y)
		jab_byte bits = decodeModuleHD(matrix, symbol->palette, color_number, cs, *x, *y);
		//write the bits into part2
		for(jab_int32 i=0; i<bits_per_module; i++)
		{
			jab_byte bit = (bits >> (bits_per_module - 1 - i)) & 0x01;
			if(part2_bit_count < MASTER_METADATA_PART2_LENGTH)
			{
				part2[part2_bit_count] = bit;
				part2_bit_count++;
			}
			else	//if part2 is full, stop
			{
				break;
			}
		}
		//set data map
		data_map[(*y) * matrix->width + (*x)] = 1;
		//go to the next module
		(*module_count)++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, (*module_count), x, y);
    }

	//decode ldpc for part2
	if( !decodeLDPChd(part2, MASTER_METADATA_PART2_LENGTH, MASTER_METADATA_PART2_LENGTH > 36 ? 4 : 3, 0) )
	{
#if TEST_MODE
		reportError("LDPC decoding for master metadata part 2 failed");
#endif
		return DECODE_METADATA_FAILED;
	}

    //parse part2
	//read V
	//get horizontal side version
	V = 0;
	for(jab_int32 i=0; i<V_length/2; i++)
	{
		V += part2[i] << (V_length/2 - 1 - i);
	}
	symbol->metadata.side_version.x = V + 1;
	//get vertical side version
	V = 0;
	for(jab_int32 i=0; i<V_length/2; i++)
	{
		V += part2[i+V_length/2] << (V_length/2 - 1 - i);
	}
	symbol->metadata.side_version.y = V + 1;

	//read E
	jab_int32 bit_index = V_length;
	//get wc (the first half of E)
	E = 0;
	for(jab_int32 i=bit_index; i<(bit_index+E_length/2); i++)
	{
		E += part2[i] << (E_length/2 - 1 - (i - bit_index));
	}
	symbol->metadata.ecl.x = E + 3;		//wc = E_part1 + 3
	//get wr (the second half of E)
	E = 0;
	for(jab_int32 i=bit_index; i<(bit_index+E_length/2); i++)
	{
		E += part2[i+E_length/2] << (E_length/2 - 1 - (i - bit_index));
	}
	symbol->metadata.ecl.y = E + 4;		//wr = E_part2 + 4

	//read MSK
	bit_index = V_length + E_length;
	symbol->metadata.mask_type = (part2[bit_index+0] << 2) + (part2[bit_index+1] << 1) + part2[bit_index+2];

	symbol->metadata.docked_position = 0;

	//check side version
	symbol->side_size.x = VERSION2SIZE(symbol->metadata.side_version.x);
	symbol->side_size.y = VERSION2SIZE(symbol->metadata.side_version.y);
	if(matrix->width != symbol->side_size.x || matrix->height != symbol->side_size.y)
	{
		reportError("Primary symbol matrix size does not match the metadata");
		return JAB_FAILURE;
	}
	//check wc and wr
	jab_int32 wc = symbol->metadata.ecl.x;
	jab_int32 wr = symbol->metadata.ecl.y;
	if(wc >= wr)
	{
		reportError("Incorrect error correction parameter in primary symbol metadata");
		return DECODE_METADATA_FAILED;
	}
	return JAB_SUCCESS;
}

/**
 * @brief Decode data modules
 * @param matrix the symbol matrix
 * @param symbol the symbol to be decoded
 * @param data_map the data module positions
 * @param cs the color changing slopes
 * @return the decoded data | NULL if failed
*/
jab_data* readRawModuleData(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_float* cs)
{
    jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	jab_int32 module_count = 0;
    jab_data* data = (jab_data*)malloc(sizeof(jab_data) + matrix->width * matrix->height * sizeof(jab_char));
    if(data == NULL)
	{
		reportError("Memory allocation for raw module data failed");
		return NULL;
	}
#if TEST_MODE
	FILE* fp = fopen("jab_dec_module_rgb.bin", "wb");
#endif // TEST_MODE
	for(jab_int32 j=0; j<matrix->width; j++)
	{
		for(jab_int32 i=0; i<matrix->height; i++)
		{
			if(data_map[i*matrix->width + j] == 0)
			{
				//decode bits out of the module at (x,y)
				jab_byte bits = decodeModuleHD(matrix, symbol->palette, color_number, cs, j, i);
				//write the bits into data
				data->data[module_count] = (jab_char)bits;
				module_count++;
			}
#if TEST_MODE
			if(data_map[i*matrix->width + j] == 0)
			{
				jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
				jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;
				jab_int32 mtx_offset = i * mtx_bytes_per_row + j * mtx_bytes_per_pixel;
				fwrite(&matrix->pixel[mtx_offset], 3, 1, fp);
			}
			else
			{
				jab_byte rgb[3] = {128,128,128};
				fwrite(rgb, 3, 1, fp);
			}
#endif // TEST_MODE
		}
	}
#if TEST_MODE
	fclose(fp);
#endif // TEST_MODE
	data->length = module_count;
	return data;
}

/**
 * @brief Convert multi-bit-per-byte raw module data to one-bit-per-byte raw data
 * @param raw_module_data the input raw module data
 * @param bits_per_module the number of bits per module
 * @return the converted data | NULL if failed
*/
jab_data* rawModuleData2RawData(jab_data* raw_module_data, jab_int32 bits_per_module)
{
	//
	jab_data* raw_data = (jab_data *)malloc(sizeof(jab_data) + raw_module_data->length * bits_per_module * sizeof(jab_char));
    if(raw_data == NULL)
	{
		reportError("Memory allocation for raw data failed");
		return NULL;
	}
	for(jab_int32 i=0; i<raw_module_data->length; i++)
	{
		for(jab_int32 j=0; j<bits_per_module; j++)
		{
			raw_data->data[i * bits_per_module + j] = (raw_module_data->data[i] >> (bits_per_module - 1 - j)) & 0x01;
		}
	}
	raw_data->length = raw_module_data->length * bits_per_module;
	return raw_data;
}

/**
 * @brief Mark the positions of finder patterns and alignment patterns in the data map
 * @param data_map the data module positions
 * @param width the width of the data map
 * @param height the height of the data map
 * @param type the symbol type, 0: master, 1: slave
*/
void fillDataMap(jab_byte* data_map, jab_int32 width, jab_int32 height, jab_int32 type)
{
	//calculate the number of alignment patterns between the finder patterns
    jab_int32 number_of_ap_x = (width - (DISTANCE_TO_BORDER*2 - 1)) / MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS - 1;
    jab_int32 number_of_ap_y = (height- (DISTANCE_TO_BORDER*2 - 1)) / MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS - 1;
	if(number_of_ap_x < 0)
		number_of_ap_x = 0;
	if(number_of_ap_y < 0)
		number_of_ap_y = 0;
	//add the finder patterns
	number_of_ap_x += 2;
	number_of_ap_y += 2;
	//calculate the distance between alignment patters
    jab_float ap_distance_x = 0.0f;
    jab_float ap_distance_y = 0.0f;
    if(number_of_ap_x > 2)
        ap_distance_x = (jab_float)(width - (DISTANCE_TO_BORDER*2 - 1)) / (jab_float)(number_of_ap_x - 1);
	else
		ap_distance_x = (jab_float)(width - (DISTANCE_TO_BORDER*2 - 1));
    if(number_of_ap_y > 2)
        ap_distance_y = (jab_float)(height- (DISTANCE_TO_BORDER*2 - 1)) / (jab_float)(number_of_ap_y - 1);
	else
		ap_distance_y = (jab_float)(height- (DISTANCE_TO_BORDER*2 - 1));

    for(jab_int32 i=0; i<number_of_ap_y; i++)
    {
		for(jab_int32 j=0; j<number_of_ap_x; j++)
		{
			//the center coordinate
			jab_int32 x_offset = (DISTANCE_TO_BORDER - 1) + j * ap_distance_x;
			jab_int32 y_offset = (DISTANCE_TO_BORDER - 1) + i * ap_distance_y;
			//the cross
			data_map[y_offset 		* width + x_offset]		  =
			data_map[y_offset		* width + (x_offset - 1)] =
			data_map[y_offset		* width + (x_offset + 1)] =
			data_map[(y_offset - 1) * width + x_offset] 	  =
			data_map[(y_offset + 1) * width + x_offset] 	  = 1;

			//the diagonal modules
			if(i == 0 && (j == 0 || j == number_of_ap_x - 1))	//at finder pattern 0 and 1 positions
			{
				data_map[(y_offset - 1) * width + (x_offset - 1)] =
				data_map[(y_offset + 1) * width + (x_offset + 1)] = 1;
				if(type == 0)	//master symbol
				{
					data_map[(y_offset - 2) * width + (x_offset - 2)] =
					data_map[(y_offset - 2) * width + (x_offset - 1)] =
					data_map[(y_offset - 2) * width +  x_offset] 	  =
					data_map[(y_offset - 1) * width + (x_offset - 2)] =
					data_map[ y_offset		* width + (x_offset - 2)] = 1;

					data_map[(y_offset + 2) * width + (x_offset + 2)] =
					data_map[(y_offset + 2) * width + (x_offset + 1)] =
					data_map[(y_offset + 2) * width +  x_offset] 	  =
					data_map[(y_offset + 1) * width + (x_offset + 2)] =
					data_map[ y_offset		* width + (x_offset + 2)] = 1;
				}
			}
			else if(i == number_of_ap_y - 1 && (j == 0 || j == number_of_ap_x - 1))	//at finder pattern 2 and 3 positions
			{
				data_map[(y_offset - 1) * width + (x_offset + 1)] =
				data_map[(y_offset + 1) * width + (x_offset - 1)] = 1;
				if(type == 0) 	//master symbol
				{
					data_map[(y_offset - 2) * width + (x_offset + 2)] =
					data_map[(y_offset - 2) * width + (x_offset + 1)] =
					data_map[(y_offset - 2) * width +  x_offset] 	  =
					data_map[(y_offset - 1) * width + (x_offset + 2)] =
					data_map[ y_offset		* width + (x_offset + 2)] = 1;

					data_map[(y_offset + 2) * width + (x_offset - 2)] =
					data_map[(y_offset + 2) * width + (x_offset - 1)] =
					data_map[(y_offset + 2) * width +  x_offset] 	  =
					data_map[(y_offset + 1) * width + (x_offset - 2)] =
					data_map[ y_offset		* width + (x_offset - 2)] = 1;
				}
			}
			else	//at other positions
			{
				//even row, even column / odd row, odd column
				if( (i % 2 == 0 && j % 2 == 0) || (i % 2 == 1 && j % 2 == 1))
				{
					data_map[(y_offset - 1) * width + (x_offset - 1)] =
					data_map[(y_offset + 1) * width + (x_offset + 1)] = 1;
				}
				//odd row, even column / even row, old column
				else
				{
					data_map[(y_offset - 1) * width + (x_offset + 1)] =
					data_map[(y_offset + 1) * width + (x_offset - 1)] = 1;
				}
			}
		}
    }
}

/**
 * @brief Load default metadata values and color palettes for master symbol
 * @param matrix the symbol matrix
 * @param symbol the master symbol
*/
void loadDefaultMasterMetadata(jab_bitmap* matrix, jab_decoded_symbol* symbol)
{
#if TEST_MODE
	JAB_REPORT_INFO(("Loading default master metadata"))
#endif
	//set default metadata values
	symbol->metadata.Nc = DEFAULT_MODULE_COLOR_MODE;
	symbol->metadata.ecl.x = ecclevel2wcwr[DEFAULT_ECC_LEVEL][0];
	symbol->metadata.ecl.y = ecclevel2wcwr[DEFAULT_ECC_LEVEL][1];
	symbol->metadata.mask_type = DEFAULT_MASKING_REFERENCE;
	symbol->metadata.docked_position = 0;							//no default value
	symbol->metadata.side_version.x = SIZE2VERSION(matrix->width);	//no default value
	symbol->metadata.side_version.y = SIZE2VERSION(matrix->height);	//no default value
}

/**
 * @brief Decode symbol
 * @param matrix the symbol matrix
 * @param symbol the symbol to be decoded
 * @param data_map the data module positions
 * @param cs the color changing slopes
 * @param type the symbol type, 0: master, 1: slave
 * @return JAB_SUCCESS | JAB_FAILURE | DECODE_METADATA_FAILED | FATAL_ERROR
*/
jab_int32 decodeSymbol(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_float* cs, jab_int32 type)
{
#if TEST_MODE
	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	printf("p1:\n");
	for(int i=0;i<color_number;i++)
	{
		printf("%d\t%d\t%d\n", symbol->palette[i*3], symbol->palette[i*3+1], symbol->palette[i*3+2]);
	}
	printf("p2:\n");
	for(int i=0;i<color_number;i++)
	{
		printf("%d\t%d\t%d\n", symbol->palette[3*color_number+i*3], symbol->palette[3*color_number+i*3+1], symbol->palette[3*color_number+i*3+2]);
	}
	printf("p3:\n");
	for(int i=0;i<color_number;i++)
	{
		printf("%d\t%d\t%d\n", symbol->palette[3*color_number*2+i*3], symbol->palette[3*color_number*2+i*3+1], symbol->palette[3*color_number*2+i*3+2]);
	}
	printf("p4:\n");
	for(int i=0;i<color_number;i++)
	{
		printf("%d\t%d\t%d\n", symbol->palette[3*color_number*3+i*3], symbol->palette[3*color_number*3+i*3+1], symbol->palette[3*color_number*3+i*3+2]);
	}
#endif // TEST_MODE

	//fill data map
	fillDataMap(data_map, matrix->width, matrix->height, type);

	//read raw data
	jab_data* raw_module_data = readRawModuleData(matrix, symbol, data_map, cs);
	if(raw_module_data == NULL)
	{
		JAB_REPORT_ERROR(("Reading raw module data in symbol %d failed", symbol->index))
		free(data_map);
		return FATAL_ERROR;
	}
#if TEST_MODE
	FILE* fp = fopen("jab_dec_module_data.bin", "wb");
	fwrite(raw_module_data->data, raw_module_data->length, 1, fp);
	fclose(fp);
#endif // TEST_MODE

	//demask
	demaskSymbol(raw_module_data, data_map, symbol->side_size, symbol->metadata.mask_type, (jab_int32)pow(2, symbol->metadata.Nc + 1));
	free(data_map);
#if TEST_MODE
	fp = fopen("jab_demasked_module_data.bin", "wb");
	fwrite(raw_module_data->data, raw_module_data->length, 1, fp);
	fclose(fp);
#endif // TEST_MODE

	//change to one-bit-per-byte representation
	jab_data* raw_data = rawModuleData2RawData(raw_module_data, symbol->metadata.Nc + 1);
	free(raw_module_data);
	if(raw_data == NULL)
	{
		JAB_REPORT_ERROR(("Reading raw data in symbol %d failed", symbol->index))
		return FATAL_ERROR;
	}

	//calculate Pn and Pg
	jab_int32 wc = symbol->metadata.ecl.x;
	jab_int32 wr = symbol->metadata.ecl.y;
    jab_int32 Pg = (raw_data->length / wr) * wr;	//max_gross_payload = floor(capacity / wr) * wr
    jab_int32 Pn = Pg * (wr - wc) / wr;				//code_rate = 1 - wc/wr = (wr - wc)/wr, max_net_payload = max_gross_payload * code_rate

	//deinterleave data
	raw_data->length = Pg;	//drop the padding bits
    deinterleaveData(raw_data);

#if TEST_MODE
	JAB_REPORT_INFO(("wc:%d, wr:%d, Pg:%d, Pn: %d", wc, wr, Pg, Pn))
	fp = fopen("jab_dec_bit_data.bin", "wb");
	fwrite(raw_data->data, raw_data->length, 1, fp);
	fclose(fp);
#endif // TEST_MODE

	//decode ldpc
    if(decodeLDPChd((jab_byte*)raw_data->data, Pg, symbol->metadata.ecl.x, symbol->metadata.ecl.y) != Pn)
    {
		JAB_REPORT_ERROR(("LDPC decoding for data in symbol %d failed", symbol->index))
		free(raw_data);
		return JAB_FAILURE;
	}

	//find the start flag of metadata
	jab_int32 metadata_offset = Pn - 1;
	while(raw_data->data[metadata_offset] == 0)
	{
		metadata_offset--;
	}
	//skip the flag bit
	metadata_offset--;
	//set docked positions in host metadata
	symbol->metadata.docked_position = 0;
	for(jab_int32 i=0; i<4; i++)
	{
		if(type == 1)	//if host is a slave symbol
		{
			if(i == symbol->host_position) continue; //skip host position
		}
		symbol->metadata.docked_position += raw_data->data[metadata_offset--] << (3 - i);
	}
	//decode metadata for docked slave symbols
	for(jab_int32 i=0; i<4; i++)
	{
		if(symbol->metadata.docked_position & (0x08 >> i))
		{
			jab_int32 read_bit_length = decodeSlaveMetadata(symbol, i, raw_data, metadata_offset);
			if(read_bit_length == DECODE_METADATA_FAILED)
			{
				free(raw_data);
				return DECODE_METADATA_FAILED;
			}
			metadata_offset -= read_bit_length;
		}
	}

	//copy the decoded data to symbol
	jab_int32 net_data_length = metadata_offset + 1;
	symbol->data = (jab_data *)malloc(sizeof(jab_data) + net_data_length * sizeof(jab_char));
	if(symbol->data == NULL)
	{
		reportError("Memory allocation for symbol data failed");
		free(raw_data);
		return FATAL_ERROR;
	}
	symbol->data->length = net_data_length;
	memcpy(symbol->data->data, raw_data->data, net_data_length);

	//clean memory
	free(raw_data);
	return JAB_SUCCESS;
}

/**
 * @brief Decode master symbol
 * @param matrix the symbol matrix
 * @param symbol the master symbol
 * @return JAB_SUCCESS | JAB_FAILURE | FATAL_ERROR
*/
jab_int32 decodeMaster(jab_bitmap* matrix, jab_decoded_symbol* symbol)
{
	if(matrix == NULL)
	{
		reportError("Invalid master symbol matrix");
		return FATAL_ERROR;
	}

	//create data map
	jab_byte* data_map = (jab_byte*)calloc(1, matrix->width*matrix->height*sizeof(jab_byte));
	if(data_map == NULL)
	{
		reportError("Memory allocation for data map in master failed");
		return FATAL_ERROR;
	}

	//decode metadata and color palette
	jab_int32 x = MASTER_METADATA_X;
	jab_int32 y = MASTER_METADATA_Y;
	jab_int32 module_count = 0;

	//decode metadata PartI (Nc)
	jab_int32 decode_partI_ret = decodeMasterMetadataPartI(matrix, symbol, data_map, &module_count, &x, &y);
	if(decode_partI_ret == JAB_FAILURE)
	{
		return JAB_FAILURE;
	}
	if(decode_partI_ret == DECODE_METADATA_FAILED)
	{
		//reset variables
		x = MASTER_METADATA_X;
		y = MASTER_METADATA_Y;
		module_count = 0;
		//clear data_map
		memset(data_map, 0, matrix->width*matrix->height*sizeof(jab_byte));
		//load default metadata and color palette
		loadDefaultMasterMetadata(matrix, symbol);
	}

	//read color palettes
    if(readColorPaletteInMaster(matrix, symbol, data_map, &module_count, &x, &y) < 0)
	{
		reportError("Reading color palettes in master symbol failed");
		return JAB_FAILURE;
	}

	//calculate color changing slopes
	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	jab_float cs[COLOR_PALETTE_NUMBER * (color_number*3) * 2];	//color slopes for 4 palettes in x and y directions
	calculateColorSlopes(matrix, symbol->palette, color_number, cs);

	//decode metadata PartII
	if(decode_partI_ret == JAB_SUCCESS)
	{
		if(decodeMasterMetadataPartII(matrix, symbol, data_map, cs, &module_count, &x, &y) <= 0)
		{
			return JAB_FAILURE;
		}
	}

	//decode master symbol
	return decodeSymbol(matrix, symbol, data_map, cs, 0);
}

/**
 * @brief Decode slave symbol
 * @param matrix the symbol matrix
 * @param symbol the slave symbol
 * @return JAB_SUCCESS | JAB_FAILURE | FATAL_ERROR
*/
jab_int32 decodeSlave(jab_bitmap* matrix, jab_decoded_symbol* symbol)
{
	if(matrix == NULL)
	{
		reportError("Invalid slave symbol matrix");
		return FATAL_ERROR;
	}

	//create data map
	jab_byte* data_map = (jab_byte*)calloc(1, matrix->width*matrix->height*sizeof(jab_byte));
	if(data_map == NULL)
	{
		reportError("Memory allocation for data map in slave failed");
		return FATAL_ERROR;
	}

	//read color palettes
	if(readColorPaletteInSlave(matrix, symbol, data_map) < 0)
	{
		reportError("Reading color palettes in slave symbol failed");
		free(data_map);
		return FATAL_ERROR;
	}

	//calculate color changing slope
	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	jab_float cs[COLOR_PALETTE_NUMBER * (color_number*3) * 2];	//color slopes for 4 palettes in x and y directions
	//calculate color changing slopes
	calculateColorSlopes(matrix, symbol->palette, color_number, cs);

	//decode slave symbol
	return decodeSymbol(matrix, symbol, data_map, cs, 1);
}

/**
 * @brief Read bit data
 * @param data the data buffer
 * @param start the start reading offset
 * @param length the length of the data to be read
 * @param value the read data
 * @return the length of the read data
*/
jab_int32 readData(jab_data* data, jab_int32 start, jab_int32 length, jab_int32* value)
{
	jab_int32 i;
	jab_int32 val = 0;
	for(i=start; i<(start + length) && i<data->length; i++)
	{
		val += data->data[i] << (length - 1 - (i - start));
	}
	*value = val;
	return (i - start);
}

/**
 * @brief Interpret decoded bits
 * @param bits the input bits
 * @return the data message
*/
jab_data* decodeData(jab_data* bits)
{
	jab_byte* decoded_bytes = (jab_byte *)malloc(bits->length * sizeof(jab_byte));
	if(decoded_bytes == NULL)
	{
		reportError("Memory allocation for decoded bytes failed");
		return NULL;
	}

	jab_encode_mode mode = Upper;
	jab_encode_mode pre_mode = None;
	jab_int32 index = 0;	//index of input bits
	jab_int32 count = 0;	//index of decoded bytes

	while(index < bits->length)
	{
		//read the encoded value
		jab_boolean flag = 0;
		jab_int32 value = 0;
        jab_int32 n;
        if(mode != Byte)
        {
            n = readData(bits, index, character_size[mode], &value);
            if(n < character_size[mode])	//did not read enough bits
                break;
            //update index
            index += character_size[mode];
        }

		//decode value
		switch(mode)
		{
			case Upper:
				if(value <= 26)
				{
					decoded_bytes[count++] = jab_decoding_table_upper[value];
					if(pre_mode != None)
						mode = pre_mode;
				}
				else
				{
					switch(value)
					{
						case 27:
							mode = Punct;
							pre_mode = Upper;
							break;
						case 28:
							mode = Lower;
							pre_mode = None;
							break;
						case 29:
							mode = Numeric;
							pre_mode = None;
							break;
						case 30:
							mode = Alphanumeric;
							pre_mode = None;
							break;
						case 31:
							//read 2 bits more
							n = readData(bits, index, 2, &value);
							if(n < 2)	//did not read enough bits
							{
								flag = 1;
								break;
							}
							//update index
							index += 2;
							switch(value)
							{
								case 0:
									mode = Byte;
									pre_mode = Upper;
									break;
								case 1:
									mode = Mixed;
									pre_mode = Upper;
									break;
								case 2:
									mode = ECI;
									pre_mode = None;
									break;
								case 3:
									flag = 1;		//end of message (EOM)
									break;
							}
							break;
						default:
							reportError("Invalid value decoded");
							free(decoded_bytes);
							return NULL;
					}
				}
				break;
			case Lower:
				if(value <= 26)
				{
					decoded_bytes[count++] = jab_decoding_table_lower[value];
					if(pre_mode != None)
						mode = pre_mode;
				}
				else
				{
					switch(value)
					{
						case 27:
							mode = Punct;
							pre_mode = Lower;
							break;
						case 28:
							mode = Upper;
							pre_mode = Lower;
							break;
						case 29:
							mode = Numeric;
							pre_mode = None;
							break;
						case 30:
							mode = Alphanumeric;
							pre_mode = None;
							break;
						case 31:
							//read 2 bits more
							n = readData(bits, index, 2, &value);
							if(n < 2)	//did not read enough bits
							{
								flag = 1;
								break;
							}
							//update index
							index += 2;
							switch(value)
							{
								case 0:
									mode = Byte;
									pre_mode = Lower;
									break;
								case 1:
									mode = Mixed;
									pre_mode = Lower;
									break;
								case 2:
									mode = Upper;
									pre_mode = None;
									break;
								case 3:
									mode = FNC1;
									pre_mode = None;
									break;
							}
							break;
						default:
							reportError("Invalid value decoded");
							free(decoded_bytes);
							return NULL;
					}
				}
				break;
			case Numeric:
				if(value <= 12)
				{
					decoded_bytes[count++] = jab_decoding_table_numeric[value];
					if(pre_mode != None)
						mode = pre_mode;
				}
				else
				{
					switch(value)
					{
						case 13:
							mode = Punct;
							pre_mode = Numeric;
							break;
						case 14:
							mode = Upper;
							pre_mode = None;
							break;
						case 15:
							//read 2 bits more
							n = readData(bits, index, 2, &value);
							if(n < 2)	//did not read enough bits
							{
								flag = 1;
								break;
							}
							//update index
							index += 2;
							switch(value)
							{
								case 0:
									mode = Byte;
									pre_mode = Numeric;
									break;
								case 1:
									mode = Mixed;
									pre_mode = Numeric;
									break;
								case 2:
									mode = Upper;
									pre_mode = Numeric;
									break;
								case 3:
									mode = Lower;
									pre_mode = None;
									break;
							}
							break;
						default:
							reportError("Invalid value decoded");
							free(decoded_bytes);
							return NULL;
					}
				}
				break;
			case Punct:
				if(value >=0 && value <= 15)
				{
					decoded_bytes[count++] = jab_decoding_table_punct[value];
					mode = pre_mode;
				}
				else
				{
					reportError("Invalid value decoded");
					free(decoded_bytes);
					return NULL;
				}
				break;
			case Mixed:
				if(value >=0 && value <= 31)
				{
					if(value == 19)
					{
						decoded_bytes[count++] = 10;
						decoded_bytes[count++] = 13;
					}
					else if(value == 20)
					{
						decoded_bytes[count++] = 44;
						decoded_bytes[count++] = 32;
					}
					else if(value == 21)
					{
						decoded_bytes[count++] = 46;
						decoded_bytes[count++] = 32;
					}
					else if(value == 22)
					{
						decoded_bytes[count++] = 58;
						decoded_bytes[count++] = 32;
					}
					else
					{
						decoded_bytes[count++] = jab_decoding_table_mixed[value];
					}
					mode = pre_mode;
				}
				else
				{
					reportError("Invalid value decoded");
					free(decoded_bytes);
					return NULL;
				}
				break;
			case Alphanumeric:
				if(value <= 62)
				{
					decoded_bytes[count++] = jab_decoding_table_alphanumeric[value];
					if(pre_mode != None)
						mode = pre_mode;
				}
				else if(value == 63)
				{
					//read 2 bits more
					n = readData(bits, index, 2, &value);
					if(n < 2)	//did not read enough bits
					{
						flag = 1;
						break;
					}
					//update index
					index += 2;
					switch(value)
					{
						case 0:
							mode = Byte;
							pre_mode = Alphanumeric;
							break;
						case 1:
							mode = Mixed;
							pre_mode = Alphanumeric;
							break;
						case 2:
							mode = Punct;
							pre_mode = Alphanumeric;
							break;
						case 3:
							mode = Upper;
							pre_mode = None;
							break;
					}
				}
				else
				{
					reportError("Invalid value decoded");
					free(decoded_bytes);
					return NULL;
				}
				break;
			case Byte:
			{
				//read 4 bits more
				n = readData(bits, index, 4, &value);
				if(n < 4)	//did not read enough bits
				{
                    reportError("Not enough bits to decode");
					free(decoded_bytes);
					return NULL;
				}
				//update index
				index += 4;
				if(value == 0)		//read the next 13 bits
				{
					//read 13 bits more
					n = readData(bits, index, 13, &value);
					if(n < 13)	//did not read enough bits
					{
                        reportError("Not enough bits to decode");
						free(decoded_bytes);
						return NULL;
					}
                    value += 15+1;	//the number of encoded bytes = value + 15
					//update index
					index += 13;
				}
				jab_int32 byte_length = value;
				//read the next (byte_length * 8) bits
				for(jab_int32 i=0; i<byte_length; i++)
				{
					n = readData(bits, index, 8, &value);
					if(n < 8)	//did not read enough bits
					{
                        reportError("Not enough bits to decode");
						free(decoded_bytes);
						return NULL;
					}
					//update index
					index += 8;
					decoded_bytes[count++] = (jab_byte)value;
				}
				mode = pre_mode;
				break;
			}
			case ECI:
				//TODO: not implemented
				index += bits->length;
				break;
			case FNC1:
				//TODO: not implemented
				index += bits->length;
				break;
			case None:
				reportError("Decoding mode is None.");
				index += bits->length;
				break;
		}
		if(flag) break;
	}

	//copy decoded data
	jab_data* decoded_data = (jab_data *)malloc(sizeof(jab_data) + count * sizeof(jab_byte));
	if(decoded_data == NULL){
        reportError("Memory allocation for decoded data failed");
        return NULL;
    }
    decoded_data->length = count;
    memcpy(decoded_data->data, decoded_bytes, count);

	free(decoded_bytes);
	return decoded_data;
}
