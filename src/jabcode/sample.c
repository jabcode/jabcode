/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file sample.c
 * @brief Symbol sampling
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jabcode.h"
#include "detector.h"
#include "decoder.h"

#define SAMPLE_AREA_WIDTH	(CROSS_AREA_WIDTH / 2 - 2) //width of the columns where the metadata and palette in slave symbol are located
#define SAMPLE_AREA_HEIGHT	20	//height of the metadata rows including the first row, though it does not contain metadata

/**
 * @brief Sample a symbol
 * @param bitmap the image bitmap
 * @param pt the transformation matrix
 * @param side_size the symbol size in module
 * @return the sampled symbol matrix
*/
jab_bitmap* sampleSymbol(jab_bitmap* bitmap, jab_perspective_transform* pt, jab_vector2d side_size)
{
	jab_int32 mtx_bytes_per_pixel = bitmap->bits_per_pixel / 8;
	jab_int32 mtx_bytes_per_row = side_size.x * mtx_bytes_per_pixel;
	jab_bitmap* matrix = (jab_bitmap*)malloc(sizeof(jab_bitmap) + side_size.x*side_size.y*mtx_bytes_per_pixel*sizeof(jab_byte));
	if(matrix == NULL)
	{
		reportError("Memory allocation for symbol bitmap matrix failed");
		return NULL;
	}
	matrix->channel_count = bitmap->channel_count;
	matrix->bits_per_channel = bitmap->bits_per_channel;
	matrix->bits_per_pixel = matrix->bits_per_channel * matrix->channel_count;
	matrix->width = side_size.x;
	matrix->height= side_size.y;

	jab_int32 bmp_bytes_per_pixel = bitmap->bits_per_pixel / 8;
	jab_int32 bmp_bytes_per_row = bitmap->width * bmp_bytes_per_pixel;

	jab_point points[side_size.x];
    for(jab_int32 i=0; i<side_size.y; i++)
    {
		for(jab_int32 j=0; j<side_size.x; j++)
		{
			points[j].x = (jab_float)j + 0.5f;
			points[j].y = (jab_float)i + 0.5f;
		}
		warpPoints(pt, points, side_size.x);
		for(jab_int32 j=0; j<side_size.x; j++)
		{
			jab_int32 mapped_x = (jab_int32)points[j].x;
			jab_int32 mapped_y = (jab_int32)points[j].y;
			if(mapped_x < 0 || mapped_x > bitmap->width-1)
			{
				if(mapped_x == -1) mapped_x = 0;
				else if(mapped_x ==  bitmap->width) mapped_x = bitmap->width - 1;
				else return NULL;
			}
			if(mapped_y < 0 || mapped_y > bitmap->height-1)
			{
				if(mapped_y == -1) mapped_y = 0;
				else if(mapped_y ==  bitmap->height) mapped_y = bitmap->height - 1;
				else return NULL;
			}
			for(jab_int32 c=0; c<matrix->channel_count; c++)
			{
				//get the average of pixel values in 3x3 neighborhood as the sampled value
				jab_float sum = 0;
				for(jab_int32 dx=-1; dx<=1; dx++)
				{
					for(jab_int32 dy=-1; dy<=1; dy++)
					{
						jab_int32 px = mapped_x + dx;
						jab_int32 py = mapped_y + dy;
						if(px < 0 || px > bitmap->width - 1)  px = mapped_x;
						if(py < 0 || py > bitmap->height - 1) py = mapped_y;
						sum += bitmap->pixel[py*bmp_bytes_per_row + px*bmp_bytes_per_pixel + c];
					}
				}
				jab_byte ave = (jab_byte)(sum / 9.0f + 0.5f);
				matrix->pixel[i*mtx_bytes_per_row + j*mtx_bytes_per_pixel + c] = ave;
				//matrix->pixel[i*mtx_bytes_per_row + j*mtx_bytes_per_pixel + c] = bitmap->pixel[mapped_y*bmp_bytes_per_row + mapped_x*bmp_bytes_per_pixel + c];
#if TEST_MODE
				test_mode_bitmap->pixel[mapped_y*bmp_bytes_per_row + mapped_x*bmp_bytes_per_pixel + c] = test_mode_color;
				if(c == 3 && test_mode_color == 0)
                    test_mode_bitmap->pixel[mapped_y*bmp_bytes_per_row + mapped_x*bmp_bytes_per_pixel + c] = 255;
#endif
			}
		}
    }
	return matrix;
}

/**
 * @brief Sample a cross area between the host and slave symbols
 * @param bitmap the image bitmap
 * @param pt the transformation matrix
 * @return the sampled area matrix
*/
jab_bitmap* sampleCrossArea(jab_bitmap* bitmap, jab_perspective_transform* pt)
{
	jab_int32 mtx_bytes_per_pixel = bitmap->bits_per_pixel / 8;
	jab_int32 mtx_bytes_per_row = SAMPLE_AREA_WIDTH * mtx_bytes_per_pixel;
	jab_bitmap* matrix = (jab_bitmap*)malloc(sizeof(jab_bitmap) + SAMPLE_AREA_WIDTH*SAMPLE_AREA_HEIGHT*mtx_bytes_per_pixel*sizeof(jab_byte));
	if(matrix == NULL)
	{
		reportError("Memory allocation for cross area bitmap matrix failed");
		return NULL;
	}
	matrix->channel_count = bitmap->channel_count;
	matrix->bits_per_channel = bitmap->bits_per_channel;
	matrix->bits_per_pixel = matrix->bits_per_channel * matrix->channel_count;
	matrix->width = SAMPLE_AREA_WIDTH;
	matrix->height= SAMPLE_AREA_HEIGHT;

	jab_int32 bmp_bytes_per_pixel = bitmap->bits_per_pixel / 8;
	jab_int32 bmp_bytes_per_row = bitmap->width * bmp_bytes_per_pixel;

	//only sample the area where the metadata and palette are located
	jab_point points[SAMPLE_AREA_WIDTH];
    for(jab_int32 i=0; i<SAMPLE_AREA_HEIGHT; i++)
    {
		for(jab_int32 j=0; j<SAMPLE_AREA_WIDTH; j++)
		{
			points[j].x = (jab_float)j + CROSS_AREA_WIDTH / 2 + 0.5f;
			points[j].y = (jab_float)i + 0.5f;
		}
		warpPoints(pt, points, SAMPLE_AREA_WIDTH);
		for(jab_int32 j=0; j<SAMPLE_AREA_WIDTH; j++)
		{
			jab_int32 mapped_x = (jab_int32)points[j].x;
			jab_int32 mapped_y = (jab_int32)points[j].y;
			if(mapped_x < 0 || mapped_x > bitmap->width-1)
			{
				if(mapped_x == -1) mapped_x = 0;
				else if(mapped_x ==  bitmap->width) mapped_x = bitmap->width - 1;
				else return NULL;
			}
			if(mapped_y < 0 || mapped_y > bitmap->height-1)
			{
				if(mapped_y == -1) mapped_y = 0;
				else if(mapped_y ==  bitmap->height) mapped_y = bitmap->height - 1;
				else return NULL;
			}
			for(jab_int32 c=0; c<matrix->channel_count; c++)
			{
				//get the average of pixel values in 3x3 neighborhood as the sampled value
				jab_float sum = 0;
				for(jab_int32 dx=-1; dx<=1; dx++)
				{
					for(jab_int32 dy=-1; dy<=1; dy++)
					{
						jab_int32 px = mapped_x + dx;
						jab_int32 py = mapped_y + dy;
						if(px < 0 || px > bitmap->width - 1)  px = mapped_x;
						if(py < 0 || py > bitmap->height - 1) py = mapped_y;
						sum += bitmap->pixel[py*bmp_bytes_per_row + px*bmp_bytes_per_pixel + c];
					}
				}
				jab_byte ave = (jab_byte)(sum / 9.0f + 0.5f);
				matrix->pixel[i*mtx_bytes_per_row + j*mtx_bytes_per_pixel + c] = ave;
				//matrix->pixel[i*mtx_bytes_per_row + j*mtx_bytes_per_pixel + c] = bitmap->pixel[mapped_y*bmp_bytes_per_row + mapped_x*bmp_bytes_per_pixel + c];
			}
		}
    }
	return matrix;
}

