/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file binarizer.c
 * @brief Binarize the image
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jabcode.h"

#define BLOCK_SIZE_POWER	3
#define BLOCK_SIZE 			(1 << BLOCK_SIZE_POWER)
#define BLOCK_SIZE_MASK 	(BLOCK_SIZE - 1)
#define MINIMUM_DIMENSION 	(BLOCK_SIZE * 5)
#define CAP(val, min, max)	(val < min ? min : (val > max ? max : val))

/**
 * @brief Check bimodal/trimodal distribution
 * @param hist the histogram
 * @param channel the color channel
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean isBiTrimodal(jab_float hist[], jab_int32 channel)
{
	jab_int32 modal_number;
	if(channel == 1)
		modal_number = 3;
	else
		modal_number = 2;

    jab_int32 count = 0;
    for(jab_int32 i=1; i<255; i++)
    {
        if(hist[i-1]<hist[i] && hist[i+1]<hist[i])
        {
            count++;
            if(count > modal_number) return JAB_FAILURE;
        }
    }
    if (count == modal_number)
        return JAB_SUCCESS;
    else
        return JAB_FAILURE;
}

/**
 * @brief Get the minimal value in a histogram with bimodal distribution
 * @param hist the histogram
 * @param channel the color channel
 * @return minimum threshold | -1 if failed
*/
jab_int32 getMinimumThreshold(jab_int32 hist[], jab_int32 channel)
{
    jab_float hist_c[256], hist_s[256];
    for (jab_int32 i=0; i<256; i++)
    {
        hist_c[i] = (jab_float)hist[i];
        hist_s[i] = (jab_float)hist[i];
    }

    //smooth histogram
    jab_int32 iter = 0;
    while(!isBiTrimodal(hist_s, channel))
    {
        hist_s[0] = (hist_c[0] + hist_c[0] + hist_c[1]) / 3;
        for (jab_int32 i=1; i<255; i++)
            hist_s[i] = (hist_c[i - 1] + hist_c[i] + hist_c[i + 1]) / 3;
        hist_s[255] = (hist_c[254] + hist_c[255] + hist_c[255]) / 3;
        memcpy(hist_c, hist_s, 256*sizeof(jab_float));
        iter++;
        if (iter >= 1000) return -1;
    }

    //get the minimum between two peaks as threshold
    jab_int32 peak_number;
    if(channel == 1)
		peak_number = 2;
	else
		peak_number = 1;

    jab_boolean peak_found = 0;
	for (jab_int32 i=1; i<255; i++)
	{
		if (hist_s[i-1]<hist_s[i] && hist_s[i+1]<hist_s[i]) peak_found++;
		if (peak_found==peak_number && hist_s[i-1]>=hist_s[i] && hist_s[i+1]>=hist_s[i])
			return i-1;
	}
    return -1;
}

/**
 * @brief Binarize a color channel of a bitmap using histogram binarization algorithm
 * @param bitmap the input bitmap
 * @param channel the color channel
 * @return binarized bitmap | NULL if failed
*/
jab_bitmap* binarizerHist(jab_bitmap* bitmap, jab_int32 channel)
{
	jab_bitmap* binary = (jab_bitmap*)malloc(sizeof(jab_bitmap) + bitmap->width*bitmap->height*sizeof(jab_byte));
	if(binary == NULL)
	{
		reportError("Memory allocation for binary bitmap failed");
		return NULL;
	}
	binary->width = bitmap->width;
	binary->height= bitmap->height;
	binary->bits_per_channel = 8;
	binary->bits_per_pixel = 8;
	binary->channel_count = 1;
	jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;

	//get histogram
	jab_int32 hist[256];
	memset(hist, 0, 256*sizeof(jab_int32));
	for(jab_int32 i=0; i<bitmap->width*bitmap->height; i++)
	{
		if(channel>0)
		{
			jab_byte r, g, b;
			r = bitmap->pixel[i*bytes_per_pixel];
			g = bitmap->pixel[i*bytes_per_pixel + 1];
			b = bitmap->pixel[i*bytes_per_pixel + 2];

			float mean = (float)(r + g + b)/(float)3.0;
			float pr = (float)r/mean;
			float pg = (float)g/mean;
			float pb = (float)b/mean;

			//green channel
			if(channel == 1)
			{
				//skip white, black, yellow
				if( (r>200 && g>200 && b>200) || (r<50 && g<50 && b<50) || (r>200 && g>200))
					continue;
				//skip white and black: r, g, b values are very close
				if( (pr<1.25 && pr>0.8) && (pg<1.25 && pg>0.8) && (pb<1.25 && pb>0.8) )
					continue;
				//skip yellow: blue is small, red and green are very close
				if( pb<0.5 && (pr/pg<1.25 && pr/pg>0.8) )
					continue;
			}
			//blue channel
			else if(channel == 2)
			{
				//skip white,black
				if( (r>200 && g>200 && b>200) || (r<50 && g<50 && b<50) )
					continue;
				//skip white and black: r, g, b values are very close
				if( (pr<1.25 && pr>0.8) && (pg<1.25 && pg>0.8) && (pb<1.25 && pb>0.8) )
					continue;
			}
		}
		hist[bitmap->pixel[i*bytes_per_pixel + channel]]++;
	}

	//get threshold
	jab_int32 ths = getMinimumThreshold(hist, channel);

	//binarize bitmap
	for(jab_int32 i=0; i<bitmap->width*bitmap->height; i++)
	{
		binary->pixel[i] = bitmap->pixel[i*bytes_per_pixel + channel] > ths ? 255 : 0;
	}

	return binary;
}

/**
 * @brief Binarize a color channel of a bitmap using a given threshold
 * @param bitmap the input bitmap
 * @param channel the color channel
 * @param threshold the threshold
 * @return binarized bitmap | NULL if failed
*/
jab_bitmap* binarizerHard(jab_bitmap* bitmap, jab_int32 channel, jab_int32 threshold)
{
	jab_bitmap* binary = (jab_bitmap*)malloc(sizeof(jab_bitmap) + bitmap->width*bitmap->height*sizeof(jab_byte));
	if(binary == NULL)
	{
		reportError("Memory allocation for binary bitmap failed");
		return NULL;
	}
	binary->width = bitmap->width;
	binary->height= bitmap->height;
	binary->bits_per_channel = 8;
	binary->bits_per_pixel = 8;
	binary->channel_count = 1;
	jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
	//binarize bitmap
	for(jab_int32 i=0; i<bitmap->width*bitmap->height; i++)
	{
		binary->pixel[i] = bitmap->pixel[i*bytes_per_pixel + channel] > threshold ? 255 : 0;
	}
	return binary;
}

/**
 * @brief Do local binarization based on the black points
 * @param bitmap the input bitmap
 * @param channel the color channel
 * @param sub_width the number of blocks in x direction
 * @param sub_height the number of blocks in y direction
 * @param black_points the black points
 * @param binary the binarized bitmap
*/
void getBinaryBitmap(jab_bitmap* bitmap, jab_int32 channel, jab_int32 sub_width, jab_int32 sub_height, jab_byte* black_points, jab_bitmap* binary)
{
    jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
    jab_int32 bytes_per_row = bitmap->width * bytes_per_pixel;

    for (jab_int32 y=0; y<sub_height; y++)
    {
        jab_int32 yoffset = y << BLOCK_SIZE_POWER;
        jab_int32 max_yoffset = bitmap->height - BLOCK_SIZE;
        if (yoffset > max_yoffset)
        {
            yoffset = max_yoffset;
        }
        for (jab_int32 x=0; x<sub_width; x++)
        {
            jab_int32 xoffset = x << BLOCK_SIZE_POWER;
            jab_int32 max_xoffset = bitmap->width - BLOCK_SIZE;
            if (xoffset > max_xoffset)
            {
                xoffset = max_xoffset;
            }
            jab_int32 left = CAP(x, 2, sub_width - 3);
            jab_int32 top = CAP(y, 2, sub_height - 3);
            jab_int32 sum = 0;
            for (jab_int32 z = -2; z <= 2; z++)
            {
                jab_byte* black_row = &black_points[(top + z) * sub_width];
                sum += black_row[left - 2] + black_row[left - 1] + black_row[left] + black_row[left + 1] + black_row[left + 2];
            }
            jab_int32 average = sum / 25;

            //threshold block
            for (jab_int32 yy = 0; yy < BLOCK_SIZE; yy++)
            {
                for (jab_int32 xx = 0; xx < BLOCK_SIZE; xx++)
                {
                    jab_int32 offset = (yoffset + yy) * bytes_per_row + (xoffset + xx) * bytes_per_pixel;
                    if (bitmap->pixel[offset + channel] > average)
                    {
                        binary->pixel[(yoffset + yy) * binary->width + (xoffset + xx)] = 255;
                    }
                }
            }
        }
    }
}

/**
 * @brief Calculate the black point of each block
 * @param bitmap the input bitmap
 * @param channel the color channel
 * @param sub_width the number of blocks in x direction
 * @param sub_height the number of blocks in y direction
 * @param black_points the black points
*/
void calculateBlackPoints(jab_bitmap* bitmap, jab_int32 channel, jab_int32 sub_width, jab_int32 sub_height, jab_byte* black_points)
{
    jab_int32 min_dynamic_range = 24;

    jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
    jab_int32 bytes_per_row = bitmap->width * bytes_per_pixel;

    for(jab_int32 y=0; y<sub_height; y++)
    {
        jab_int32 yoffset = y << BLOCK_SIZE_POWER;
        jab_int32 max_yoffset = bitmap->height - BLOCK_SIZE;
        if (yoffset > max_yoffset)
        {
            yoffset = max_yoffset;
        }
        for (jab_int32 x=0; x<sub_width; x++)
        {
            jab_int32 xoffset = x << BLOCK_SIZE_POWER;
            jab_int32 max_xoffset = bitmap->width - BLOCK_SIZE;
            if (xoffset > max_xoffset)
            {
                xoffset = max_xoffset;
            }
            jab_int32 sum = 0;
            jab_int32 min = 0xFF;
            jab_int32 max = 0;
            for (jab_int32 yy=0; yy<BLOCK_SIZE; yy++)
            {
                for (jab_int32 xx=0; xx<BLOCK_SIZE; xx++)
                {
                    jab_int32 offset = (yoffset + yy) * bytes_per_row + (xoffset + xx) * bytes_per_pixel;
                    jab_byte pixel = bitmap->pixel[offset + channel];
                    sum += pixel;
                    //check contrast
                    if (pixel < min)
                    {
                        min = pixel;
                    }
                    if (pixel > max)
                    {
                        max = pixel;
                    }
                }
                //bypass contrast check once the dynamic range is met
                if (max-min > min_dynamic_range)
                {
                    //finish the rest of the rows quickly
                    for (yy++; yy < BLOCK_SIZE; yy++)
                    {
                        for (jab_int32 xx = 0; xx < BLOCK_SIZE; xx++)
                        {
                            jab_int32 offset = (yoffset + yy) * bytes_per_row + (xoffset + xx) * bytes_per_pixel;
                            sum += bitmap->pixel[offset + channel];
                        }
                    }
                }
            }

            jab_int32 average = sum >> (BLOCK_SIZE_POWER * 2);
            if (max-min <= min_dynamic_range)	//smooth block
            {
                average = min / 2;
                if (y > 0 && x > 0)
                {
                    jab_int32 average_neighbor_blackpoint = (black_points[(y-1) * sub_width + x] +
                                                            (2 * black_points[y * sub_width + x-1]) +
                                                            black_points[(y-1) * sub_width + x-1]) / 4;
                    if (min < average_neighbor_blackpoint)
                    {
                        average = average_neighbor_blackpoint;
                    }
                }
            }
            black_points[y*sub_width + x] = (jab_byte)average;
        }
    }
}

/**
 * @brief Filter out noises in binary bitmap
 * @param binary the binarized bitmap
*/
void filterBinary(jab_bitmap* binary)
{
	jab_int32 width = binary->width;
	jab_int32 height= binary->height;

	jab_int32 filter_size = 5;
	jab_int32 half_size = (filter_size - 1)/2;

	jab_bitmap* tmp = (jab_bitmap*)malloc(sizeof(jab_bitmap) + width*height*sizeof(jab_byte));
	if(tmp == NULL)
	{
		reportError("Memory allocation for temporary binary bitmap failed");
		return;
	}

	//horizontal filtering
	memcpy(tmp, binary, sizeof(jab_bitmap) + width*height*sizeof(jab_byte));
	for(jab_int32 i=half_size; i<height-half_size; i++)
	{
		for(jab_int32 j=half_size; j<width-half_size; j++)
		{
			jab_int32 sum = 0;
			sum += tmp->pixel[i*width + j] > 0 ? 1 : 0;
			for(jab_int32 k=1; k<=half_size; k++)
			{
				sum += tmp->pixel[i*width + (j - k)] > 0 ? 1 : 0;
				sum += tmp->pixel[i*width + (j + k)] > 0 ? 1 : 0;
			}
			binary->pixel[i*width + j] = sum > half_size ? 255 : 0;
		}
	}
	//vertical filtering
	memcpy(tmp, binary, sizeof(jab_bitmap) + width*height*sizeof(jab_byte));
	for(jab_int32 i=half_size; i<height-half_size; i++)
	{
		for(jab_int32 j=half_size; j<width-half_size; j++)
		{
			jab_int32 sum = 0;
			sum += tmp->pixel[i*width + j] > 0 ? 1 : 0;
			for(jab_int32 k=1; k<=half_size; k++)
			{
				sum += tmp->pixel[(i - k)*width + j] > 0 ? 1 : 0;
				sum += tmp->pixel[(i + k)*width + j] > 0 ? 1 : 0;
			}
			binary->pixel[i*width + j] = sum > half_size ? 255 : 0;
		}
	}
	free(tmp);
}

/**
 * @brief Binarize a color channel of a bitmap using local binarization algorithm
 * @param bitmap the input bitmap
 * @param channel the color channel
 * @return binarized bitmap | NULL if failed
*/
jab_bitmap* binarizer(jab_bitmap* bitmap, jab_int32 channel)
{
	if(bitmap->width >= MINIMUM_DIMENSION && bitmap->height >= MINIMUM_DIMENSION)
	{
		jab_int32 sub_width = bitmap->width >> BLOCK_SIZE_POWER;
		if((sub_width & BLOCK_SIZE_MASK) != 0 )	sub_width++;
		jab_int32 sub_height= bitmap->height>> BLOCK_SIZE_POWER;
		if((sub_height& BLOCK_SIZE_MASK) != 0 )	sub_height++;

		jab_byte* black_points = (jab_byte*)malloc(sub_width * sub_height * sizeof(jab_byte));
		if(black_points == NULL)
		{
			reportError("Memory allocation for black points failed");
			return NULL;
		}
		calculateBlackPoints(bitmap, channel, sub_width, sub_height, black_points);

		jab_bitmap* binary = (jab_bitmap*)calloc(1, sizeof(jab_bitmap) + bitmap->width*bitmap->height*sizeof(jab_byte));
		if(binary == NULL)
		{
			reportError("Memory allocation for binary bitmap failed");
			return NULL;
		}
		binary->width = bitmap->width;
		binary->height= bitmap->height;
		binary->bits_per_channel = 8;
		binary->bits_per_pixel = 8;
		binary->channel_count = 1;
		getBinaryBitmap(bitmap, channel, sub_width, sub_height, black_points, binary);

		filterBinary(binary);

		free(black_points);
		return binary;
	}
	else
	{
		//if the bitmap is too small, use the global histogram-based method
		return binarizerHist(bitmap, channel);
	}
}
