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
#include <math.h>

#define BLOCK_SIZE_POWER	5
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
		if((sub_height & BLOCK_SIZE_MASK) != 0 )	sub_height++;

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

/**
 * @brief Get the histogram of a color channel
 * @param bitmap the image
 * @param channel the channel
 * @param hist the histogram
*/
void getHistogram(jab_bitmap* bitmap, jab_int32 channel, jab_int32 hist[256])
{
	//get histogram
	memset(hist, 0, 256*sizeof(jab_int32));
	jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
	for(jab_int32 i=0; i<bitmap->width*bitmap->height; i++)
	{
		hist[bitmap->pixel[i*bytes_per_pixel + channel]]++;
	}
}

/**
 * @brief Get the min and max index in the histogram whose value is larger than the threshold
 * @param hist the histogram
 * @param max the max index
 * @param min the min index
 * @param ths the threshold
*/
void getHistMaxMin(jab_int32 hist[256], jab_int32* max, jab_int32* min, jab_int32 ths)
{
	//get min
	*min = 0;
	for(jab_int32 i=0; i<256; i++)
	{
		if(hist[i] > ths)
		{
			*min = i;
			break;
		}
	}
	//get max
	*max = 255;
	for(jab_int32 i=255; i>=0; i--)
	{
		if(hist[i] > ths)
		{
			*max = i;
			break;
		}
	}
}

/**
 * @brief Stretch the histograms of R, G and B channels
 * @param bitmap the image
*/
void balanceRGB(jab_bitmap* bitmap)
{
	jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
    jab_int32 bytes_per_row = bitmap->width * bytes_per_pixel;

	//calculate max and min for each channel
	jab_int32 max_r, max_g, max_b;
    jab_int32 min_r, min_g, min_b;
    jab_int32 hist_r[256], hist_g[256], hist_b[256];

    getHistogram(bitmap, 0, hist_r);
    getHistogram(bitmap, 1, hist_g);
    getHistogram(bitmap, 2, hist_b);

    //threshold for the number of pixels having the max or min values
	jab_int32 count_ths = 20;
    getHistMaxMin(hist_r, &max_r, &min_r, count_ths);
    getHistMaxMin(hist_g, &max_g, &min_g, count_ths);
    getHistMaxMin(hist_b, &max_b, &min_b, count_ths);

	//normalize each channel
	for(jab_int32 i=0; i<bitmap->height; i++)
	{
		for(jab_int32 j=0; j<bitmap->width; j++)
		{
			jab_int32 offset = i * bytes_per_row + j * bytes_per_pixel;
			//R channel
			if		(bitmap->pixel[offset + 0] < min_r)	bitmap->pixel[offset + 0] = 0;
			else if (bitmap->pixel[offset + 0] > max_r)	bitmap->pixel[offset + 0] = 255;
			else 	 bitmap->pixel[offset + 0] = (jab_byte)((jab_double)(bitmap->pixel[offset + 0] - min_r) / (jab_double)(max_r - min_r) * 255.0);
			//G channel
			if		(bitmap->pixel[offset + 1] < min_g)	bitmap->pixel[offset + 1] = 0;
			else if (bitmap->pixel[offset + 1] > max_g)	bitmap->pixel[offset + 1] = 255;
			else 	 bitmap->pixel[offset + 1] = (jab_byte)((jab_double)(bitmap->pixel[offset + 1] - min_g) / (jab_double)(max_g - min_g) * 255.0);
			//B channel
			if		(bitmap->pixel[offset + 2] < min_b) bitmap->pixel[offset + 2] = 0;
			else if	(bitmap->pixel[offset + 2] > max_b)	bitmap->pixel[offset + 2] = 255;
			else 	 bitmap->pixel[offset + 2] = (jab_byte)((jab_double)(bitmap->pixel[offset + 2] - min_b) / (jab_double)(max_b - min_b) * 255.0);
		}
	}
}

/**
 * @brief Get the average and variance of RGB values
 * @param rgb the pixel with RGB values
 * @param ave the average value
 * @param var the variance value
*/
void getAveVar(jab_byte* rgb, jab_double* ave, jab_double* var)
{
	//calculate mean
	*ave = (rgb[0] + rgb[1] + rgb[2]) / 3;
	//calculate variance
	jab_double sum = 0.0;
	for(jab_int32 i=0; i<3; i++)
	{
		sum += (rgb[i] - (*ave)) * (rgb[i] - (*ave));
	}
	*var = sum / 3;
}

/**
 * @brief Swap two variables
*/
void swap(jab_int32* a, jab_int32* b)
{
	jab_int32 tmp;
	tmp = *a;
	*a = *b;
	*b = tmp;
}

/**
 * @brief Get the min, middle and max value of three values and the corresponding indexes
 * @param rgb the pixel with RGB values
 * @param min the min value
 * @param mid the middle value
 * @param max the max value
*/
void getMinMax(jab_byte* rgb, jab_byte* min, jab_byte* mid, jab_byte* max, jab_int32* index_min, jab_int32* index_mid, jab_int32* index_max)
{
	*index_min = 0;
	*index_mid = 1;
	*index_max = 2;
	if(rgb[*index_min] > rgb[*index_max])
		swap(index_min, index_max);
	if(rgb[*index_min] > rgb[*index_mid])
		swap(index_min, index_mid);
	if(rgb[*index_mid] > rgb[*index_max])
		swap(index_mid, index_max);
	*min = rgb[*index_min];
	*mid = rgb[*index_mid];
	*max = rgb[*index_max];
}

/**
 * @brief Binarize a color channel of a bitmap using local binarization algorithm
 * @param bitmap the input bitmap
 * @param rgb the binarized RGB channels
 * @param blk_ths the black color thresholds for RGB channels
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean binarizerRGB(jab_bitmap* bitmap, jab_bitmap* rgb[3], jab_float* blk_ths)
{
	for(jab_int32 i=0; i<3; i++)
	{
		rgb[i] = (jab_bitmap*)calloc(1, sizeof(jab_bitmap) + bitmap->width*bitmap->height*sizeof(jab_byte));
		if(rgb[i] == NULL)
		{
			JAB_REPORT_ERROR(("Memory allocation for binary bitmap %d failed", i))
			return JAB_FAILURE;
		}
		rgb[i]->width = bitmap->width;
		rgb[i]->height= bitmap->height;
		rgb[i]->bits_per_channel = 8;
		rgb[i]->bits_per_pixel = 8;
		rgb[i]->channel_count = 1;
	}

	jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
    jab_int32 bytes_per_row = bitmap->width * bytes_per_pixel;

    //calculate the average pixel value, block-wise
    jab_int32 max_block_size = MAX(bitmap->width, bitmap->height) / 2;
    jab_int32 block_num_x = (bitmap->width % max_block_size) != 0 ? (bitmap->width / max_block_size) + 1 : (bitmap->width / max_block_size);
    jab_int32 block_num_y = (bitmap->height% max_block_size) != 0 ? (bitmap->height/ max_block_size) + 1 : (bitmap->height/ max_block_size);
    jab_int32 block_size_x = bitmap->width / block_num_x;
    jab_int32 block_size_y = bitmap->height/ block_num_y;
    jab_float pixel_ave[block_num_x*block_num_y][3];
    memset(pixel_ave, 0, sizeof(jab_float)*block_num_x*block_num_y*3);
    if(blk_ths == 0)
    {
        for(jab_int32 i=0; i<block_num_y; i++)
        {
            for(jab_int32 j=0; j<block_num_x; j++)
            {
                jab_int32 block_index = i*block_num_x + j;

                jab_int32 sx = j * block_size_x;
                jab_int32 ex = (j == block_num_x-1) ? bitmap->width : (sx + block_size_x);
                jab_int32 sy = i * block_size_y;
                jab_int32 ey = (i == block_num_y-1) ? bitmap->height: (sy + block_size_y);
                jab_int32 counter = 0;
                for(jab_int32 y=sy; y<ey; y++)
                {
                    for(jab_int32 x=sx; x<ex; x++)
                    {
                        jab_int32 offset = y * bytes_per_row + x * bytes_per_pixel;
                        pixel_ave[block_index][0] += bitmap->pixel[offset + 0];
                        pixel_ave[block_index][1] += bitmap->pixel[offset + 1];
                        pixel_ave[block_index][2] += bitmap->pixel[offset + 2];
                        counter++;
                    }
                }
                pixel_ave[block_index][0] /= (jab_float)counter;
                pixel_ave[block_index][1] /= (jab_float)counter;
                pixel_ave[block_index][2] /= (jab_float)counter;
            }
        }
    }

	//binarize each pixel in each channel
	jab_double ths_std = 0.08;
	jab_float rgb_ths[3] = {0, 0, 0};
    for(jab_int32 i=0; i<bitmap->height; i++)
	{
		for(jab_int32 j=0; j<bitmap->width; j++)
		{
			jab_int32 offset = i * bytes_per_row + j * bytes_per_pixel;
			//check black pixel
			if(blk_ths == 0)
            {
                jab_int32 block_index = MIN(i/block_size_y, block_num_y-1) * block_num_x + MIN(j/block_size_x, block_num_x-1);
                rgb_ths[0] = pixel_ave[block_index][0];
                rgb_ths[1] = pixel_ave[block_index][1];
                rgb_ths[2] = pixel_ave[block_index][2];
            }
            else
            {
                rgb_ths[0] = blk_ths[0];
                rgb_ths[1] = blk_ths[1];
                rgb_ths[2] = blk_ths[2];
            }
			if(bitmap->pixel[offset + 0] < rgb_ths[0] && bitmap->pixel[offset + 1] < rgb_ths[1] && bitmap->pixel[offset + 2] < rgb_ths[2])
            {
                rgb[0]->pixel[i*bitmap->width + j] = 0;
                rgb[1]->pixel[i*bitmap->width + j] = 0;
                rgb[2]->pixel[i*bitmap->width + j] = 0;
                continue;
            }

			jab_double ave, var;
			getAveVar(&bitmap->pixel[offset], &ave, &var);
			jab_double std = sqrt(var);	//standard deviation
			jab_byte min, mid, max;
			jab_int32 index_min, index_mid, index_max;
			getMinMax(&bitmap->pixel[offset], &min, &mid, &max, &index_min, &index_mid, &index_max);
			std /= (jab_double)max;	//normalize std

			if(std < ths_std && (bitmap->pixel[offset + 0] > rgb_ths[0] && bitmap->pixel[offset + 1] > rgb_ths[1] && bitmap->pixel[offset + 2] > rgb_ths[2]))
			{
				rgb[0]->pixel[i*bitmap->width + j] = 255;
				rgb[1]->pixel[i*bitmap->width + j] = 255;
				rgb[2]->pixel[i*bitmap->width + j] = 255;
			}
			else
			{
				rgb[index_max]->pixel[i*bitmap->width + j] = 255;
				rgb[index_min]->pixel[i*bitmap->width + j] = 0;
				jab_double r1 = (jab_double)bitmap->pixel[offset + index_mid] / (jab_double)bitmap->pixel[offset + index_min];
				jab_double r2 = (jab_double)bitmap->pixel[offset + index_max] / (jab_double)bitmap->pixel[offset + index_mid];
				if(r1 > r2)
					rgb[index_mid]->pixel[i*bitmap->width + j] = 255;
				else
					rgb[index_mid]->pixel[i*bitmap->width + j] = 0;
			}
		}
	}
	filterBinary(rgb[0]);
	filterBinary(rgb[1]);
	filterBinary(rgb[2]);
	return JAB_SUCCESS;
}
