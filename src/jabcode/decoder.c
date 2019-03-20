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
	for(jab_int32 i=0; i<2; i++)
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
	symbol->palette = (jab_byte*)malloc(color_number * sizeof(jab_byte) * 3 * 2); //two palettes
	if(symbol->palette == NULL)
	{
		reportError("Memory allocation for master palette failed");
		return FATAL_ERROR;
	}

	jab_int32 palette_offset = 0;
	jab_boolean flag = 0;					//switch palette flag
	if((*module_count) == 0)
	{
		palette_offset = 0;					//start from module 0 for palette 1 around FP0
	}
	else
	{
		palette_offset = color_number * 3;	//start from module 6 for palette 2 around FP2
	}
	if(matrix->width > matrix->height)		//palette 1 consists of the left two parts
	{
		flag = 1;
	}
	else									//palette 1 consists of the top two parts
	{
		flag = 0;
	}
	jab_int32 color_index = 0;
	jab_int32 counter = 0;
	jab_int32 mtx_offset;
	jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
    jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;
	while(color_index < MIN(color_number, 64))
	{
		//palette 1 or palette 2
		mtx_offset = (*y) * mtx_bytes_per_row + (*x) * mtx_bytes_per_pixel;
		symbol->palette[palette_offset + color_index*3]	    = matrix->pixel[mtx_offset];
		symbol->palette[palette_offset + color_index*3 + 1] = matrix->pixel[mtx_offset + 1];
		symbol->palette[palette_offset + color_index*3 + 2] = matrix->pixel[mtx_offset + 2];
		//set data map
		data_map[(*y) * matrix->width + (*x)] = 1;
		//go to the next module
		(*module_count)++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, (*module_count), x, y);
		//next color and switch palette when necessary
		counter++;
		switch(counter % 4)
		{
			case 1:
				color_index++;
				if(flag) palette_offset = (palette_offset == 0) ? color_number * 3 : 0;
				break;
			case 2:
				color_index--;
				if(!flag) palette_offset = (palette_offset == 0) ? color_number * 3 : 0;
				break;
			case 3:
				color_index++;
				if(flag) palette_offset = (palette_offset == 0) ? color_number * 3 : 0;
				break;
			case 0:
				color_index++;
				if(!flag) palette_offset = (palette_offset == 0) ? color_number * 3 : 0;
				break;
		}
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
	symbol->palette = (jab_byte*)malloc(color_number * sizeof(jab_byte) * 3 * 2); //two palettes
    if(symbol->palette == NULL)
    {
		reportError("Memory allocation for slave palette failed");
		return FATAL_ERROR;
    }

    jab_int32 color_index = 0;
	jab_int32 mtx_offset;
    jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
    jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;
	while(color_index < MIN(color_number, 64))
	{
		jab_int32 px, py;
		if((color_index % 2) == 0)
		{
			//even color index in palette 1 is next to AP0
			px = slave_palette_position[color_index/2].x;
			py = slave_palette_position[color_index/2].y;
		}
		else
		{
			if(matrix->width > matrix->height)	//palette 1 consists of the left two parts
			{
				//odd color index in palette 1 is next to AP3
				px = slave_palette_position[color_index/2].y;
				py = matrix->height - 1 - slave_palette_position[color_index/2].x;
			}
			else								//palette 1 consists of the top two parts
			{
				//odd color index in palette 1 is next to AP1
				px = matrix->width - 1 - slave_palette_position[color_index/2].y;
				py = slave_palette_position[color_index/2].x;
			}
		}
		//set palette 1
		mtx_offset = py * mtx_bytes_per_row + px * mtx_bytes_per_pixel;
		symbol->palette[color_index*3]	   = matrix->pixel[mtx_offset];
		symbol->palette[color_index*3 + 1] = matrix->pixel[mtx_offset + 1];
		symbol->palette[color_index*3 + 2] = matrix->pixel[mtx_offset + 2];
		//set data map
		data_map[py * matrix->width + px] = 1;
		//set palette 2
		px = matrix->width - 1 - px;
		py = matrix->height - 1 - py;
		mtx_offset = py * mtx_bytes_per_row + px * mtx_bytes_per_pixel;
		symbol->palette[color_number*3 + color_index*3]	    = matrix->pixel[mtx_offset];
		symbol->palette[color_number*3 + color_index*3 + 1] = matrix->pixel[mtx_offset + 1];
		symbol->palette[color_number*3 + color_index*3 + 2] = matrix->pixel[mtx_offset + 2];
		//set data map
		data_map[py * matrix->width + px] = 1;
		//go to the next color
		color_index++;
	}

	//interpolate the palette if there are more than 64 colors
	if(color_number > 64)
	{
		interpolatePalette(symbol->palette, color_number);
	}
	return JAB_SUCCESS;
}

/**
 * @brief Decode a module using hard decision
 * @param palette the color palette
 * @param color_number the number of colors
 * @param r the red channel value
 * @param g the green channel value
 * @param b the blue channel value
 * @return the decoded value
*/
jab_byte decodeModuleHD(jab_byte* palette, jab_int32 color_number, jab_byte r, jab_byte g, jab_byte b)
{
	jab_byte index1 = 0, index2 = 0;
	if(palette)
	{
		//jab_int32 min1 = 766, min2 = 766;
		jab_int32 min1 = 255*255*3, min2 = 255*255*3;
		for(jab_int32 i=0; i<color_number; i++)
		{
			/*jab_int32 diff = abs(palette[i*3]	  - r) +
							 abs(palette[i*3 + 1] - g) +
							 abs(palette[i*3 + 2] - b);*/
			jab_int32 diff = (palette[i*3 + 0] - r) * (palette[i*3 + 0] - r) +
							 (palette[i*3 + 1] - g) * (palette[i*3 + 1] - g) +
							 (palette[i*3 + 2] - b) * (palette[i*3 + 2] - b);
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
			//printf("min1(%d) * 1.5 > min2(%d), %d %d %d", index1, index2, r, g, b);
			jab_int32 rg = abs(r - g);
			jab_int32 rb = abs(r - b);
			jab_int32 gb = abs(g - b);

			jab_int32 c1rg = abs(palette[index1*3] - palette[index1*3 + 1]);
			jab_int32 c1rb = abs(palette[index1*3] - palette[index1*3 + 2]);
			jab_int32 c1gb = abs(palette[index1*3 + 1] - palette[index1*3 + 2]);
			jab_int32 diff1 = abs(rg - c1rg) + abs(rb - c1rb) + abs(gb - c1gb);

			jab_int32 c2rg = abs(palette[index2*3] - palette[index2*3 + 1]);
			jab_int32 c2rb = abs(palette[index2*3] - palette[index2*3 + 2]);
			jab_int32 c2gb = abs(palette[index2*3 + 1] - palette[index2*3 + 2]);
			jab_int32 diff2 = abs(rg - c2rg) + abs(rb - c2rb) + abs(gb - c2gb);

			if(diff2 < diff1)
				index1 = index2;
			//printf("final: %d\n", index1);
		}
	}
	else	//if no palette is available, decode the module as black/white
	{
		index1 = ((r > 100 ? 1 : 0) + (g > 100 ? 1 : 0) + (b > 100 ? 1 : 0)) > 1 ? 1 : 0;
	}
	return index1;
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
/*			//fine-tune red and magenta
			jab_float r = rgb[0];
			jab_float g = rgb[1];
			jab_float b = rgb[2];
			if(cv[0] == 1 && cv[1] == 0)
			{
				jab_int32 cpb0 = MAX(MAX(MAX(palette[2], palette[8]), palette[14]), palette[20]);
				jab_int32 cpb1 = MIN(MIN(MIN(palette[5], palette[11]), palette[17]), palette[23]);
				jab_float b_g = ((jab_float)palette[14] / (jab_float)palette[13] + (jab_float)palette[17] / (jab_float)palette[16]) / 2.0f;
				if(cv[2] == 0 && b > cpb0)
				{
					if(b/g > b_g)
						cv[2] = 1;
				}
				else if(cv[2] == 1 && b < cpb1)
				{
					if(b/g < b_g)
						cv[2] = 0;
				}
			}
			//fine-tune blue and cyan
			else if(cv[0] == 0 && cv[2] == 1)
			{
				jab_int32 cpg0 = MAX(MAX(MAX(palette[1], palette[4]), palette[13]), palette[16]);
				jab_int32 cpg1 = MIN(MIN(MIN(palette[7], palette[10]), palette[19]), palette[22]);
				jab_float g_b = ((jab_float)palette[4] / (jab_float)palette[5] + (jab_float)palette[10] / (jab_float)palette[11]) / 2.0f;
				if(cv[1] == 0 && g > cpg0)
				{
					if(g/b > g_b)
						cv[1] = 1;
				}
				else if(cv[1] == 1 && g < cpg1)
				{
					if(g/b < g_b)
						cv[1] = 0;
				}
			}
*/
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
		host_symbol->slave_metadata[docked_position].VF = host_symbol->metadata.VF;
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
		//calculate VF
		jab_int32 sv_max = MAX(host_symbol->slave_metadata[docked_position].side_version.x, host_symbol->slave_metadata[docked_position].side_version.y);
		if(sv_max <= 4)
			host_symbol->slave_metadata[docked_position].VF = 0;
		else if(sv_max <= 8)
			host_symbol->slave_metadata[docked_position].VF = 1;
		else if(sv_max <= 16)
			host_symbol->slave_metadata[docked_position].VF = 2;
		else if(sv_max <= 32)
			host_symbol->slave_metadata[docked_position].VF = 3;
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
 * @brief Decode master symbol metadata
 * @param matrix the symbol matrix
 * @param symbol the master symbol
 * @param data_map the data module positions
 * @return JAB_SUCCESS | JAB_FAILURE | DECODE_METADATA_FAILED | FATAL_ERROR
*/
jab_int32 decodeMasterMetadata(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map)
{
	if(matrix == NULL)
	{
		reportError("Invalid master symbol matrix");
		return FATAL_ERROR;
	}

	jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
    jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;
    jab_int32 mtx_offset;

	jab_int32 x = MASTER_METADATA_X;
	jab_int32 y = MASTER_METADATA_Y;
	jab_int32 module_count = 0;

	jab_byte part1[MASTER_METADATA_PART1_LENGTH] = {0};			//6 encoded bits
	jab_byte part2[MASTER_METADATA_PART2_LENGTH] = {0};			//12 encoded bits
	jab_float part2_p[MASTER_METADATA_PART2_LENGTH] = {0.0f};
	jab_byte part3[MASTER_METADATA_PART3_MAX_LENGTH] = {0};		//16-32 encoded bits
	jab_float part3_p[MASTER_METADATA_PART3_MAX_LENGTH] = {0.0f};
	jab_int32 part1_bit_length = MASTER_METADATA_PART1_LENGTH;	//part1 encoded length
	jab_int32 part2_bit_length = MASTER_METADATA_PART2_LENGTH;	//part2 encoded length
	jab_int32 part3_bit_length = 0;								//part3 encoded length
	jab_int32 part1_bit_count = 0;
	jab_int32 part2_bit_count = 0;
	jab_int32 part3_bit_count = 0;
	jab_uint32 SS, VF, V, E;
	jab_uint32 V_length = 0, E_length = 6;

	//read part 1 - decode Nc out of modules in 2-color mode
	while(part1_bit_count < part1_bit_length)
	{
		//decode bit out of the module at [x][y]
		mtx_offset = y * mtx_bytes_per_row + x * mtx_bytes_per_pixel;
		part1[part1_bit_count++] = decodeModuleHD(0, 0,
											      matrix->pixel[mtx_offset],
											      matrix->pixel[mtx_offset + 1],
											      matrix->pixel[mtx_offset + 2]);
		//set data map
		data_map[y * matrix->width + x] = 1;
		//go to the next module
		module_count++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, module_count, &x, &y);
	}
	//decode ldpc for part1
	if( !decodeLDPChd(part1, part1_bit_length, part1_bit_length > 36 ? 4 : 3, 0) )
	{
#if TEST_MODE
		reportError("LDPC decoding for master metadata part 1 failed");
#endif
		return DECODE_METADATA_FAILED;
	}
	//parse part1
	symbol->metadata.Nc = (part1[0] << 2) + (part1[1] << 1) + part1[2];
	jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	jab_int32 bits_per_module = (jab_int32)(log(color_number) / log(2));

	//read color palettes
	if(readColorPaletteInMaster(matrix, symbol, data_map, &module_count, &x, &y) < 0)
	{
		reportError("Reading color palettes in master symbol failed");
		return FATAL_ERROR;
	}

	//calculate palette thresholds and reference points
    jab_float* palette_ths1 = 0;
    jab_float* palette_rp1 = 0;
    if(!getPaletteThreshold(symbol->palette, color_number, &palette_ths1, &palette_rp1))
	{
		reportError("Getting palette threshold and reference points failed");
		free(palette_ths1);
		free(palette_rp1);
		return FATAL_ERROR;
	}
	jab_float* palette_ths2 = 0;
    jab_float* palette_rp2 = 0;
    if(!getPaletteThreshold(symbol->palette + color_number * 3, color_number, &palette_ths2, &palette_rp2))
	{
		reportError("Getting palette threshold and reference points failed");
		free(palette_ths1);
		free(palette_rp1);
		free(palette_ths2);
		free(palette_rp2);
		return FATAL_ERROR;
	}

    //read part2
    jab_float bits_p[bits_per_module];
    while(part2_bit_count < part2_bit_length)
    {
		//decode bits out of the module at [x][y]
		jab_byte* palette;
		jab_float* palette_ths;
		jab_float* palette_rp;
		if(matrix->width > matrix->height)		//if the width is bigger than the height,
		{										//the first palette is used for the modules in the left half
			if(x < matrix->width/2)
			{
				palette = symbol->palette;
				palette_ths = palette_ths1;
				palette_rp = palette_rp1;
			}
			else								//the second palette is used for the modules in the right half
			{
				palette = symbol->palette + color_number * 3;
				palette_ths = palette_ths2;
				palette_rp = palette_rp2;
			}
		}
		else									//if the height is bigger than the width,
		{										//the first palette is used for the modules in the upper half
			if(y < matrix->height/2)
			{
				palette = symbol->palette;
				palette_ths = palette_ths1;
				palette_rp = palette_rp1;
			}
			else								//the second palette is used for the modules in the lower half
			{
				palette = symbol->palette + color_number * 3;
				palette_ths = palette_ths2;
				palette_rp = palette_rp2;
			}
		}
		mtx_offset = y * mtx_bytes_per_row + x * mtx_bytes_per_pixel;
		jab_byte bits = decodeModule(palette,
									 color_number,
									 palette_ths,
									 palette_rp,
									 &matrix->pixel[mtx_offset],
									 bits_p);
		//write the bits into part2
		for(jab_int32 i=0; i<bits_per_module; i++)
		{
			jab_byte bit = (bits >> (bits_per_module - 1 - i)) & 0x01;
			if(part2_bit_count < part2_bit_length)
			{
				part2[part2_bit_count] = bit;
				part2_p[part2_bit_count] = bits_p[i];
				part2_bit_count++;
			}
			else	//if part2 is full, write the rest bits into part3
			{
				part3[part3_bit_count] = bit;
				part3_p[part3_bit_count] = bits_p[i];
				part3_bit_count++;

			}
		}
		//set data map
		data_map[y * matrix->width + x] = 1;
		//go to the next module
		module_count++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, module_count, &x, &y);
    }
	//decode ldpc for part2
	if( !decodeLDPC(part2_p, part2_bit_length, part2_bit_length > 36 ? 4 : 3, 0, part2) )
	{
#if TEST_MODE
		reportError("LDPC decoding for master metadata part 2 failed");
#endif
		free(palette_ths1);
		free(palette_rp1);
		free(palette_ths2);
		free(palette_rp2);
		return DECODE_METADATA_FAILED;
	}
    //parse part2
	SS = part2[0];
	VF = (part2[1] << 1) + part2[2];
	symbol->metadata.VF = VF;
	symbol->metadata.mask_type = (part2[3] << 2) + (part2[4] << 1) + part2[5];

	if(SS == 0)	//square symbol
	{
		V_length = (VF == 0) ? 2 : (VF + 1);
	}
	else		//rectangle symbol
	{
		V_length = VF * 2 + 4;
	}
	part3_bit_length += V_length * 2;
	part3_bit_length += E_length * 2;

    //read part3
	while(part3_bit_count < part3_bit_length)
	{
		//decode bits out of the module at [x][y]
		jab_byte* palette;
		jab_float* palette_ths;
		jab_float* palette_rp;
		if(matrix->width > matrix->height)		//if the width is bigger than the height,
		{										//the first palette is used for the modules in the left half
			if(x < matrix->width/2)
			{
				palette = symbol->palette;
				palette_ths = palette_ths1;
				palette_rp = palette_rp1;
			}
			else								//the second palette is used for the modules in the right half
			{
				palette = symbol->palette + color_number * 3;
				palette_ths = palette_ths2;
				palette_rp = palette_rp2;
			}
		}
		else									//if the height is bigger than the width,
		{										//the first palette is used for the modules in the upper half
			if(y < matrix->height/2)
			{
				palette = symbol->palette;
				palette_ths = palette_ths1;
				palette_rp = palette_rp1;
			}
			else								//the second palette is used for the modules in the lower half
			{
				palette = symbol->palette + color_number * 3;
				palette_ths = palette_ths2;
				palette_rp = palette_rp2;
			}
		}
		mtx_offset = y * mtx_bytes_per_row + x * mtx_bytes_per_pixel;
		jab_byte bits = decodeModule(palette,
									 color_number,
									 palette_ths,
									 palette_rp,
									 &matrix->pixel[mtx_offset],
									 bits_p);
		//write the bits into part1
		for(jab_int32 i=0; i<bits_per_module; i++)
		{
			jab_byte bit = (bits >> (bits_per_module - 1 - i)) & 0x01;
			if(part3_bit_count < part3_bit_length)
			{
				part3[part3_bit_count] = bit;
				part3_p[part3_bit_count] = bits_p[i];
				part3_bit_count++;
			}
			else	//if part3 is full, stop
			{
				break;
			}
		}
		//set data map
		data_map[y * matrix->width + x] = 1;
		//go to the next module
		module_count++;
		getNextMetadataModuleInMaster(matrix->height, matrix->width, module_count, &x, &y);
	}
	//decode ldpc for part3
	if( !decodeLDPC(part3_p, part3_bit_length, part3_bit_length > 36 ? 4 : 3, 0, part3) )
	{
#if TEST_MODE
		reportError("LDPC decoding for master metadata part 3 failed");
#endif
		free(palette_ths1);
		free(palette_rp1);
		free(palette_ths2);
		free(palette_rp2);
		return DECODE_METADATA_FAILED;
	}
    //parse part3
	jab_int32 bit_index = 0;
	if(V_length > 0)	//parse V
	{
		if(SS == 0)	//square symbol
		{
			V = 0;
			for(jab_int32 i=0; i<V_length; i++)
			{
				V += part3[i] << (V_length - 1 - i);
			}
			jab_int32 side_version = (VF == 0) ? V + 1 : pow(2, VF + 1) + V + 1;
			symbol->metadata.side_version.x = side_version;
			symbol->metadata.side_version.y = side_version;
		}
		else		//rectangle symbol
		{
			//get horizontal side version
			V = 0;
			for(jab_int32 i=0; i<V_length/2; i++)
			{
				V += part3[i] << (V_length/2 - 1 - i);
			}
			symbol->metadata.side_version.x = V + 1;
			//get vertical side version
			V = 0;
			for(jab_int32 i=0; i<V_length/2; i++)
			{
				V += part3[i+V_length/2] << (V_length/2 - 1 - i);
			}
			symbol->metadata.side_version.y = V + 1;
		}
		bit_index += V_length;
	}
	if(E_length > 0)	//parse E
	{
		//get wc (the first half of E)
		E = 0;
		for(jab_int32 i=bit_index; i<(bit_index+E_length/2); i++)
		{
			E += part3[i] << (E_length/2 - 1 - (i - bit_index));
		}
		symbol->metadata.ecl.x = E + 3;		//wc = E_part1 + 3
		//get wr (the second half of E)
		E = 0;
		for(jab_int32 i=bit_index; i<(bit_index+E_length/2); i++)
		{
			E += part3[i+E_length/2] << (E_length/2 - 1 - (i - bit_index));
		}
		symbol->metadata.ecl.y = E + 4;		//wr = E_part2 + 4
		bit_index += E_length;
	}
	symbol->metadata.docked_position = 0;

	//check side version
	symbol->side_size.x = VERSION2SIZE(symbol->metadata.side_version.x);
	symbol->side_size.y = VERSION2SIZE(symbol->metadata.side_version.y);
	if(matrix->width != symbol->side_size.x || matrix->height != symbol->side_size.y)
	{
		reportError("Master symbol matrix size does not match the metadata");
		free(palette_ths1);
		free(palette_rp1);
		free(palette_ths2);
		free(palette_rp2);
		return JAB_FAILURE;
	}
	//check wc and wr
	jab_int32 wc = symbol->metadata.ecl.x;
	jab_int32 wr = symbol->metadata.ecl.y;
	if(wc >= wr)
	{
#if TEST_MODE
		reportError("Incorrect error correction parameter in master metadata");
#endif
		free(palette_ths1);
		free(palette_rp1);
		free(palette_ths2);
		free(palette_rp2);
		return DECODE_METADATA_FAILED;
	}

	free(palette_ths1);
	free(palette_rp1);
	free(palette_ths2);
	free(palette_rp2);
	return JAB_SUCCESS;
}

/**
 * @brief Decode data modules
 * @param matrix the symbol matrix
 * @param symbol the symbol to be decoded
 * @param data_map the data module positions
 * @param bits_p the probability of the reliability of the decoded bits
 * @return the decoded data | NULL if failed
*/
jab_data* readRawModuleData(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_float** bits_p)
{
	jab_int32 mtx_bytes_per_pixel = matrix->bits_per_pixel / 8;
    jab_int32 mtx_bytes_per_row = matrix->width * mtx_bytes_per_pixel;
    jab_int32 mtx_offset;

    jab_int32 color_number = (jab_int32)pow(2, symbol->metadata.Nc + 1);
	jab_int32 module_count = 0;
    jab_data* data = (jab_data*)malloc(sizeof(jab_data) + matrix->width * matrix->height * sizeof(jab_char));
    if(data == NULL)
	{
		reportError("Memory allocation for raw module data failed");
		return NULL;
	}

	jab_int32 bits_per_module = symbol->metadata.Nc + 1;
	(*bits_p) = (jab_float*)malloc(matrix->width * matrix->height * bits_per_module * sizeof(jab_float));
	if((*bits_p) == NULL)
	{
		reportError("Memory allocation for bit probability failed");
		return NULL;
	}

	//calculate palette thresholds and reference points
    jab_float* palette_ths1 = 0;
    jab_float* palette_rp1 = 0;
    if(!getPaletteThreshold(symbol->palette, color_number, &palette_ths1, &palette_rp1))
	{
		reportError("Getting palette threshold and reference points failed");
		free(palette_ths1);
		free(palette_rp1);
		return JAB_FAILURE;
	}
	jab_float* palette_ths2 = 0;
    jab_float* palette_rp2 = 0;
    if(!getPaletteThreshold(symbol->palette + color_number * 3, color_number, &palette_ths2, &palette_rp2))
	{
		reportError("Getting palette threshold and reference points failed");
		free(palette_ths1);
		free(palette_rp1);
		free(palette_ths2);
		free(palette_rp2);
		return JAB_FAILURE;
	}
#if TEST_MODE
	printf("p1:\n");
	for(int i=0;i<color_number;i++)
	{
		printf("%d\t%d\t%d\n", symbol->palette[i*3], symbol->palette[i*3+1], symbol->palette[i*3+2]);
	}
	printf("ths1r: %f, %f, %f\n", palette_ths1[0], palette_ths1[1], palette_ths1[2]);
	printf("ths1g: %f, %f, %f\n", palette_ths1[3], palette_ths1[4], palette_ths1[5]);
	printf("ths1b: %f, %f, %f\n", palette_ths1[6], palette_ths1[7], palette_ths1[8]);
	printf("p2:\n");
	for(int i=0;i<color_number;i++)
	{
		printf("%d\t%d\t%d\n", symbol->palette[3*color_number+i*3], symbol->palette[3*color_number+i*3+1], symbol->palette[3*color_number+i*3+2]);
	}
	printf("ths2r: %f, %f, %f\n", palette_ths2[0], palette_ths2[1], palette_ths2[2]);
	printf("ths2g: %f, %f, %f\n", palette_ths2[3], palette_ths2[4], palette_ths2[5]);
	printf("ths2b: %f, %f, %f\n", palette_ths2[6], palette_ths2[7], palette_ths2[8]);

	FILE* fp = fopen("dec_module_rgb.bin", "wb");
#endif // TEST_MODE
	for(jab_int32 j=0; j<matrix->width; j++)
	{
		for(jab_int32 i=0; i<matrix->height; i++)
		{
			if(data_map[i*matrix->width + j] == 0)
			{
				//decode bits out of the module at [x][y]
				jab_byte* palette;
				jab_float* palette_ths;
				jab_float* palette_rp;
				if(matrix->width > matrix->height)		//if the width is bigger than the height,
				{										//the first palette is used for the modules in the left half
					if(j < matrix->width/2)
					{
						palette = symbol->palette;
						palette_ths = palette_ths1;
						palette_rp = palette_rp1;
					}
					else								//the second palette is used for the modules in the right half
					{
						palette = symbol->palette + color_number * 3;
						palette_ths = palette_ths2;
						palette_rp = palette_rp2;
					}
				}
				else									//if the height is bigger than the width,
				{										//the first palette is used for the modules in the upper half
					if(i < matrix->height/2)
					{
						palette = symbol->palette;
						palette_ths = palette_ths1;
						palette_rp = palette_rp1;
					}
					else								//the second palette is used for the modules in the lower half
					{
						palette = symbol->palette + color_number * 3;
						palette_ths = palette_ths2;
						palette_rp = palette_rp2;
					}
				}
				mtx_offset = i * mtx_bytes_per_row + j * mtx_bytes_per_pixel;
				/*jab_byte bits = decodeModuleHD(palette, color_number,
											   matrix->pixel[mtx_offset],
											   matrix->pixel[mtx_offset + 1],
											   matrix->pixel[mtx_offset + 2]);*/
				jab_byte bits = decodeModule(palette,
											 color_number,
											 palette_ths,
											 palette_rp,
											 &matrix->pixel[mtx_offset],
											 (*bits_p) + module_count * bits_per_module);
				//write the bits into data
				data->data[module_count] = (jab_char)bits;
#if TEST_MODE
				fwrite(&matrix->pixel[mtx_offset], 3, 1, fp);
#endif // TEST_MODE
				module_count++;
			}
		}
	}
	data->length = module_count;
#if TEST_MODE
	fclose(fp);
#endif // TEST_MODE
	free(palette_ths1);
	free(palette_rp1);
	free(palette_ths2);
	free(palette_rp2);
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
 * @param data_map the data module positions
 * @return JAB_SUCCESS | FATAL_ERROR
*/
jab_int32 loadDefaultMasterMetadata(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map)
{
	//set default metadata values
	symbol->metadata.Nc = DEFAULT_MODULE_COLOR_MODE;
	symbol->metadata.ecl.x = ecclevel2wcwr[DEFAULT_ECC_LEVEL][0];
	symbol->metadata.ecl.y = ecclevel2wcwr[DEFAULT_ECC_LEVEL][1];
	symbol->metadata.mask_type = DEFAULT_MASKING_REFERENCE;
	symbol->metadata.docked_position = 0;							//no default value
	symbol->metadata.side_version.x = SIZE2VERSION(matrix->width);	//no default value
	symbol->metadata.side_version.y = SIZE2VERSION(matrix->height);	//no default value
	jab_int32 max_side_version = MAX(symbol->metadata.side_version.x, symbol->metadata.side_version.y);
	if(max_side_version <= 4)
		symbol->metadata.VF = 0;
	else if(max_side_version <= 8)
		symbol->metadata.VF = 1;
	else if(max_side_version <= 16)
		symbol->metadata.VF = 2;
	else
		symbol->metadata.VF = 3;

	//read color palettes
	jab_int32 x = MASTER_METADATA_X;
	jab_int32 y = MASTER_METADATA_Y;
	jab_int32 module_count = 0;
    if(readColorPaletteInMaster(matrix, symbol, data_map, &module_count, &x, &y) < 0)
	{
		reportError("Reading color palettes in master symbol failed");
		return FATAL_ERROR;
	}
    return JAB_SUCCESS;
}

/**
 * @brief Decode symbol
 * @param matrix the symbol matrix
 * @param symbol the symbol to be decoded
 * @param data_map the data module positions
 * @param type the symbol type, 0: master, 1: slave
 * @return JAB_SUCCESS | JAB_FAILURE | DECODE_METADATA_FAILED | FATAL_ERROR
*/
jab_int32 decodeSymbol(jab_bitmap* matrix, jab_decoded_symbol* symbol, jab_byte* data_map, jab_int32 type)
{
	//fill data map
	fillDataMap(data_map, matrix->width, matrix->height, type);

	//read raw data
	jab_float* bits_p = 0;
	jab_data* raw_module_data = readRawModuleData(matrix, symbol, data_map, &bits_p);
	if(raw_module_data == NULL)
	{
		JAB_REPORT_ERROR(("Reading raw module data in symbol %d failed", symbol->index))
		free(data_map);
		return FATAL_ERROR;
	}

#if TEST_MODE
	FILE* fp = fopen("dec_module_data.bin", "wb");
	fwrite(raw_module_data->data, raw_module_data->length, 1, fp);
	fclose(fp);
#endif // TEST_MODE

	//demask
	demaskSymbol(raw_module_data, data_map, symbol->side_size, symbol->metadata.mask_type, (jab_int32)pow(2, symbol->metadata.Nc + 1));
	free(data_map);

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
    deinterleaveData(raw_data, bits_p);

#if TEST_MODE
	JAB_REPORT_INFO(("wc:%d, wr:%d, Pg:%d, Pn: %d", wc, wr, Pg, Pn))
	fp = fopen("dec_bit_data.bin", "wb");
	fwrite(raw_data->data, raw_data->length, 1, fp);
	fclose(fp);
#endif // TEST_MODE

	//decode ldpc
    //if(decodeLDPC(bits_p, Pg, symbol->metadata.ecl.x, symbol->metadata.ecl.y, (jab_byte*)raw_data->data) != Pn)
    if(decodeLDPChd((jab_byte*)raw_data->data, Pg, symbol->metadata.ecl.x, symbol->metadata.ecl.y) != Pn)
    {
		JAB_REPORT_ERROR(("LDPC decoding for data in symbol %d failed", symbol->index))
		free(raw_data);
		free(bits_p);
		return JAB_FAILURE;
	}
	free(bits_p);

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

	//decode metadata and build palette
	jab_int32 ret = decodeMasterMetadata(matrix, symbol, data_map);
	if(DECODE_METADATA_FAILED == ret)
	{
		//clear data_map
		memset(data_map, 0, matrix->width*matrix->height*sizeof(jab_byte));
		//load default metadata and color palette
		if(loadDefaultMasterMetadata(matrix, symbol, data_map) < 0)
		{
			reportError("Loading default master metadata failed");
			free(data_map);
			return FATAL_ERROR;
		}
	}

	//decode master symbol
	return decodeSymbol(matrix, symbol, data_map, 0);
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

	//decode slave symbol
	return decodeSymbol(matrix, symbol, data_map, 1);
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
