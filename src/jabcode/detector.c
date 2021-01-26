/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file detector.c
 * @brief JABCode detector
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jabcode.h"
#include "detector.h"
#include "decoder.h"
#include "encoder.h"

/**
 * @brief Check the proportion of layer sizes in finder pattern
 * @param state_count the layer sizes in pixel
 * @param module_size the module size
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean checkPatternCross(jab_int32* state_count, jab_float* module_size)
{
    jab_int32 layer_number = 3;
    jab_int32 inside_layer_size = 0;
    for(jab_int32 i=1; i<layer_number+1; i++)
    {
        if(state_count[i] == 0)
            return JAB_FAILURE;
        inside_layer_size += state_count[i];
    }

    jab_float layer_size = (jab_float)inside_layer_size / 3.0f;
    *module_size = layer_size;
    jab_float layer_tolerance = layer_size / 2.0f;

    jab_boolean size_condition;
	//layer size proportion must be n-1-1-1-m where n>1, m>1
	size_condition = fabs(layer_size - (jab_float)state_count[1]) < layer_tolerance &&
					 fabs(layer_size - (jab_float)state_count[2]) < layer_tolerance &&
					 fabs(layer_size - (jab_float)state_count[3]) < layer_tolerance &&
					 (jab_float)state_count[0] > 0.5 * layer_tolerance && //the two outside layers can be larger than layer_size
					 (jab_float)state_count[4] > 0.5 * layer_tolerance &&
					 fabs(state_count[1] - state_count[3]) < layer_tolerance; //layer 1 and layer 3 shall be of the same size

    return size_condition;
}

/**
 * @brief Check if the input module sizes are the same
 * @param size_r the first module size
 * @param size_g the second module size
 * @param size_b the third module size
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean checkModuleSize3(jab_float size_r, jab_float size_g, jab_float size_b)
{
	jab_float mean= (size_r + size_g + size_b) / 3.0f;
	jab_float tolerance = mean / 2.5f;

	jab_boolean condition = fabs(mean - size_r) < tolerance &&
							fabs(mean - size_g) < tolerance &&
							fabs(mean - size_b) < tolerance;
	return condition;
}

/**
 * @brief Check if the input module sizes are the same
 * @param size1 the first module size
 * @param size2 the second module size

 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean checkModuleSize2(jab_float size1, jab_float size2)
{
	jab_float mean= (size1 + size2) / 2.0f;
	jab_float tolerance = mean / 2.5f;

	jab_boolean condition = fabs(mean - size1) < tolerance &&
							fabs(mean - size2) < tolerance;

	return condition;
}

/**
 * @brief Find a candidate scanline of finder pattern
 * @param ch the image channel
 * @param row the row to be scanned
 * @param col the column to be scanned
 * @param start the start position
 * @param end the end position
 * @param center the center of the candidate scanline
 * @param module_size the module size
 * @param skip the number of pixels to be skipped in the next scan
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean seekPattern(jab_bitmap* ch, jab_int32 row, jab_int32 col, jab_int32* start, jab_int32* end, jab_float* center, jab_float* module_size, jab_int32* skip)
{
    jab_int32 state_number = 5;
    jab_int32 cur_state = 0;
    jab_int32 state_count[5] = {0};

    jab_int32 min = *start;
    jab_int32 max = *end;
    for(jab_int32 p=min; p<max; p++)
    {
        //first pixel in a scanline
        if(p == min)
        {
            state_count[cur_state]++;
            *start = p;
        }
        else
        {
           	//previous pixel and current pixel
			jab_byte prev;
			jab_byte curr;
			if(row >= 0)		//horizontal scan
			{
				prev = ch->pixel[row*ch->width + (p-1)];
				curr = ch->pixel[row*ch->width + p];
			}
			else if(col >= 0)	//vertical scan
			{
				prev = ch->pixel[(p-1)*ch->width + col];
				curr = ch->pixel[p*ch->width + col];
			}
			else
				return JAB_FAILURE;
            //the pixel has the same color as the preceding pixel
            if(curr == prev)
            {
                state_count[cur_state]++;
            }
            //the pixel has different color from the preceding pixel
            if(curr != prev || p == max-1)
            {
                //change state
                if(cur_state < state_number-1)
                {
                    //check if the current state is valid
                    if(state_count[cur_state] < 3)
                    {
                        if(cur_state == 0)
                        {
                            state_count[cur_state]=1;
                            *start = p;
                        }
                        else
                        {
                            //combine the current state to the previous one and continue the previous state
                            state_count[cur_state-1] += state_count[cur_state];
                            state_count[cur_state] = 0;
                            cur_state--;
                            state_count[cur_state]++;
                        }
                    }
                    else
                    {
                        //enter the next state
                        cur_state++;
                        state_count[cur_state]++;
                    }
                }
                //find a candidate
                else
                {
                    if(state_count[cur_state] < 3)
                    {
                        //combine the current state to the previous one and continue the previous state
                        state_count[cur_state-1] += state_count[cur_state];
                        state_count[cur_state] = 0;
                        cur_state--;
                        state_count[cur_state]++;
                        continue;
                    }
                    //check if it is a valid finder pattern
                    if(checkPatternCross(state_count, module_size))
                    {
                        *end = p+1;
                        if(skip)  *skip = state_count[0];
						jab_int32 end_pos;
						if(p == (max - 1) && curr == prev) end_pos = p + 1;
						else end_pos = p;
						*center = (jab_float)(end_pos - state_count[4] - state_count[3]) - (jab_float)state_count[2] / 2.0f;
                        return JAB_SUCCESS;
                    }
                    else //check failed, update state_count
                    {
                        *start += state_count[0];
                        for(jab_int32 k=0; k<state_number-1; k++)
                        {
                            state_count[k] = state_count[k+1];
                        }
                        state_count[state_number-1] = 1;
                        cur_state = state_number-1;
                    }
                }
            }
        }
    }
    *end = max;
    return JAB_FAILURE;
}

/**
 * @brief Find a candidate horizontal scanline of finder pattern
 * @param row the bitmap row
 * @param startx the start position
 * @param endx the end position
 * @param centerx the center of the candidate scanline
 * @param module_size the module size
 * @param skip the number of pixels to be skipped in the next scan
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean seekPatternHorizontal(jab_byte* row, jab_int32* startx, jab_int32* endx, jab_float* centerx, jab_float* module_size, jab_int32* skip)
{
    jab_int32 state_number = 5;
    jab_int32 cur_state = 0;
    jab_int32 state_count[5] = {0};

    jab_int32 min = *startx;
    jab_int32 max = *endx;
    for(jab_int32 j=min; j<max; j++)
    {
        //first pixel in a scanline
        if(j == min)
        {
            state_count[cur_state]++;
            *startx = j;
        }
        else
        {
            //the pixel has the same color as the preceding pixel
            if(row[j] == row[j-1])
            {
                state_count[cur_state]++;
            }
            //the pixel has different color from the preceding pixel
            if(row[j] != row[j-1] || j == max-1)
            {
                //change state
                if(cur_state < state_number-1)
                {
                    //check if the current state is valid
                    if(state_count[cur_state] < 3)
                    {
                        if(cur_state == 0)
                        {
                            state_count[cur_state]=1;
                            *startx = j;
                        }
                        else
                        {
                            //combine the current state to the previous one and continue the previous state
                            state_count[cur_state-1] += state_count[cur_state];
                            state_count[cur_state] = 0;
                            cur_state--;
                            state_count[cur_state]++;
                        }
                    }
                    else
                    {
                        //enter the next state
                        cur_state++;
                        state_count[cur_state]++;
                    }
                }
                //find a candidate
                else
                {
                    if(state_count[cur_state] < 3)
                    {
                        //combine the current state to the previous one and continue the previous state
                        state_count[cur_state-1] += state_count[cur_state];
                        state_count[cur_state] = 0;
                        cur_state--;
                        state_count[cur_state]++;
                        continue;
                    }
                    //check if it is a valid finder pattern
                    if(checkPatternCross(state_count, module_size))
                    {
                        *endx = j+1;
                        if(skip)  *skip = state_count[0];
						jab_int32 end;
						if(j == (max - 1) && row[j] == row[j-1]) end = j + 1;
						else end = j;
						*centerx = (jab_float)(end - state_count[4] - state_count[3]) - (jab_float)state_count[2] / 2.0f;
                        return JAB_SUCCESS;
                    }
                    else //check failed, update state_count
                    {
                        *startx += state_count[0];
                        for(jab_int32 k=0; k<state_number-1; k++)
                        {
                            state_count[k] = state_count[k+1];
                        }
                        state_count[state_number-1] = 1;
                        cur_state = state_number-1;
                    }
                }
            }
        }
    }
    *endx = max;
    return JAB_FAILURE;
}

/**
 * @brief Crosscheck the finder pattern candidate in diagonal direction
 * @param image the image bitmap
 * @param type the finder pattern type
 * @param module_size_max the maximal allowed module size
 * @param centerx the x coordinate of the finder pattern center
 * @param centery the y coordinate of the finder pattern center
 * @param module_size the module size in diagonal direction
 * @param dir the finder pattern direction
 * @param both_dir scan both diagonal scanlines
 * @return the number of confirmed diagonal scanlines
*/
jab_int32 crossCheckPatternDiagonal(jab_bitmap* image, jab_int32 type, jab_float module_size_max, jab_float* centerx, jab_float* centery, jab_float* module_size, jab_int32* dir, jab_boolean both_dir)
{
    jab_int32 state_number = 5;
    jab_int32 state_middle = (state_number - 1) / 2;

    jab_int32 offset_x, offset_y;
    jab_boolean fix_dir = 0;
    //if the direction is given, ONLY check the given direction
    if((*dir) != 0)
    {
		if((*dir) > 0)
		{
			offset_x = -1;
			offset_y = -1;
			*dir = 1;
		}
		else
		{
			offset_x = 1;
			offset_y = -1;
			*dir = -1;
		}
		fix_dir = 1;
    }
    else
    {
		//for fp0 (and fp1 in 4 color mode), first check the diagonal at +45 degree
		if(type == FP0 || type == FP1)
		{
			offset_x = -1;
			offset_y = -1;
			*dir = 1;
		}
		//for fp2, fp3, (and fp1 in 2 color mode) first check the diagonal at -45 degree
		else
		{
			offset_x = 1;
			offset_y = -1;
			*dir = -1;
		}
    }

    jab_int32 confirmed = 0;
    jab_boolean flag = 0;
    jab_int32 try_count = 0;
    jab_float tmp_module_size = 0.0f;
    do
    {
		flag = 0;
		try_count++;

		jab_int32 i, j, state_index;
        jab_int32 state_count[5] = {0};
        jab_int32 startx = (jab_int32)(*centerx);
        jab_int32 starty = (jab_int32)(*centery);

        state_count[state_middle]++;
        for(j=1, state_index=0; (starty+j*offset_y)>=0 && (starty+j*offset_y)<image->height && (startx+j*offset_x)>=0 && (startx+j*offset_x)<image->width && state_index<=state_middle; j++)
        {
            if( image->pixel[(starty + j*offset_y)*image->width + (startx + j*offset_x)] == image->pixel[(starty + (j-1)*offset_y)*image->width + (startx + (j-1)*offset_x)] )
            {
                state_count[state_middle - state_index]++;
            }
            else
            {
                if(state_index >0 && state_count[state_middle - state_index] < 3)
                {
                    state_count[state_middle - (state_index-1)] += state_count[state_middle - state_index];
                    state_count[state_middle - state_index] = 0;
                    state_index--;
                    state_count[state_middle - state_index]++;
                }
                else
                {
                    state_index++;
                    if(state_index > state_middle) break;
                    else state_count[state_middle - state_index]++;
                }
            }
        }
        if(state_index < state_middle)
        {
			if(try_count == 1)
			{
				flag = 1;
				offset_x = -offset_x;
				*dir = -(*dir);
			}
			else
				return confirmed;
		}

		if(!flag)
		{
			for(i=1, state_index=0; (starty-i*offset_y)>=0 && (starty-i*offset_y)<image->height && (startx-i*offset_x)>=0 && (startx-i*offset_x)<image->width && state_index<=state_middle; i++)
			{
				if( image->pixel[(starty - i*offset_y)*image->width + (startx - i*offset_x)] == image->pixel[(starty - (i-1)*offset_y)*image->width + (startx - (i-1)*offset_x)] )
				{
					state_count[state_middle + state_index]++;
				}
				else
				{
					if(state_index >0 && state_count[state_middle + state_index] < 3)
					{
						state_count[state_middle + (state_index-1)] += state_count[state_middle + state_index];
						state_count[state_middle + state_index] = 0;
						state_index--;
						state_count[state_middle + state_index]++;
					}
					else
					{
						state_index++;
						if(state_index > state_middle) break;
						else state_count[state_middle + state_index]++;
					}
				}
			}
			if(state_index < state_middle)
			{
				if(try_count == 1)
				{
					flag = 1;
					offset_x = -offset_x;
					*dir = -(*dir);
				}
				else
					return confirmed;
			}
		}

		if(!flag)
		{
			//check module size, if it is too big, assume it is a false positive
			jab_boolean ret = checkPatternCross(state_count, module_size);
			if(ret && ((*module_size) <= module_size_max))
			{
				if(tmp_module_size > 0)
					*module_size = (*module_size + tmp_module_size) / 2.0f;
				else
					tmp_module_size = *module_size;

				//calculate the center x
				*centerx = (jab_float)(startx+i - state_count[4] - state_count[3]) - (jab_float)state_count[2] / 2.0f;
				//calculate the center y
				*centery = (jab_float)(starty+i - state_count[4] - state_count[3]) - (jab_float)state_count[2] / 2.0f;
				confirmed++;
				if( !both_dir || try_count == 2 || fix_dir)
				{
					if(confirmed == 2)
						*dir = 2;
					return confirmed;
				}
			}
			else
			{
				offset_x = -offset_x;
				*dir = -(*dir);
			}
        }
    }while(try_count < 2 && !fix_dir);

    return confirmed;
}

/**
 * @brief Crosscheck the finder pattern candidate in vertical direction
 * @param image the image bitmap
 * @param module_size_max the maximal allowed module size
 * @param centerx the x coordinate of the finder pattern center
 * @param centery the y coordinate of the finder pattern center
 * @param module_size the module size in vertical direction
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean crossCheckPatternVertical(jab_bitmap* image, jab_int32 module_size_max, jab_float centerx, jab_float* centery, jab_float* module_size)
{
	jab_int32 state_number = 5;
	jab_int32 state_middle = (state_number - 1) / 2;
    jab_int32 state_count[5] = {0};

    jab_int32 centerx_int = (jab_int32)centerx;
	jab_int32 centery_int = (jab_int32)(*centery);
    jab_int32 i, state_index;

    state_count[1]++;
    for(i=1, state_index=0; i<=centery_int && state_index<=state_middle; i++)
    {
        if( image->pixel[(centery_int-i)*image->width + centerx_int] == image->pixel[(centery_int-(i-1))*image->width + centerx_int] )
        {
            state_count[state_middle - state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[state_middle - state_index] < 3)
            {
                state_count[state_middle - (state_index-1)] += state_count[state_middle - state_index];
                state_count[state_middle - state_index] = 0;
                state_index--;
                state_count[state_middle - state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > state_middle) break;
                else state_count[state_middle - state_index]++;
            }
        }
    }
    if(state_index < state_middle)
        return JAB_FAILURE;

    for(i=1, state_index=0; (centery_int+i)<image->height && state_index<=state_middle; i++)
    {
        if( image->pixel[(centery_int+i)*image->width + centerx_int] == image->pixel[(centery_int+(i-1))*image->width + centerx_int] )
        {
            state_count[state_middle + state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[state_middle + state_index] < 3)
            {
                state_count[state_middle + (state_index-1)] += state_count[state_middle + state_index];
                state_count[state_middle + state_index] = 0;
                state_index--;
                state_count[state_middle + state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > state_middle) break;
                else state_count[state_middle + state_index]++;
            }
        }
    }
    if(state_index < state_middle)
        return JAB_FAILURE;

    //check module size, if it is too big, assume it is a false positive
    jab_boolean ret = checkPatternCross(state_count, module_size);
    if(ret && ((*module_size) <= module_size_max))
    {
        //calculate the center y
        *centery = (jab_float)(centery_int + i - state_count[4] - state_count[3]) - (jab_float)state_count[2] / 2.0f;
        return JAB_SUCCESS;
    }
    return JAB_FAILURE;
}

/**
 * @brief Crosscheck the finder pattern candidate in horizontal direction
 * @param image the image bitmap
 * @param module_size_max the maximal allowed module size
 * @param centerx the x coordinate of the finder pattern center
 * @param centery the y coordinate of the finder pattern center
 * @param module_size the module size in horizontal direction
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean crossCheckPatternHorizontal(jab_bitmap* image, jab_float module_size_max, jab_float* centerx, jab_float centery, jab_float* module_size)
{
    jab_int32 state_number = 5;
    jab_int32 state_middle = (state_number - 1) / 2;
    jab_int32 state_count[5] = {0};

    jab_int32 startx = (jab_int32)(*centerx);
    jab_int32 offset = (jab_int32)centery * image->width;
    jab_int32 i, state_index;

    state_count[state_middle]++;
    for(i=1, state_index=0; i<=startx && state_index<=state_middle; i++)
    {
        if( image->pixel[offset + (startx - i)] == image->pixel[offset + (startx - (i-1))] )
        {
            state_count[state_middle - state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[state_middle - state_index] < 3)
            {
                state_count[state_middle - (state_index-1)] += state_count[state_middle - state_index];
                state_count[state_middle - state_index] = 0;
                state_index--;
                state_count[state_middle - state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > state_middle) break;
                else state_count[state_middle - state_index]++;
            }
        }
    }
    if(state_index < state_middle)
        return JAB_FAILURE;

    for(i=1, state_index=0; (startx+i)<image->width && state_index<=state_middle; i++)
    {
        if( image->pixel[offset + (startx + i)] == image->pixel[offset + (startx + (i-1))] )
        {
            state_count[state_middle + state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[state_middle + state_index] < 3)
            {
                state_count[state_middle + (state_index-1)] += state_count[state_middle + state_index];
                state_count[state_middle + state_index] = 0;
                state_index--;
                state_count[state_middle + state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > state_middle) break;
                else state_count[state_middle + state_index]++;
            }
        }
    }
    if(state_index < state_middle)
        return JAB_FAILURE;

    //check module size, if it is too big, assume it is a false positive
    jab_boolean ret = checkPatternCross(state_count, module_size);
    if(ret && ((*module_size) <= module_size_max))
    {
        //calculate the center x
        *centerx = (jab_float)(startx+i - state_count[4] - state_count[3]) - (jab_float)state_count[2] / 2.0f;
        return JAB_SUCCESS;
    }
    return JAB_FAILURE;
}

/**
 * @brief Crosscheck the finder pattern color
 * @param image the image bitmap
 * @param color the expected module color
 * @param module_size the module size
 * @param module_number the number of modules that should be checked
 * @param centerx the x coordinate of the finder pattern center
 * @param centery the y coordinate of the finder pattern center
 * @param dir the check direction
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean crossCheckColor(jab_bitmap* image, jab_int32 color, jab_int32 module_size, jab_int32 module_number, jab_int32 centerx, jab_int32 centery, jab_int32 dir)
{
	jab_int32 tolerance = 3;
	//horizontal
	if(dir == 0)
	{
		jab_int32 length = module_size * (module_number - 1); //module number minus one for better tolerance
		jab_int32 startx = (centerx - length/2) < 0 ? 0 : (centerx - length/2);
		jab_int32 unmatch = 0;
		for(jab_int32 j=startx; j<(startx+length) && j<image->width; j++)
		{
			if(image->pixel[centery * image->width + j] != color) unmatch++;
			else
			{
				if(unmatch <= tolerance) unmatch = 0;
			}
			if(unmatch > tolerance)	return JAB_FAILURE;
		}
		return JAB_SUCCESS;
	}
	//vertical
	else if(dir == 1)
	{
		jab_int32 length = module_size * (module_number - 1);
		jab_int32 starty = (centery - length/2) < 0 ? 0 : (centery - length/2);
		jab_int32 unmatch = 0;
		for(jab_int32 i=starty; i<(starty+length) && i<image->height; i++)
		{
			if(image->pixel[image->width * i + centerx] != color) unmatch++;
			else
			{
				if(unmatch <= tolerance) unmatch = 0;
			}
			if(unmatch > tolerance)	return JAB_FAILURE;
		}
		return JAB_SUCCESS;
	}
	//diagonal
	else if(dir == 2)
	{
		jab_int32 offset = module_size * (module_number / (2.0f * 1.41421f)); // e.g. for FP, module_size * (5/(2*sqrt(2));
		jab_int32 length = offset * 2;

		//one direction
		jab_int32 unmatch = 0;
		jab_int32 startx = (centerx - offset) < 0 ? 0 : (centerx - offset);
		jab_int32 starty = (centery - offset) < 0 ? 0 : (centery - offset);
		for(jab_int32 i=0; i<length && (starty+i)<image->height; i++)
		{
			if(image->pixel[image->width * (starty+i) + (startx+i)] != color) unmatch++;
			else
			{
				if(unmatch <= tolerance) unmatch = 0;
			}
			if(unmatch > tolerance) break;
		}
		if(unmatch < tolerance) return JAB_SUCCESS;

		//the other direction
		unmatch = 0;
		startx = (centerx - offset) < 0 ? 0 : (centerx - offset);
		starty = (centery + offset) > (image->height - 1) ? (image->height - 1) : (centery + offset);
		for(jab_int32 i=0; i<length && (starty-i)>=0; i++)
		{
			if(image->pixel[image->width * (starty-i) + (startx+i)] != color) unmatch++;
			else
			{
				if(unmatch <= tolerance) unmatch = 0;
			}
			if(unmatch > tolerance) return JAB_FAILURE;
		}
		return JAB_SUCCESS;
	}
	else
	{
		JAB_REPORT_ERROR(("Invalid direction"))
		return JAB_FAILURE;
	}
}

/**
 * @brief Crosscheck the finder pattern candidate in one channel
 * @param ch the binarized color channel
 * @param type the finder pattern type
 * @param h_v the direction of the candidate scanline, 0:horizontal 1:vertical
 * @param module_size_max the maximal allowed module size
 * @param module_size the module size in all directions
 * @param centerx the x coordinate of the finder pattern center
 * @param centery the y coordinate of the finder pattern center
 * @param dir the finder pattern direction
 * @param dcc the diagonal crosscheck result
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean crossCheckPatternCh(jab_bitmap* ch, jab_int32 type, jab_int32 h_v, jab_float module_size_max, jab_float* module_size, jab_float* centerx, jab_float* centery, jab_int32* dir, jab_int32* dcc)
{
	jab_float module_size_v = 0.0f;
	jab_float module_size_h = 0.0f;
	jab_float module_size_d = 0.0f;

	if(h_v == 0)
	{
		jab_boolean vcc = JAB_FAILURE;
		if(crossCheckPatternVertical(ch, module_size_max, *centerx, centery, &module_size_v))
		{
			vcc = JAB_SUCCESS;
			if(!crossCheckPatternHorizontal(ch, module_size_max, centerx, *centery, &module_size_h))
				return JAB_FAILURE;
		}
		*dcc = crossCheckPatternDiagonal(ch, type, module_size_max, centerx, centery, &module_size_d, dir, !vcc);
		if(vcc && *dcc > 0)
		{
			*module_size = (module_size_v + module_size_h + module_size_d) / 3.0f;
			return JAB_SUCCESS;
		}
		else if(*dcc == 2)
		{
			if(!crossCheckPatternHorizontal(ch, module_size_max, centerx, *centery, &module_size_h))
				return JAB_FAILURE;
			*module_size = (module_size_h + module_size_d * 2.0f) / 3.0f;
			return JAB_SUCCESS;
		}
	}
	else
	{
		jab_boolean hcc = JAB_FAILURE;
		if(crossCheckPatternHorizontal(ch, module_size_max, centerx, *centery, &module_size_h))
		{
			hcc = JAB_SUCCESS;
			if(!crossCheckPatternVertical(ch, module_size_max, *centerx, centery, &module_size_v))
				return JAB_FAILURE;
		}
		*dcc = crossCheckPatternDiagonal(ch, type, module_size_max, centerx, centery, &module_size_d, dir, !hcc);
		if(hcc && *dcc > 0)
		{
			*module_size = (module_size_v + module_size_h + module_size_d) / 3.0f;
			return JAB_SUCCESS;
		}
		else if(*dcc == 2)
		{
			if(!crossCheckPatternVertical(ch, module_size_max, *centerx, centery, &module_size_v))
				return JAB_FAILURE;
			*module_size = (module_size_v + module_size_d * 2.0f) / 3.0f;
			return JAB_SUCCESS;
		}
	}
	return JAB_FAILURE;
}

/**
 * @brief Crosscheck the finder pattern candidate
 * @param ch the binarized color channels of the image
 * @param fp the finder pattern
 * @param h_v the direction of the candidate scanline, 0:horizontal 1:vertical
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean crossCheckPattern(jab_bitmap* ch[], jab_finder_pattern* fp, jab_int32 h_v)
{
    jab_float module_size_max = fp->module_size * 2.0f;

    //check g channel
	jab_float module_size_g;
    jab_float centerx_g = fp->center.x;
    jab_float centery_g = fp->center.y;
    jab_int32 dir_g = 0;
    jab_int32 dcc_g = 0;
    if(!crossCheckPatternCh(ch[1], fp->type, h_v, module_size_max, &module_size_g, &centerx_g, &centery_g, &dir_g, &dcc_g))
		return JAB_FAILURE;

	//Finder Pattern FP1 and FP2
    if(fp->type == FP1 || fp->type == FP2)
	{
		//check r channel
		jab_float module_size_r;
		jab_float centerx_r = fp->center.x;
		jab_float centery_r = fp->center.y;
		jab_int32 dir_r = 0;
		jab_int32 dcc_r = 0;
		if(!crossCheckPatternCh(ch[0], fp->type, h_v, module_size_max, &module_size_r, &centerx_r, &centery_r, &dir_r, &dcc_r))
			return JAB_FAILURE;

		//module size must be consistent
		if(!checkModuleSize2(module_size_r, module_size_g))
			return JAB_FAILURE;
		fp->module_size = (module_size_r + module_size_g) / 2.0f;
		fp->center.x = (centerx_r + centerx_g) / 2.0f;
		fp->center.y = (centery_r + centery_g) / 2.0f;

		//check b channel
		jab_int32 core_color_in_blue_channel = jab_default_palette[FP2_CORE_COLOR * 3 + 2];
		if(!crossCheckColor(ch[2], core_color_in_blue_channel, (jab_int32)fp->module_size, 5, (jab_int32)fp->center.x, (jab_int32)fp->center.y, 0))
			return JAB_FAILURE;
		if(!crossCheckColor(ch[2], core_color_in_blue_channel, (jab_int32)fp->module_size, 5, (jab_int32)fp->center.x, (jab_int32)fp->center.y, 1))
			return JAB_FAILURE;
		if(!crossCheckColor(ch[2], core_color_in_blue_channel, (jab_int32)fp->module_size, 5, (jab_int32)fp->center.x, (jab_int32)fp->center.y, 2))
			return JAB_FAILURE;

		if(dcc_r == 2 || dcc_g == 2)
			fp->direction = 2;
		else
			fp->direction = (dir_r + dir_g) > 0 ? 1 : -1;
	}

	//Finder Pattern FP0 and FP3
	if(fp->type == FP0 || fp->type == FP3)
	{
		//check b channel
		jab_float module_size_b;
		jab_float centerx_b = fp->center.x;
		jab_float centery_b = fp->center.y;
		jab_int32 dir_b = 0;
		jab_int32 dcc_b = 0;
		if(!crossCheckPatternCh(ch[2], fp->type, h_v, module_size_max, &module_size_b, &centerx_b, &centery_b, &dir_b, &dcc_b))
			return JAB_FAILURE;

		//module size must be consistent
		if(!checkModuleSize2(module_size_g, module_size_b))
			return JAB_FAILURE;
		fp->module_size = (module_size_g + module_size_b) / 2.0f;
		fp->center.x = (centerx_g + centerx_b) / 2.0f;
		fp->center.y = (centery_g + centery_b) / 2.0f;

		//check r channel
		jab_int32 core_color_in_red_channel = jab_default_palette[FP3_CORE_COLOR * 3 + 0];
		if(!crossCheckColor(ch[0], core_color_in_red_channel, (jab_int32)fp->module_size, 5, (jab_int32)fp->center.x, (jab_int32)fp->center.y, 0))
			return JAB_FAILURE;
		if(!crossCheckColor(ch[0], core_color_in_red_channel, (jab_int32)fp->module_size, 5, (jab_int32)fp->center.x, (jab_int32)fp->center.y, 1))
			return JAB_FAILURE;
		if(!crossCheckColor(ch[0], core_color_in_red_channel, (jab_int32)fp->module_size, 5, (jab_int32)fp->center.x, (jab_int32)fp->center.y, 2))
			return JAB_FAILURE;

		if(dcc_g == 2 || dcc_b == 2)
			fp->direction = 2;
		else
			fp->direction = (dir_g + dir_b) > 0 ? 1 : -1;
	}

	return JAB_SUCCESS;
}

/**
 * @brief Save a found alignment pattern into the alignment pattern list
 * @param ap the alignment pattern
 * @param aps the alignment pattern list
 * @param counter the number of alignment patterns in the list
 * @return  -1 if added as a new alignment pattern | the alignment pattern index if combined with an existing pattern
*/
jab_int32 saveAlignmentPattern(jab_alignment_pattern* ap, jab_alignment_pattern* aps, jab_int32* counter)
{
    //combine the alignment patterns at the same position with the same size
    for(jab_int32 i=0; i<(*counter); i++)
    {
        if(aps[i].found_count > 0)
        {
            if( fabs(ap->center.x - aps[i].center.x) <= ap->module_size && fabs(ap->center.y - aps[i].center.y) <= ap->module_size &&
                (fabs(ap->module_size - aps[i].module_size) <= aps[i].module_size || fabs(ap->module_size - aps[i].module_size) <= 1.0) &&
                ap->type == aps[i].type )
            {
                aps[i].center.x = ((jab_float)aps[i].found_count * aps[i].center.x + ap->center.x) / (jab_float)(aps[i].found_count + 1);
                aps[i].center.y = ((jab_float)aps[i].found_count * aps[i].center.y + ap->center.y) / (jab_float)(aps[i].found_count + 1);
                aps[i].module_size = ((jab_float)aps[i].found_count * aps[i].module_size + ap->module_size) / (jab_float)(aps[i].found_count + 1);
                aps[i].found_count++;
                return i;
            }
        }
    }
    //add a new alignment pattern
    aps[*counter] = *ap;
    (*counter)++;
    return -1;
}

/**
 * @brief Save a found finder pattern into the finder pattern list
 * @param fp the finder pattern
 * @param fps the finder pattern list
 * @param counter the number of finder patterns in the list
 * @param fp_type_count the number of finder pattern types in the list
*/
void saveFinderPattern(jab_finder_pattern* fp, jab_finder_pattern* fps, jab_int32* counter, jab_int32* fp_type_count)
{
    //combine the finder patterns at the same position with the same size
    for(jab_int32 i=0; i<(*counter); i++)
    {
        if(fps[i].found_count > 0)
        {
            if( fabs(fp->center.x - fps[i].center.x) <= fp->module_size && fabs(fp->center.y - fps[i].center.y) <= fp->module_size &&
                (fabs(fp->module_size - fps[i].module_size) <= fps[i].module_size || fabs(fp->module_size - fps[i].module_size) <= 1.0) &&
                fp->type == fps[i].type)
            {
                fps[i].center.x = ((jab_float)fps[i].found_count * fps[i].center.x + fp->center.x) / (jab_float)(fps[i].found_count + 1);
                fps[i].center.y = ((jab_float)fps[i].found_count * fps[i].center.y + fp->center.y) / (jab_float)(fps[i].found_count + 1);
                fps[i].module_size = ((jab_float)fps[i].found_count * fps[i].module_size + fp->module_size) / (jab_float)(fps[i].found_count + 1);
                fps[i].found_count++;
                fps[i].direction += fp->direction;
                return;
            }
        }
    }
    //add a new finder pattern
    fps[*counter] = *fp;
    (*counter)++;
    fp_type_count[fp->type]++;
}

#if TEST_MODE
void drawFoundFinderPatterns(jab_finder_pattern* fps, jab_int32 number, jab_int32 color)
{
    jab_int32 bytes_per_pixel = test_mode_bitmap->bits_per_pixel / 8;
    jab_int32 bytes_per_row = test_mode_bitmap->width * bytes_per_pixel;
    for(jab_int32 k = 0; k<number; k++)
    {
        if(fps[k].found_count > 0)
        {
            jab_int32 centerx = (jab_int32)(fps[k].center.x + 0.5f);
            jab_int32 centery = (jab_int32)(fps[k].center.y + 0.5f);

            jab_int32 starty = (jab_int32)(fps[k].center.y - fps[k].module_size/2 + 0.5f);
            jab_int32 endy = (jab_int32)(fps[k].center.y + fps[k].module_size/2 + 1 + 0.5f);
            for(jab_int32 i=starty; i<endy; i++)
            {
				if(i*bytes_per_row + centerx*bytes_per_pixel + 2 < test_mode_bitmap->width * test_mode_bitmap->height * bytes_per_pixel)
				{
					test_mode_bitmap->pixel[i*bytes_per_row + centerx*bytes_per_pixel] 	   = (color >> 16) & 0xff;
					test_mode_bitmap->pixel[i*bytes_per_row + centerx*bytes_per_pixel + 1] = (color >>  8) & 0xff;;
					test_mode_bitmap->pixel[i*bytes_per_row + centerx*bytes_per_pixel + 2] = color & 0xff;
                }
                else
                {
					JAB_REPORT_ERROR(("Drawing finder pattern %d out of image", k))
					break;
				}
            }

            jab_int32 startx = (jab_int32)(fps[k].center.x - fps[k].module_size/2 + 0.5f);
            jab_int32 endx = (jab_int32)(fps[k].center.x + fps[k].module_size/2 + 1 + 0.5f);
            for(jab_int32 i=startx; i<endx; i++)
            {
				if(centery*bytes_per_row + i*bytes_per_pixel + 2 < test_mode_bitmap->width * test_mode_bitmap->height * bytes_per_pixel)
				{
					test_mode_bitmap->pixel[centery*bytes_per_row + i*bytes_per_pixel] 	   = (color >> 16) & 0xff;
					test_mode_bitmap->pixel[centery*bytes_per_row + i*bytes_per_pixel + 1] = (color >>  8) & 0xff;;
					test_mode_bitmap->pixel[centery*bytes_per_row + i*bytes_per_pixel + 2] = color & 0xff;
                }
                else
                {
					JAB_REPORT_ERROR(("Drawing finder pattern %d out of image", k))
					break;
				}
            }
        }
    }
}
#endif

/**
 * @brief Remove the finder patterns with greatly different module size
 * @param fps the finder pattern list
 * @param fp_count the number of finder patterns in the list
 * @param mean the average module size
 * @param threshold the tolerance threshold
*/
void removeBadPatterns(jab_finder_pattern* fps, jab_int32 fp_count, jab_float mean, jab_float threshold)
{
    jab_int32 remove_count = 0;
    jab_int32 backup[fp_count];
    for(jab_int32 i=0; i<fp_count; i++)
    {
        if( fps[i].found_count < 2 || fabs(fps[i].module_size - mean) > threshold )
        {
            backup[i] = fps[i].found_count;
            fps[i].found_count = 0;
            remove_count++;
        }
    }
    //in case all finder patterns were removed, recover the one whose module size differs from the mean minimally
    if(remove_count == fp_count)
    {
        jab_float min = (threshold + mean)* 100;
        jab_int32 min_index = 0;
        for(jab_int32 i=0; i<fp_count; i++)
        {
            jab_float diff = (jab_float)fabs(fps[i].module_size - mean);
            if(diff < min)
            {
                min = diff;
                min_index = i;
            }
        }
        fps[min_index].found_count = backup[min_index];
        remove_count--;
    }
}

/**
 * @brief Find the finder pattern with most detected times
 * @param fps the finder pattern list
 * @param fp_count the number of finder patterns in the list
*/
jab_finder_pattern getBestPattern(jab_finder_pattern* fps, jab_int32 fp_count)
{
    jab_int32 counter = 0;
    jab_float total_module_size = 0;
    for(jab_int32 i=0; i<fp_count; i++)
    {
        if(fps[i].found_count > 0)
        {
            counter++;
            total_module_size += fps[i].module_size;
        }
    }
    jab_float mean = total_module_size / (jab_float)counter;

    jab_int32 max_found_count = 0;
    jab_float min_diff = 100;
    jab_int32 max_found_count_index = 0;
    for(jab_int32 i=0; i<fp_count; i++)
    {
        if(fps[i].found_count > 0)
        {
            if(fps[i].found_count > max_found_count)
            {
                max_found_count = fps[i].found_count;
                max_found_count_index = i;
                min_diff = (jab_float)fabs(fps[i].module_size - mean);
            }
            else if(fps[i].found_count == max_found_count)
            {
                if( fabs(fps[i].module_size - mean) < min_diff )
                {
                    max_found_count_index = i;
                    min_diff = (jab_float)fabs(fps[i].module_size - mean);
                }
            }
        }
    }
    jab_finder_pattern fp = fps[max_found_count_index];
    fps[max_found_count_index].found_count = 0;
    return fp;
}

/**
 * @brief Select the best finder patterns out of the list
 * @param fps the finder pattern list
 * @param fp_count the number of finder patterns in the list
 * @param fp_type_count the number of each finder pattern type
 * @return the number of missing finder pattern types
*/
jab_int32 selectBestPatterns(jab_finder_pattern* fps, jab_int32 fp_count, jab_int32* fp_type_count)
{
    //classify finder patterns into four types
    jab_finder_pattern fps0[fp_type_count[FP0]];
    jab_finder_pattern fps1[fp_type_count[FP1]];
    jab_finder_pattern fps2[fp_type_count[FP2]];
    jab_finder_pattern fps3[fp_type_count[FP3]];
    jab_int32 counter0 = 0, counter1 = 0, counter2 = 0, counter3 = 0;

    for(jab_int32 i=0; i<fp_count; i++)
    {
        //abandon the finder patterns which are founds less than 3 times,
        //which means a module shall not be smaller than 3 pixels.
        if(fps[i].found_count < 3) continue;
        switch (fps[i].type)
        {
            case FP0:
                fps0[counter0++] = fps[i];
                break;
            case FP1:
                fps1[counter1++] = fps[i];
                break;
            case FP2:
                fps2[counter2++] = fps[i];
                break;
            case FP3:
                fps3[counter3++] = fps[i];
                break;
        }
    }

	//set fp0
    if(counter0 > 1)
		fps[0] = getBestPattern(fps0, counter0);
	else if(counter0 == 1)
		fps[0] = fps0[0];
	else
		memset(&fps[0], 0, sizeof(jab_finder_pattern));
	//set fp1
    if(counter1 > 1)
		fps[1] = getBestPattern(fps1, counter1);
	else if(counter1 == 1)
		fps[1] = fps1[0];
	else
		memset(&fps[1], 0, sizeof(jab_finder_pattern));
	//set fp2
    if(counter2 > 1)
		fps[2] = getBestPattern(fps2, counter2);
	else if(counter2 == 1)
		fps[2] = fps2[0];
	else
		memset(&fps[2], 0, sizeof(jab_finder_pattern));
    //set fp3
    if(counter3 > 1)
		fps[3] = getBestPattern(fps3, counter3);
	else if(counter3 == 1)
		fps[3] = fps3[0];
	else
		memset(&fps[3], 0, sizeof(jab_finder_pattern));

    //if the found-count of a FP is smaller than the half of the max-found-count, abandon it
    jab_int32 max_found_count = 0;
    for(jab_int32 i=0; i<4; i++)
    {
        if(fps[i].found_count > max_found_count)
        {
            max_found_count = fps[i].found_count;
        }
    }
    for(jab_int32 i=0; i<4; i++)
    {
        if(fps[i].found_count > 0)
        {
            if(fps[i].found_count < (0.5*max_found_count))
                memset(&fps[i], 0, sizeof(jab_finder_pattern));
        }
    }

	//check how many finder patterns are not found
	jab_int32 missing_fp_count = 0;
	for(jab_int32 i=0; i<4; i++)
	{
		if(fps[i].found_count == 0)
			missing_fp_count++;
	}
	return missing_fp_count;
}

/**
 * @brief Scan the image vertically
 * @param ch the binarized color channels of the image
 * @param min_module_size the minimal module size
 * @param fps the found finder patterns
 * @param fp_type_count the number of found finder patterns for each type
 * @param total_finder_patterns the number of totally found finder patterns
*/
void scanPatternVertical(jab_bitmap* ch[], jab_int32 min_module_size, jab_finder_pattern* fps, jab_int32* fp_type_count, jab_int32* total_finder_patterns)
{
    jab_boolean done = 0;

    for(jab_int32 j=0; j<ch[0]->width && done == 0; j+=min_module_size)
    {
        jab_int32 starty = 0;
        jab_int32 endy = ch[0]->height;
        jab_int32 skip = 0;

        do
        {
        	jab_int32 type_r, type_g, type_b;
			jab_float centery_r, centery_g, centery_b;
			jab_float module_size_r, module_size_g, module_size_b;
			jab_boolean finder_pattern1_found = JAB_FAILURE;
			jab_boolean finder_pattern2_found = JAB_FAILURE;

            starty += skip;
            endy = ch[0]->height;
            //green channel
            if(seekPattern(ch[1], -1, j, &starty, &endy, &centery_g, &module_size_g, &skip))
            {
                type_g = ch[1]->pixel[(jab_int32)(centery_g)*ch[0]->width + j] > 0 ? 255 : 0;

                centery_r = centery_g;
                centery_b = centery_g;
                //check blue channel for Finder Pattern UL and LL
                if(crossCheckPatternVertical(ch[2], module_size_g*2, (jab_float)j, &centery_b, &module_size_b))
                {
                    type_b = ch[2]->pixel[(jab_int32)(centery_b)*ch[2]->width + j] > 0 ? 255 : 0;
                    //check red channel
                    module_size_r = module_size_g;
                    jab_int32 core_color_in_red_channel = jab_default_palette[FP3_CORE_COLOR * 3 + 0];
                    if(crossCheckColor(ch[0], core_color_in_red_channel, module_size_r, 5, j, (jab_int32)centery_r, 1))
                    {
                    	type_r = 0;
                    	finder_pattern1_found = JAB_SUCCESS;
                    }
                }
                //check red channel for Finder Pattern UR and LR
                else if(crossCheckPatternVertical(ch[0], module_size_g*2, (jab_float)j, &centery_r, &module_size_r))
				{
					type_r = ch[0]->pixel[(jab_int32)(centery_r)*ch[0]->width + j] > 0 ? 255 : 0;
					//check blue channel
					module_size_b = module_size_g;
					jab_int32 core_color_in_blue_channel = jab_default_palette[FP2_CORE_COLOR * 3 + 2];
                    if(crossCheckColor(ch[2], core_color_in_blue_channel, module_size_b, 5, j, (jab_int32)centery_b, 1))
                    {
                    	type_b = 0;
                    	finder_pattern2_found = JAB_SUCCESS;
                    }
				}
                //finder pattern candidate found
				if(finder_pattern1_found || finder_pattern2_found)
				{
					jab_finder_pattern fp;
					fp.center.x = (jab_float)j;
					fp.found_count = 1;
					if(finder_pattern1_found)
					{
						if(!checkModuleSize2(module_size_g, module_size_b)) continue;
						fp.center.y = (centery_g + centery_b) / 2.0f;
						fp.module_size = (module_size_g + module_size_b) / 2.0f;
						if( type_r == jab_default_palette[FP0_CORE_COLOR * 3] &&
							type_g == jab_default_palette[FP0_CORE_COLOR * 3 + 1] &&
							type_b == jab_default_palette[FP0_CORE_COLOR * 3 + 2])
						{
							fp.type = FP0;	//candidate for fp0
						}
						else if(type_r == jab_default_palette[FP3_CORE_COLOR * 3] &&
								type_g == jab_default_palette[FP3_CORE_COLOR * 3 + 1] &&
								type_b == jab_default_palette[FP3_CORE_COLOR * 3 + 2])
						{
							fp.type = FP3;	//candidate for fp3
						}
						else
						{
							continue;		//invalid type
						}
					}
					else if(finder_pattern2_found)
					{
						if(!checkModuleSize2(module_size_r, module_size_g)) continue;
						fp.center.y = (centery_r + centery_g) / 2.0f;
						fp.module_size = (module_size_r + module_size_g) / 2.0f;
						if(type_r == jab_default_palette[FP1_CORE_COLOR * 3] &&
						   type_g == jab_default_palette[FP1_CORE_COLOR * 3 + 1] &&
						   type_b == jab_default_palette[FP1_CORE_COLOR * 3 + 2])
						{
							fp.type = FP1;	//candidate for fp1
						}
						else if(type_r == jab_default_palette[FP2_CORE_COLOR * 3] &&
								type_g == jab_default_palette[FP2_CORE_COLOR * 3 + 1] &&
								type_b == jab_default_palette[FP2_CORE_COLOR * 3 + 2])
						{
							fp.type = FP2;	//candidate for fp2
						}
						else
						{
							continue;		//invalid type
						}
					}
					//cross check
					if( crossCheckPattern(ch, &fp, 1) )
					{
						saveFinderPattern(&fp, fps, total_finder_patterns, fp_type_count);
						if(*total_finder_patterns >= (MAX_FINDER_PATTERNS - 1) )
						{
							done = 1;
							break;
						}
					}
				}
            }
        }while(starty < ch[0]->height && endy < ch[0]->height);
    }
}

/**
 * @brief Search for the missing finder pattern at the estimated position
 * @param bitmap the image bitmap
 * @param fps the finder patterns
 * @param miss_fp_index the index of the missing finder pattern
*/
void seekMissingFinderPattern(jab_bitmap* bitmap, jab_finder_pattern* fps, jab_int32 miss_fp_index)
{
	//determine the search area
	jab_float radius = fps[miss_fp_index].module_size * 5;	//search radius
	jab_int32 start_x = (fps[miss_fp_index].center.x - radius) >= 0 ? (fps[miss_fp_index].center.x - radius) : 0;
	jab_int32 start_y = (fps[miss_fp_index].center.y - radius) >= 0 ? (fps[miss_fp_index].center.y - radius) : 0;
	jab_int32 end_x	  = (fps[miss_fp_index].center.x + radius) <= (bitmap->width - 1) ? (fps[miss_fp_index].center.x + radius) : (bitmap->width - 1);
	jab_int32 end_y   = (fps[miss_fp_index].center.y + radius) <= (bitmap->height- 1) ? (fps[miss_fp_index].center.y + radius) : (bitmap->height- 1);
	jab_int32 area_width = end_x - start_x;
	jab_int32 area_height= end_y - start_y;

	jab_bitmap* rgb[3];
	for(jab_int32 i=0; i<3; i++)
	{
		rgb[i] = (jab_bitmap*)calloc(1, sizeof(jab_bitmap) + area_height*area_width*sizeof(jab_byte));
		if(rgb[i] == NULL)
		{
			JAB_REPORT_INFO(("Memory allocation for binary bitmap failed, the missing finder pattern can not be found."))
			return;
		}
		rgb[i]->width = area_width;
		rgb[i]->height= area_height;
		rgb[i]->bits_per_channel = 8;
		rgb[i]->bits_per_pixel = 8;
		rgb[i]->channel_count = 1;
	}

	//calculate average pixel value
	jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
    jab_int32 bytes_per_row = bitmap->width * bytes_per_pixel;
	jab_float pixel_sum[3] = {0, 0, 0};
	jab_float pixel_ave[3];
	for(jab_int32 i=start_y; i<end_y; i++)
	{
		for(jab_int32 j=start_x; j<end_x; j++)
		{
			jab_int32 offset = i * bytes_per_row + j * bytes_per_pixel;
			pixel_sum[0] += bitmap->pixel[offset + 0];
			pixel_sum[1] += bitmap->pixel[offset + 1];
			pixel_sum[2] += bitmap->pixel[offset + 2];
		}
	}
	pixel_ave[0] = pixel_sum[0] / (jab_float)(area_width * area_height);
	pixel_ave[1] = pixel_sum[1] / (jab_float)(area_width * area_height);
	pixel_ave[2] = pixel_sum[2] / (jab_float)(area_width * area_height);

	//quantize the pixels inside the search area to black, cyan and yellow
	for(jab_int32 i=start_y, y=0; i<end_y; i++, y++)
	{
		for(jab_int32 j=start_x, x=0; j<end_x; j++, x++)
		{
			jab_int32 offset = i * bytes_per_row + j * bytes_per_pixel;
			//check black pixel
			if(bitmap->pixel[offset + 0] < pixel_ave[0] && bitmap->pixel[offset + 1] < pixel_ave[1] && bitmap->pixel[offset + 2] < pixel_ave[2])
			{
				rgb[0]->pixel[y*area_width + x] = 0;
				rgb[1]->pixel[y*area_width + x] = 0;
				rgb[2]->pixel[y*area_width + x] = 0;
			}
			else if(bitmap->pixel[offset + 0] < bitmap->pixel[offset + 2])	//R < B
			{
				rgb[0]->pixel[y*area_width + x] = 0;
				rgb[1]->pixel[y*area_width + x] = 255;
				rgb[2]->pixel[y*area_width + x] = 255;
			}
			else															//R > B
			{
				rgb[0]->pixel[y*area_width + x] = 255;
				rgb[1]->pixel[y*area_width + x] = 255;
				rgb[2]->pixel[y*area_width + x] = 0;
			}
		}
	}

	//set the core type of the expected finder pattern
	jab_int32 exp_type_r=0, exp_type_g=0, exp_type_b=0;
	switch(miss_fp_index)
	{
	case 0:
	case 1:
		exp_type_r = 0;
		exp_type_g = 0;
		exp_type_b = 0;
		break;
	case 2:
		exp_type_r = 255;
		exp_type_g = 255;
		exp_type_b = 0;
		break;
	case 3:
		exp_type_r = 0;
		exp_type_g = 255;
		exp_type_b = 255;
		break;
	}
	//search for the missing finder pattern
	jab_finder_pattern* fps_miss = (jab_finder_pattern*)calloc(MAX_FINDER_PATTERNS, sizeof(jab_finder_pattern));
    if(fps_miss == NULL)
    {
        reportError("Memory allocation for finder patterns failed, the missing finder pattern can not be found.");
        return;
    }
    jab_int32 total_finder_patterns = 0;
    jab_boolean done = 0;
    jab_int32 fp_type_count[4] = {0};

	for(jab_int32 i=0; i<area_height && done == 0; i++)
    {
        //get row
        jab_byte* row_r = rgb[0]->pixel + i*rgb[0]->width;
        jab_byte* row_g = rgb[1]->pixel + i*rgb[1]->width;
        jab_byte* row_b = rgb[2]->pixel + i*rgb[2]->width;

        jab_int32 startx = 0;
        jab_int32 endx = rgb[0]->width;
        jab_int32 skip = 0;

        do
        {
        	jab_finder_pattern fp;

        	jab_int32 type_r, type_g, type_b;
			jab_float centerx_r, centerx_g, centerx_b;
			jab_float module_size_r, module_size_g, module_size_b;
			jab_boolean finder_pattern_found = JAB_FAILURE;

            startx += skip;
            endx = rgb[0]->width;
            //green channel
            if(seekPatternHorizontal(row_g, &startx, &endx, &centerx_g, &module_size_g, &skip))
            {
                type_g = row_g[(jab_int32)(centerx_g)] > 0 ? 255 : 0;
                if(type_g != exp_type_g) continue;

                centerx_r = centerx_g;
                centerx_b = centerx_g;
                switch(miss_fp_index)
                {
				case 0:
				case 3:
					//check blue channel for Finder Pattern UL and LL
					if(crossCheckPatternHorizontal(rgb[2], module_size_g*2, &centerx_b, (jab_float)i, &module_size_b))
					{
						type_b = row_b[(jab_int32)(centerx_b)] > 0 ? 255 : 0;
						if(type_b != exp_type_b) continue;
						//check red channel
						module_size_r = module_size_g;
						jab_int32 core_color_in_red_channel = jab_default_palette[FP3_CORE_COLOR * 3 + 0];
						if(crossCheckColor(rgb[0], core_color_in_red_channel, module_size_r, 5, (jab_int32)centerx_r, i, 0))
						{
							type_r = 0;
							finder_pattern_found = JAB_SUCCESS;
						}
					}
					if(finder_pattern_found)
					{
						if(!checkModuleSize2(module_size_g, module_size_b)) continue;
						fp.center.x = (centerx_g + centerx_b) / 2.0f;
						fp.module_size = (module_size_g + module_size_b) / 2.0f;
					}
					break;
				case 1:
				case 2:
					//check red channel for Finder Pattern UR and LR
					if(crossCheckPatternHorizontal(rgb[0], module_size_g*2, &centerx_r, (jab_float)i, &module_size_r))
					{
						type_r = row_r[(jab_int32)(centerx_r)] > 0 ? 255 : 0;
						if(type_r != exp_type_r) continue;
						//check blue channel
						module_size_b = module_size_g;
						jab_int32 core_color_in_blue_channel = jab_default_palette[FP2_CORE_COLOR * 3 + 2];
						if(crossCheckColor(rgb[2], core_color_in_blue_channel, module_size_b, 5, (jab_int32)centerx_b, i, 0))
						{
							type_b = 0;
							finder_pattern_found = JAB_SUCCESS;
						}
					}
					if(finder_pattern_found)
					{
						if(!checkModuleSize2(module_size_r, module_size_g)) continue;
						fp.center.x = (centerx_r + centerx_g) / 2.0f;
						fp.module_size = (module_size_r + module_size_g) / 2.0f;
					}
					break;
                }
				//finder pattern candidate found
				if(finder_pattern_found)
				{
					fp.center.y = (jab_float)i;
					fp.found_count = 1;
					fp.type = miss_fp_index;
					//cross check
					if( crossCheckPattern(rgb, &fp, 0) )
					{
						//combine the finder patterns at the same position with the same size
						saveFinderPattern(&fp, fps_miss, &total_finder_patterns, fp_type_count);
						if(total_finder_patterns >= (MAX_FINDER_PATTERNS - 1) )
						{
							done = 1;
							break;
						}
					}
				}
            }
        }while(startx < rgb[0]->width && endx < rgb[0]->width);
    }
	//if the missing FP is found, get the best found
	if(total_finder_patterns > 0)
    {
        jab_int32 max_found = 0;
        jab_int32 max_index = 0;
        for(jab_int32 i=0; i<total_finder_patterns; i++)
        {
            if(fps_miss[i].found_count > max_found)
            {
                max_found = fps_miss[i].found_count;
                max_index = i;
            }
        }
        fps[miss_fp_index] = fps_miss[max_index];
        //recover the coordinates in bitmap
        fps[miss_fp_index].center.x += start_x;
        fps[miss_fp_index].center.y += start_y;
    }
}

/**
 * @brief Find the master symbol in the image
 * @param bitmap the image bitmap
 * @param ch the binarized color channels of the image
 * @param mode the detection mode
 * @param status the detection status
 * @return the finder pattern list | NULL
*/
jab_finder_pattern* findMasterSymbol(jab_bitmap* bitmap, jab_bitmap* ch[], jab_detect_mode mode, jab_int32* status)
{
    //suppose the code size is minimally 1/4 image size
    jab_int32 min_module_size = ch[0]->height / (2 * MAX_SYMBOL_ROWS * MAX_MODULES);
    if(min_module_size < 1 || mode == INTENSIVE_DETECT) min_module_size = 1;

    jab_finder_pattern* fps = (jab_finder_pattern*)calloc(MAX_FINDER_PATTERNS, sizeof(jab_finder_pattern));
    if(fps == NULL)
    {
        reportError("Memory allocation for finder patterns failed");
        *status = FATAL_ERROR;
        return NULL;
    }
    jab_int32 total_finder_patterns = 0;
    jab_boolean done = 0;
    jab_int32 fp_type_count[4] = {0};

    for(jab_int32 i=0; i<ch[0]->height && done == 0; i+=min_module_size)
    {
        //get row
        jab_byte* row_r = ch[0]->pixel + i*ch[0]->width;
        jab_byte* row_g = ch[1]->pixel + i*ch[1]->width;
        jab_byte* row_b = ch[2]->pixel + i*ch[2]->width;

        jab_int32 startx = 0;
        jab_int32 endx = ch[0]->width;
        jab_int32 skip = 0;

        do
        {
        	jab_int32 type_r, type_g, type_b;
			jab_float centerx_r, centerx_g, centerx_b;
			jab_float module_size_r, module_size_g, module_size_b;
			jab_boolean finder_pattern1_found = JAB_FAILURE;
			jab_boolean finder_pattern2_found = JAB_FAILURE;

            startx += skip;
            endx = ch[0]->width;
            //green channel
            if(seekPatternHorizontal(row_g, &startx, &endx, &centerx_g, &module_size_g, &skip))
            {
                type_g = row_g[(jab_int32)(centerx_g)] > 0 ? 255 : 0;

                centerx_r = centerx_g;
                centerx_b = centerx_g;
                //check blue channel for Finder Pattern UL and LL
                if(crossCheckPatternHorizontal(ch[2], module_size_g*2, &centerx_b, (jab_float)i, &module_size_b))
                {
                    type_b = row_b[(jab_int32)(centerx_b)] > 0 ? 255 : 0;
                    //check red channel
                    module_size_r = module_size_g;
                    jab_int32 core_color_in_red_channel = jab_default_palette[FP3_CORE_COLOR * 3 + 0];
                    if(crossCheckColor(ch[0], core_color_in_red_channel, module_size_r, 5, (jab_int32)centerx_r, i, 0))
                    {
                    	type_r = 0;
                    	finder_pattern1_found = JAB_SUCCESS;
                    }
                }
                //check red channel for Finder Pattern UR and LR
                else if(crossCheckPatternHorizontal(ch[0], module_size_g*2, &centerx_r, (jab_float)i, &module_size_r))
                {
                	type_r = row_r[(jab_int32)(centerx_r)] > 0 ? 255 : 0;
                	//check blue channel
                    module_size_b = module_size_g;
                    jab_int32 core_color_in_blue_channel = jab_default_palette[FP2_CORE_COLOR * 3 + 2];
                    if(crossCheckColor(ch[2], core_color_in_blue_channel, module_size_b, 5, (jab_int32)centerx_b, i, 0))
                    {
                    	type_b = 0;
                    	finder_pattern2_found = JAB_SUCCESS;
                    }
                }
				//finder pattern candidate found
				if(finder_pattern1_found || finder_pattern2_found)
				{
					jab_finder_pattern fp;
					fp.center.y = (jab_float)i;
					fp.found_count = 1;
					if(finder_pattern1_found)
					{
						if(!checkModuleSize2(module_size_g, module_size_b)) continue;
						fp.center.x = (centerx_g + centerx_b) / 2.0f;
						fp.module_size = (module_size_g + module_size_b) / 2.0f;
						if( type_r == jab_default_palette[FP0_CORE_COLOR * 3] &&
							type_g == jab_default_palette[FP0_CORE_COLOR * 3 + 1] &&
							type_b == jab_default_palette[FP0_CORE_COLOR * 3 + 2])
						{
							fp.type = FP0;	//candidate for fp0
						}
						else if(type_r == jab_default_palette[FP3_CORE_COLOR * 3] &&
								type_g == jab_default_palette[FP3_CORE_COLOR * 3 + 1] &&
								type_b == jab_default_palette[FP3_CORE_COLOR * 3 + 2])
						{
							fp.type = FP3;	//candidate for fp3
						}
						else
						{
							continue;		//invalid type
						}
					}
					else if(finder_pattern2_found)
					{
						if(!checkModuleSize2(module_size_r, module_size_g)) continue;
						fp.center.x = (centerx_r + centerx_g) / 2.0f;
						fp.module_size = (module_size_r + module_size_g) / 2.0f;
						if(type_r == jab_default_palette[FP1_CORE_COLOR * 3] &&
						   type_g == jab_default_palette[FP1_CORE_COLOR * 3 + 1] &&
						   type_b == jab_default_palette[FP1_CORE_COLOR * 3 + 2])
						{
							fp.type = FP1;	//candidate for fp1
						}
						else if(type_r == jab_default_palette[FP2_CORE_COLOR * 3] &&
								type_g == jab_default_palette[FP2_CORE_COLOR * 3 + 1] &&
								type_b == jab_default_palette[FP2_CORE_COLOR * 3 + 2])
						{
							fp.type = FP2;	//candidate for fp2
						}
						else
						{
							continue;		//invalid type
						}
					}
					//cross check
					if( crossCheckPattern(ch, &fp, 0) )
					{
						saveFinderPattern(&fp, fps, &total_finder_patterns, fp_type_count);
						if(total_finder_patterns >= (MAX_FINDER_PATTERNS - 1) )
						{
							done = 1;
							break;
						}
					}
				}
            }
        }while(startx < ch[0]->width && endx < ch[0]->width);
    }

    //if only FP0 and FP1 are found or only FP2 and FP3 are found, do vertical-scan
	if( (fp_type_count[0] != 0 && fp_type_count[1] !=0 && fp_type_count[2] == 0 && fp_type_count[3] == 0) ||
	    (fp_type_count[0] == 0 && fp_type_count[1] ==0 && fp_type_count[2] != 0 && fp_type_count[3] != 0) )
	{
		scanPatternVertical(ch, min_module_size, fps, fp_type_count, &total_finder_patterns);
		//set dir to 2?
	}


#if TEST_MODE
    //output all found finder patterns
    JAB_REPORT_INFO(("Total found: %d", total_finder_patterns))
    for(jab_int32 i=0; i<total_finder_patterns; i++)
    {
        JAB_REPORT_INFO(("x:%6.1f\ty:%6.1f\tsize:%.2f\tcnt:%d\ttype:%d\tdir:%d", fps[i].center.x, fps[i].center.y, fps[i].module_size, fps[i].found_count, fps[i].type, fps[i].direction))
    }
    drawFoundFinderPatterns(fps, total_finder_patterns, 0x00ff00);
    saveImage(test_mode_bitmap, "jab_detector_result_fp.png");
#endif

    //set finder patterns' direction
	for(jab_int32 i=0; i<total_finder_patterns; i++)
	{
		fps[i].direction = fps[i].direction >=0 ? 1 : -1;
	}
	//select best patterns
	jab_int32 missing_fp_count = selectBestPatterns(fps, total_finder_patterns, fp_type_count);

	//if more than one finder patterns are missing, detection fails
	if(missing_fp_count > 1)
	{
		reportError("Too few finder pattern found");
		*status = JAB_FAILURE;
		return fps;
	}

    //if only one finder pattern is missing, try anyway by estimating the missing one
    if(missing_fp_count == 1)
    {
        //estimate the missing finder pattern
        jab_int32 miss_fp = 0;
        if(fps[0].found_count == 0)
        {
			miss_fp = 0;
			jab_float ave_size_fp23 = (fps[2].module_size + fps[3].module_size) / 2.0f;
			jab_float ave_size_fp13 = (fps[1].module_size + fps[3].module_size) / 2.0f;
			fps[0].center.x = (fps[3].center.x - fps[2].center.x) / ave_size_fp23 * ave_size_fp13 + fps[1].center.x;
			fps[0].center.y = (fps[3].center.y - fps[2].center.y) / ave_size_fp23 * ave_size_fp13 + fps[1].center.y;
			fps[0].type = FP0;
			fps[0].found_count = 1;
			fps[0].direction = -fps[1].direction;
			fps[0].module_size = (fps[1].module_size + fps[2].module_size + fps[3].module_size) / 3.0f;
        }
        else if(fps[1].found_count == 0)
        {
			miss_fp = 1;
			jab_float ave_size_fp23 = (fps[2].module_size + fps[3].module_size) / 2.0f;
			jab_float ave_size_fp02 = (fps[0].module_size + fps[2].module_size) / 2.0f;
			fps[1].center.x = (fps[2].center.x - fps[3].center.x) / ave_size_fp23 * ave_size_fp02 + fps[0].center.x;
			fps[1].center.y = (fps[2].center.y - fps[3].center.y) / ave_size_fp23 * ave_size_fp02 + fps[0].center.y;
			fps[1].type = FP1;
			fps[1].found_count = 1;
			fps[1].direction = -fps[0].direction;
			fps[1].module_size = (fps[0].module_size + fps[2].module_size + fps[3].module_size) / 3.0f;
        }
        else if(fps[2].found_count == 0)
        {
			miss_fp = 2;
			jab_float ave_size_fp01 = (fps[0].module_size + fps[1].module_size) / 2.0f;
			jab_float ave_size_fp13 = (fps[1].module_size + fps[3].module_size) / 2.0f;
			fps[2].center.x = (fps[1].center.x - fps[0].center.x) / ave_size_fp01 * ave_size_fp13 + fps[3].center.x;
			fps[2].center.y = (fps[1].center.y - fps[0].center.y) / ave_size_fp01 * ave_size_fp13 + fps[3].center.y;
			fps[2].type = FP2;
			fps[2].found_count = 1;
			fps[2].direction = fps[3].direction;
			fps[2].module_size = (fps[0].module_size + fps[1].module_size + fps[3].module_size) / 3.0f;
        }
        else if(fps[3].found_count == 0)
        {
			miss_fp = 3;
			jab_float ave_size_fp01 = (fps[0].module_size + fps[1].module_size) / 2.0f;
			jab_float ave_size_fp02 = (fps[0].module_size + fps[2].module_size) / 2.0f;
			fps[3].center.x = (fps[0].center.x - fps[1].center.x) / ave_size_fp01 * ave_size_fp02 + fps[2].center.x;
			fps[3].center.y = (fps[0].center.y - fps[1].center.y) / ave_size_fp01 * ave_size_fp02 + fps[2].center.y;
			fps[3].type = FP3;
			fps[3].found_count = 1;
			fps[3].direction = fps[2].direction;
			fps[3].module_size = (fps[0].module_size + fps[1].module_size + fps[2].module_size) / 3.0f;
        }
        //check the position of the missed finder pattern
		if(fps[miss_fp].center.x < 0 || fps[miss_fp].center.x > ch[0]->width - 1 ||
		   fps[miss_fp].center.y < 0 || fps[miss_fp].center.y > ch[0]->height - 1)
		{
			JAB_REPORT_ERROR(("Finder pattern %d out of image", miss_fp))
			fps[miss_fp].found_count = 0;
			*status = JAB_FAILURE;
			return fps;
		}

		//try to find the missing finder pattern by a local search at the estimated position
#if TEST_MODE
		JAB_REPORT_INFO(("Trying to confirm the missing finder pattern by a local search"))
#endif
		seekMissingFinderPattern(bitmap, fps, miss_fp);
    }
#if TEST_MODE
    //output the final selected 4 patterns
    JAB_REPORT_INFO(("Final patterns:"))
    for(jab_int32 i=0; i<4; i++)
    {
        JAB_REPORT_INFO(("x:%6.1f\ty:%6.1f\tsize:%.2f\tcnt:%d\ttype:%d\tdir:%d", fps[i].center.x, fps[i].center.y, fps[i].module_size, fps[i].found_count, fps[i].type, fps[i].direction))
    }
	drawFoundFinderPatterns(fps, 4, 0xff0000);
	saveImage(test_mode_bitmap, "jab_detector_result_fp.png");
#endif
    *status = JAB_SUCCESS;
    return fps;
}

/**
 * @brief Crosscheck the alignment pattern candidate in diagonal direction
 * @param image the image bitmap
 * @param ap_type the alignment pattern type
 * @param module_size_max the maximal allowed module size
 * @param center the alignment pattern center
 * @param dir the alignment pattern direction
 * @return the y coordinate of the diagonal scanline center | -1 if failed
*/
jab_float crossCheckPatternDiagonalAP(jab_bitmap* image, jab_int32 ap_type, jab_int32 module_size_max, jab_point center, jab_int32* dir)
{
    jab_int32 offset_x, offset_y;
    jab_boolean fix_dir = 0;
    //if the direction is given, ONLY check the given direction
	if((*dir) != 0)
    {
		if((*dir) > 0)
		{
			offset_x = -1;
			offset_y = -1;
			*dir = 1;
		}
		else
		{
			offset_x = 1;
			offset_y = -1;
			*dir = -1;
		}
		fix_dir = 1;
    }
    else
	{
		//for ap0 and ap1, first check the diagonal at +45 degree
	    if(ap_type == AP0 || ap_type == AP1)
		{
			offset_x = -1;
			offset_y = -1;
			*dir = 1;
		}
		//for ap2 and ap3, first check the diagonal at -45 degree
		else
		{
			offset_x = 1;
			offset_y = -1;
			*dir = -1;
		}
	}

    jab_boolean flag = 0;
    jab_int32 try_count = 0;
    do
    {
    	flag = 0;
        try_count++;

        jab_int32 i, state_index;
        jab_int32 state_count[3] = {0};
        jab_int32 startx = (jab_int32)center.x;
        jab_int32 starty = (jab_int32)center.y;

        state_count[1]++;
        for(i=1, state_index=0; i<=starty && i<=startx && state_index<=1; i++)
        {
            if( image->pixel[(starty + i*offset_y)*image->width + (startx + i*offset_x)] == image->pixel[(starty + (i-1)*offset_y)*image->width + (startx + (i-1)*offset_x)] )
            {
                state_count[1 - state_index]++;
            }
            else
            {
                if(state_index >0 && state_count[1 - state_index] < 3)
                {
                    state_count[1 - (state_index-1)] += state_count[1 - state_index];
                    state_count[1 - state_index] = 0;
                    state_index--;
                    state_count[1 - state_index]++;
                }
                else
                {
                    state_index++;
                    if(state_index > 1) break;
                    else state_count[1 - state_index]++;
                }
            }
        }
        if(state_index < 1)
		{
			if(try_count == 1)
			{
				flag = 1;
				offset_x = -offset_x;
				*dir = -(*dir);
			}
			else
				return -1;
		}

		if(!flag)
		{
			for(i=1, state_index=0; (starty+i)<image->height && (startx+i)<image->width && state_index<=1; i++)
			{
				if( image->pixel[(starty - i*offset_y)*image->width + (startx - i*offset_x)] == image->pixel[(starty - (i-1)*offset_y)*image->width + (startx - (i-1)*offset_x)] )
				{
					state_count[1 + state_index]++;
				}
				else
				{
					if(state_index >0 && state_count[1 + state_index] < 3)
					{
						state_count[1 + (state_index-1)] += state_count[1 + state_index];
						state_count[1 + state_index] = 0;
						state_index--;
						state_count[1 + state_index]++;
					}
					else
					{
						state_index++;
						if(state_index > 1) break;
						else state_count[1 + state_index]++;
					}
				}
			}
			if(state_index < 1)
			{
				if(try_count == 1)
				{
					flag = 1;
					offset_x = -offset_x;
					*dir = -(*dir);
				}
				else
					return -1;
			}
		}

		if(!flag)
		{
			//check module size, if it is too big, assume it is a false positive
			if(state_count[1] < module_size_max && state_count[0] > 0.5 * state_count[1] && state_count[2] > 0.5 * state_count[1])
			{
				jab_float new_centery = (jab_float)(starty + i - state_count[2]) - (jab_float)state_count[1] / 2.0f;
				return new_centery;
			}
			else
			{
				flag = 1;
				offset_x = - offset_x;
				*dir = -(*dir);
			}
		}
    }
    while(flag && try_count < 2 && !fix_dir);

    return -1;
}

/**
 * @brief Crosscheck the alignment pattern candidate in vertical direction
 * @param image the image bitmap
 * @param center the alignment pattern center
 * @param module_size_max the maximal allowed module size
 * @param module_size the module size in vertical direction
 * @return the y coordinate of the vertical scanline center | -1 if failed
*/
jab_float crossCheckPatternVerticalAP(jab_bitmap* image, jab_point center, jab_int32 module_size_max, jab_float* module_size)
{
    jab_int32 state_count[3] = {0};
    jab_int32 centerx = (jab_int32)center.x;
	jab_int32 centery = (jab_int32)center.y;
    jab_int32 i, state_index;

    state_count[1]++;
    for(i=1, state_index=0; i<=centery && state_index<=1; i++)
    {
        if( image->pixel[(centery-i)*image->width + centerx] == image->pixel[(centery-(i-1))*image->width + centerx] )
        {
            state_count[1 - state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[1 - state_index] < 3)
            {
                state_count[1 - (state_index-1)] += state_count[1 - state_index];
                state_count[1 - state_index] = 0;
                state_index--;
                state_count[1 - state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > 1) break;
                else state_count[1 - state_index]++;
            }
        }
    }
    if(state_index < 1)
        return -1;

    for(i=1, state_index=0; (centery+i)<image->height && state_index<=1; i++)
    {
        if( image->pixel[(centery+i)*image->width + centerx] == image->pixel[(centery+(i-1))*image->width + centerx] )
        {
            state_count[1 + state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[1 + state_index] < 3)
            {
                state_count[1 + (state_index-1)] += state_count[1 + state_index];
                state_count[1 + state_index] = 0;
                state_index--;
                state_count[1 + state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > 1) break;
                else state_count[1 + state_index]++;
            }
        }
    }
    if(state_index < 1)
        return -1;

    //check module size, if it is too big, assume it is a false positive
    if(state_count[1] < module_size_max && state_count[0] > 0.5 * state_count[1] && state_count[2] > 0.5 * state_count[1])
    {
        *module_size = state_count[1];
        jab_float new_centery = (jab_float)(centery + i - state_count[2]) - (jab_float)state_count[1] / 2.0f;
        return new_centery;
    }
    return -1;
}

/**
 * @brief Crosscheck the alignment pattern candidate in horizontal direction
 * @param row the bitmap row
 * @param channel the color channel
 * @param startx the start position
 * @param endx the end position
 * @param centerx the center of the candidate scanline
 * @param ap_type the alignment pattern type
 * @param module_size_max the maximal allowed module size
 * @param module_size the module size in horizontal direction
 * @return the x coordinate of the horizontal scanline center | -1 if failed
*/
jab_float crossCheckPatternHorizontalAP(jab_byte* row, jab_int32 channel, jab_int32 startx, jab_int32 endx, jab_int32 centerx, jab_int32 ap_type, jab_float module_size_max, jab_float* module_size)
{
    jab_int32 core_color = -1;
    switch(ap_type)
    {
        case AP0:
			core_color = jab_default_palette[AP0_CORE_COLOR * 3 + channel];
			break;
        case AP1:
			core_color = jab_default_palette[AP1_CORE_COLOR * 3 + channel];
			break;
        case AP2:
			core_color = jab_default_palette[AP2_CORE_COLOR * 3 + channel];
            break;
        case AP3:
			core_color = jab_default_palette[AP3_CORE_COLOR * 3 + channel];
			break;
		case APX:
			core_color = jab_default_palette[APX_CORE_COLOR * 3 + channel];
			break;
    }
    if(row[centerx] != core_color)
        return -1;

    jab_int32 state_count[3] = {0};
    jab_int32 i, state_index;

    state_count[1]++;
    for(i=1, state_index=0; (centerx-i)>=startx && state_index<=1; i++)
    {
        if( row[centerx - i] == row[centerx - (i-1)] )
        {
            state_count[1 - state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[1 - state_index] < 3)
            {
                state_count[1 - (state_index-1)] += state_count[1 - state_index];
                state_count[1 - state_index] = 0;
                state_index--;
                state_count[1 - state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > 1) break;
                else state_count[1 - state_index]++;
            }
        }
    }
    if(state_index < 1)
        return -1;

    for(i=1, state_index=0; (centerx+i)<=endx && state_index<=1; i++)
    {
        if( row[centerx + i] == row[centerx + (i-1)] )
        {
            state_count[1 + state_index]++;
        }
        else
        {
            if(state_index > 0 && state_count[1 + state_index] < 3)
            {
                state_count[1 + (state_index-1)] += state_count[1 + state_index];
                state_count[1 + state_index] = 0;
                state_index--;
                state_count[1 + state_index]++;
            }
            else
            {
                state_index++;
                if(state_index > 1) break;
                else state_count[1 + state_index]++;
            }
        }
    }
    if(state_index < 1)
        return -1;

    //check module size, if it is too big, assume it is a false positive
    if(state_count[1] < module_size_max && state_count[0] > 0.5 * state_count[1] && state_count[2] > 0.5 * state_count[1])
    {
        *module_size = state_count[1];
        jab_float new_centerx = (jab_float)(centerx + i - state_count[2]) - (jab_float)state_count[1] / 2.0f;
        return new_centerx;
    }
    return -1;
}

/**
 * @brief Crosscheck the alignment pattern
 * @param ch the binarized color channels of the image
 * @param y the y coordinate of the horizontal scanline
 * @param minx the minimal coordinate of the horizontal scanline
 * @param maxx the maximal coordinate of the horizontal scanline
 * @param cur_x the start position of the horizontal scanline
 * @param ap_type the alignment pattern type
 * @param max_module_size the maximal allowed module size
 * @param centerx the x coordinate of the alignment pattern center
 * @param centery the y coordinate of the alignment pattern center
 * @param module_size the module size
 * @param dir the alignment pattern direction
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean crossCheckPatternAP(jab_bitmap* ch[], jab_int32 y, jab_int32 minx, jab_int32 maxx, jab_int32 cur_x, jab_int32 ap_type, jab_float max_module_size, jab_float* centerx, jab_float* centery, jab_float* module_size, jab_int32* dir)
{
	//get row
	jab_byte* row_r = ch[0]->pixel + y*ch[0]->width;
	jab_byte* row_b = ch[2]->pixel + y*ch[2]->width;

	jab_float l_centerx[3] = {0.0f};
	jab_float l_centery[3] = {0.0f};
	jab_float l_module_size_h[3] = {0.0f};
	jab_float l_module_size_v[3] = {0.0f};

	//check r channel horizontally
	l_centerx[0] = crossCheckPatternHorizontalAP(row_r, 0, minx, maxx, cur_x, ap_type, max_module_size, &l_module_size_h[0]);
	if(l_centerx[0] < 0) return JAB_FAILURE;
	//check b channel horizontally
	l_centerx[2] = crossCheckPatternHorizontalAP(row_b, 2, minx, maxx, (jab_int32)l_centerx[0], ap_type, max_module_size, &l_module_size_h[2]);
	if(l_centerx[2] < 0) return JAB_FAILURE;
	//calculate the center and the module size
	jab_point center;
	center.x = (l_centerx[0] + l_centerx[2]) / 2.0f;
	center.y = y;
	(*module_size) = (l_module_size_h[0] + l_module_size_h[2]) / 2.0f;
	//check g channel horizontally
	jab_int32 core_color_in_green_channel = -1;
    switch(ap_type)
    {
        case AP0:
			core_color_in_green_channel = jab_default_palette[AP0_CORE_COLOR * 3 + 1];
			break;
        case AP1:
			core_color_in_green_channel = jab_default_palette[AP1_CORE_COLOR * 3 + 1];
			break;
        case AP2:
			core_color_in_green_channel = jab_default_palette[AP2_CORE_COLOR * 3 + 1];
            break;
        case AP3:
			core_color_in_green_channel = jab_default_palette[AP3_CORE_COLOR * 3 + 1];
			break;
		case APX:
			core_color_in_green_channel = jab_default_palette[APX_CORE_COLOR * 3 + 1];
			break;
    }
	if(!crossCheckColor(ch[1], core_color_in_green_channel, (jab_int32)(*module_size), 3, (jab_int32)center.x, (jab_int32)center.y, 0))
		return JAB_FAILURE;

	//check r channel vertically
	l_centery[0] = crossCheckPatternVerticalAP(ch[0], center, max_module_size, &l_module_size_v[0]);
	if(l_centery[0] < 0) return JAB_FAILURE;
	//again horizontally
	row_r = ch[0]->pixel + ((jab_int32)l_centery[0])*ch[0]->width;
	l_centerx[0] = crossCheckPatternHorizontalAP(row_r, 0, minx, maxx, center.x, ap_type, max_module_size, &l_module_size_h[0]);
	if(l_centerx[0] < 0) return JAB_FAILURE;

	//check b channel vertically
	l_centery[2] = crossCheckPatternVerticalAP(ch[2], center, max_module_size, &l_module_size_v[2]);
	if(l_centery[2] < 0) return JAB_FAILURE;
	//again horizontally
	row_b = ch[2]->pixel + ((jab_int32)l_centery[2])*ch[2]->width;
	l_centerx[2] = crossCheckPatternHorizontalAP(row_b, 2, minx, maxx, center.x, ap_type, max_module_size, &l_module_size_h[2]);
	if(l_centerx[2] < 0) return JAB_FAILURE;

	//update the center and the module size
	(*module_size) = (l_module_size_h[0] + l_module_size_h[2] + l_module_size_v[0] + l_module_size_v[2]) / 4.0f;
	(*centerx) = (l_centerx[0] + l_centerx[2]) / 2.0f;
	(*centery) = (l_centery[0] + l_centery[2]) / 2.0f;
	center.x = (*centerx);
	center.y = (*centery);

	//check g channel vertically
	if(!crossCheckColor(ch[1], core_color_in_green_channel, (jab_int32)(*module_size), 3, (jab_int32)center.x, (jab_int32)center.y, 1))
		return JAB_FAILURE;

	//diagonal check
	jab_int32 l_dir[3] = {0};
	if(crossCheckPatternDiagonalAP(ch[0], ap_type, (*module_size)*2, center, &l_dir[0]) < 0) return JAB_FAILURE;
	if(crossCheckPatternDiagonalAP(ch[2], ap_type, (*module_size)*2, center, &l_dir[2]) < 0) return JAB_FAILURE;
	if(!crossCheckColor(ch[1], core_color_in_green_channel, (jab_int32)(*module_size), 3, (jab_int32)center.x, (jab_int32)center.y, 2))
		return JAB_FAILURE;
	(*dir) = (l_dir[0] + l_dir[2]) > 0 ? 1 : -1;

	return JAB_SUCCESS;
}

/**
 * @brief Find alignment pattern around a given position
 * @param ch the binarized color channels of the image
 * @param x the x coordinate of the given position
 * @param y the y coordinate of the given position
 * @param module_size the module size
 * @param ap_type the alignment pattern type
 * @return the found alignment pattern
*/
jab_alignment_pattern findAlignmentPattern(jab_bitmap* ch[], jab_float x, jab_float y, jab_float module_size, jab_int32 ap_type)
{
    jab_alignment_pattern ap;
    ap.type = -1;
    ap.found_count = 0;

    //determine the core color of r channel
	jab_int32 core_color_r = -1;
	switch(ap_type)
	{
		case AP0:
			core_color_r = jab_default_palette[AP0_CORE_COLOR * 3];
			break;
		case AP1:
			core_color_r = jab_default_palette[AP1_CORE_COLOR * 3];
			break;
		case AP2:
			core_color_r = jab_default_palette[AP2_CORE_COLOR * 3];
			break;
		case AP3:
			core_color_r = jab_default_palette[AP3_CORE_COLOR * 3];
			break;
		case APX:
			core_color_r = jab_default_palette[APX_CORE_COLOR * 3];
			break;
	}

    //define search range
    jab_int32 radius = (jab_int32)(4 * module_size);
    jab_int32 radius_max = 4 * radius;

    for(; radius<radius_max; radius<<=1)
    {
        jab_alignment_pattern* aps = (jab_alignment_pattern*)calloc(MAX_FINDER_PATTERNS, sizeof(jab_alignment_pattern));
        if(aps == NULL)
        {
            reportError("Memory allocation for alignment patterns failed");
            return ap;
        }

        jab_int32 startx = (jab_int32)MAX(0, x - radius);
        jab_int32 starty = (jab_int32)MAX(0, y - radius);
        jab_int32 endx = (jab_int32)MIN(ch[0]->width - 1, x + radius);
        jab_int32 endy = (jab_int32)MIN(ch[0]->height - 1, y + radius);
        if(endx - startx < 3 * module_size || endy - starty < 3 * module_size) continue;

        jab_int32 counter = 0;
        for(jab_int32 k=starty; k<endy; k++)
        {
            //search from middle outwards
            jab_int32 kk = k - starty;
            jab_int32 i = y + ((kk & 0x01) == 0 ? (kk + 1) / 2 : -((kk + 1) / 2));
            if(i < starty)
				continue;
			else if(i > endy)
				continue;

            //get r channel row
            jab_byte* row_r = ch[0]->pixel + i*ch[0]->width;

            jab_float ap_module_size, centerx, centery;
            jab_int32 ap_dir;

            jab_boolean ap_found = 0;
			jab_int32 dir = -1;
			jab_int32 left_tmpx = x;
			jab_int32 right_tmpx = x;
			while((left_tmpx > startx || right_tmpx < endx) && !ap_found)
			{
				if(dir < 0)	//go to left
				{
					while(row_r[left_tmpx] != core_color_r && left_tmpx > startx)
					{
						left_tmpx--;
					}
					if(left_tmpx <= startx)
					{
						dir = -dir;
						continue;
					}
					ap_found = crossCheckPatternAP(ch, i, startx, endx, left_tmpx, ap_type, module_size*2, &centerx, &centery, &ap_module_size, &ap_dir);
					while(row_r[left_tmpx] == core_color_r && left_tmpx > startx)
					{
						left_tmpx--;
					}
					dir = -dir;
				}
				else //go to right
				{
					while(row_r[right_tmpx] == core_color_r && right_tmpx < endx)
					{
						right_tmpx++;
					}
					while(row_r[right_tmpx] != core_color_r && right_tmpx < endx)
					{
						right_tmpx++;
					}
					if(right_tmpx >= endx)
					{
						dir = -dir;
						continue;
					}
					ap_found = crossCheckPatternAP(ch, i, startx, endx, right_tmpx, ap_type, module_size*2, &centerx, &centery, &ap_module_size, &ap_dir);
					while(row_r[right_tmpx] == core_color_r && right_tmpx < endx)
					{
						right_tmpx++;
					}
					dir = -dir;
				}
			}

            if(!ap_found) continue;

            ap.center.x = centerx;
            ap.center.y = centery;
            ap.module_size = ap_module_size;
            ap.direction = ap_dir;
            ap.type = ap_type;
            ap.found_count = 1;

            jab_int32 index = saveAlignmentPattern(&ap, aps, &counter);
            if(index >= 0) //if found twice, done!
            {
                ap = aps[index];
                free(aps);
                return ap;
            }
        }
        free(aps);
    }
    ap.type = -1;
    ap.found_count = 0;
    return ap;
}

/**
 * @brief Find a docked slave symbol
 * @param bitmap the image bitmap
 * @param ch the binarized color channels of the image
 * @param host_symbol the host symbol
 * @param slave_symbol the slave symbol
 * @param docked_position the docked position
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean findSlaveSymbol(jab_bitmap* bitmap, jab_bitmap* ch[], jab_decoded_symbol* host_symbol, jab_decoded_symbol* slave_symbol, jab_int32 docked_position)
{
    jab_alignment_pattern* aps = (jab_alignment_pattern*)calloc(4, sizeof(jab_alignment_pattern));
    if(aps == NULL)
    {
        reportError("Memory allocation for alignment patterns failed");
        return JAB_FAILURE;
    }

    //get slave symbol side size from its metadata
    slave_symbol->side_size.x = VERSION2SIZE(slave_symbol->metadata.side_version.x);
    slave_symbol->side_size.y = VERSION2SIZE(slave_symbol->metadata.side_version.y);

    //docked horizontally
    jab_float distx01 = host_symbol->pattern_positions[1].x - host_symbol->pattern_positions[0].x;
    jab_float disty01 = host_symbol->pattern_positions[1].y - host_symbol->pattern_positions[0].y;
    jab_float distx32 = host_symbol->pattern_positions[2].x - host_symbol->pattern_positions[3].x;
    jab_float disty32 = host_symbol->pattern_positions[2].y - host_symbol->pattern_positions[3].y;
    //docked vertically
    jab_float distx03 = host_symbol->pattern_positions[3].x - host_symbol->pattern_positions[0].x;
    jab_float disty03 = host_symbol->pattern_positions[3].y - host_symbol->pattern_positions[0].y;
    jab_float distx12 = host_symbol->pattern_positions[2].x - host_symbol->pattern_positions[1].x;
    jab_float disty12 = host_symbol->pattern_positions[2].y - host_symbol->pattern_positions[1].y;

    jab_float alpha1 = 0.0f, alpha2 = 0.0f;
    jab_int32 sign = 1;
    jab_int32 docked_side_size = 0, undocked_side_size;
    jab_int32 ap1 = 0, ap2 = 0, ap3 = 0, ap4 = 0, hp1 = 0, hp2 = 0;
    switch(docked_position)
    {
        case 3:
            /*
                fp[0] ... fp[1] .. ap[0] ... ap[1]
                  .         .         .         .
                  .  master .         .  slave  .
                  .         .         .         .
                fp[3] ... fp[2] .. ap[3] ... ap[2]
            */
            alpha1 = atan2(disty01, distx01);
            alpha2 = atan2(disty32, distx32);
            sign = 1;
            docked_side_size   = slave_symbol->side_size.y;
            undocked_side_size = slave_symbol->side_size.x;
            ap1 = AP0;	//ap[0]
            ap2 = AP3;	//ap[3]
            ap3 = AP1;	//ap[1]
            ap4 = AP2;	//ap[2]
            hp1 = FP1;	//fp[1] or ap[1] of the host
            hp2 = FP2;	//fp[2] or ap[2] of the host
            slave_symbol->host_position = 2;
            break;
        case 2:
            /*
                ap[0] ... ap[1] .. fp[0] ... fp[1]
                  .         .        .         .
                  .  slave  .        .  master .
                  .         .        .         .
                ap[3] ... ap[2] .. fp[3] ... fp[2]
            */
            alpha1 = atan2(disty32, distx32);
            alpha2 = atan2(disty01, distx01);
            sign = -1;
            docked_side_size   = slave_symbol->side_size.y;
            undocked_side_size = slave_symbol->side_size.x;
            ap1 = AP2;	//ap[2]
            ap2 = AP1;	//ap[1]
            ap3 = AP3;	//ap[3]
            ap4 = AP0;	//ap[0]
            hp1 = FP3;	//fp[3] or ap[3] of the host
            hp2 = FP0;	//fp[0] or ap[0] of the host
            slave_symbol->host_position = 3;
            break;
        case 1:
            /*
                fp[0] ... fp[1]
                  .         .
                  .  master .
                  .         .
                fp[3] ... fp[2]
                  .			.
                  . 		.
                ap[0] ... ap[1]
                  .         .
                  .  slave  .
                  .         .
                ap[3] ... ap[2]
            */
            alpha1 = atan2(disty12, distx12);
            alpha2 = atan2(disty03, distx03);
            sign = 1;
            docked_side_size   = slave_symbol->side_size.x;
            undocked_side_size = slave_symbol->side_size.y;
            ap1 = AP1;	//ap[1]
            ap2 = AP0;	//ap[0]
            ap3 = AP2;	//ap[2]
            ap4 = AP3;	//ap[3]
            hp1 = FP2;	//fp[2] or ap[2] of the host
            hp2 = FP3;	//fp[3] or ap[3] of the host
            slave_symbol->host_position = 0;
            break;
        case 0:
            /*
                ap[0] ... ap[1]
                  .         .
                  .  slave  .
                  .         .
                ap[3] ... ap[2]
                  .			.
                  . 		.
                fp[0] ... fp[1]
                  .         .
                  .  master .
                  .         .
                fp[3] ... fp[2]
            */
            alpha1 = atan2(disty03, distx03);
            alpha2 = atan2(disty12, distx12);
            sign = -1;
            docked_side_size   = slave_symbol->side_size.x;
            undocked_side_size = slave_symbol->side_size.y;
            ap1 = AP3;	//ap[3]
            ap2 = AP2;	//ap[2]
            ap3 = AP0;	//ap[0]
            ap4 = AP1;	//ap[1]
            hp1 = FP0;	//fp[0] or ap[0] of the host
            hp2 = FP1;	//fp[1] or ap[1] of the host
            slave_symbol->host_position = 1;
            break;
    }

    //calculate the coordinate of ap1
    aps[ap1].center.x = host_symbol->pattern_positions[hp1].x + sign * 7 * host_symbol->module_size * cos(alpha1);
    aps[ap1].center.y = host_symbol->pattern_positions[hp1].y + sign * 7 * host_symbol->module_size * sin(alpha1);
    //find the alignment pattern around ap1
    aps[ap1] = findAlignmentPattern(ch, aps[ap1].center.x, aps[ap1].center.y, host_symbol->module_size, ap1);
    if(aps[ap1].found_count == 0)
    {
        JAB_REPORT_ERROR(("The first alignment pattern in slave symbol %d not found", slave_symbol->index))
        return JAB_FAILURE;
    }
    //calculate the coordinate of ap2
    aps[ap2].center.x = host_symbol->pattern_positions[hp2].x + sign * 7 * host_symbol->module_size * cos(alpha2);
    aps[ap2].center.y = host_symbol->pattern_positions[hp2].y + sign * 7 * host_symbol->module_size * sin(alpha2);
    //find alignment pattern around ap2
    aps[ap2] = findAlignmentPattern(ch, aps[ap2].center.x, aps[ap2].center.y, host_symbol->module_size, ap2);
    if(aps[ap2].found_count == 0)
    {
        JAB_REPORT_ERROR(("The second alignment pattern in slave symbol %d not found", slave_symbol->index))
        return JAB_FAILURE;
    }

    //estimate the module size in the slave symbol
    slave_symbol->module_size = DIST(aps[ap1].center.x, aps[ap1].center.y, aps[ap2].center.x, aps[ap2].center.y) / (docked_side_size - 7);

    //calculate the coordinate of ap3
    aps[ap3].center.x = aps[ap1].center.x + sign * (undocked_side_size - 7) * slave_symbol->module_size * cos(alpha1);
    aps[ap3].center.y = aps[ap1].center.y + sign * (undocked_side_size - 7) * slave_symbol->module_size * sin(alpha1);
    //find alignment pattern around ap3
    aps[ap3] = findAlignmentPattern(ch, aps[ap3].center.x, aps[ap3].center.y, slave_symbol->module_size, ap3);
    //calculate the coordinate of ap4
    aps[ap4].center.x = aps[ap2].center.x + sign * (undocked_side_size - 7) * slave_symbol->module_size * cos(alpha2);
    aps[ap4].center.y = aps[ap2].center.y + sign * (undocked_side_size - 7) * slave_symbol->module_size * sin(alpha2);
    //find alignment pattern around ap4
    aps[ap4] = findAlignmentPattern(ch, aps[ap4].center.x, aps[ap4].center.y, slave_symbol->module_size, ap4);

    //if neither ap3 nor ap4 is found, failed
    if(aps[ap3].found_count == 0 && aps[ap4].found_count == 0)
    {
        free(aps);
        return JAB_FAILURE;
    }
    //if only 3 aps are found, try anyway by estimating the coordinate of the fourth one
    if(aps[ap3].found_count == 0)
    {
    	jab_float ave_size_ap24 = (aps[ap2].module_size + aps[ap4].module_size) / 2.0f;
		jab_float ave_size_ap14 = (aps[ap1].module_size + aps[ap4].module_size) / 2.0f;
        aps[ap3].center.x = (aps[ap4].center.x - aps[ap2].center.x) / ave_size_ap24 * ave_size_ap14 + aps[ap1].center.x;
        aps[ap3].center.y = (aps[ap4].center.y - aps[ap2].center.y) / ave_size_ap24 * ave_size_ap14 + aps[ap1].center.y;
        aps[ap3].type = ap3;
        aps[ap3].found_count = 1;
        aps[ap3].module_size = (aps[ap1].module_size + aps[ap2].module_size + aps[ap4].module_size) / 3.0f;
        if(aps[ap3].center.x > bitmap->width - 1 || aps[ap3].center.y > bitmap->height - 1)
        {
			JAB_REPORT_ERROR(("Alignment pattern %d out of image", ap3))
			free(aps);
			return JAB_FAILURE;
        }
    }
    if(aps[ap4].found_count == 0)
    {
    	jab_float ave_size_ap13 = (aps[ap1].module_size + aps[ap3].module_size) / 2.0f;
		jab_float ave_size_ap23 = (aps[ap2].module_size + aps[ap3].module_size) / 2.0f;
        aps[ap4].center.x = (aps[ap3].center.x - aps[ap1].center.x) / ave_size_ap13 * ave_size_ap23 + aps[ap2].center.x;
        aps[ap4].center.y = (aps[ap3].center.y - aps[ap1].center.y) / ave_size_ap13 * ave_size_ap23 + aps[ap2].center.y;
        aps[ap4].type = ap4;
        aps[ap4].found_count = 1;
        aps[ap4].module_size = (aps[ap1].module_size + aps[ap1].module_size + aps[ap3].module_size) / 3.0f;
        if(aps[ap4].center.x > bitmap->width - 1 || aps[ap4].center.y > bitmap->height - 1)
        {
			JAB_REPORT_ERROR(("Alignment pattern %d out of image", ap4))
			free(aps);
			return JAB_FAILURE;
        }
    }

    //save the coordinates of aps into the slave symbol
    slave_symbol->pattern_positions[ap1] = aps[ap1].center;
    slave_symbol->pattern_positions[ap2] = aps[ap2].center;
    slave_symbol->pattern_positions[ap3] = aps[ap3].center;
    slave_symbol->pattern_positions[ap4] = aps[ap4].center;
    slave_symbol->module_size = (aps[ap1].module_size + aps[ap2].module_size + aps[ap3].module_size + aps[ap4].module_size) / 4.0f;

#if TEST_MODE
	JAB_REPORT_INFO(("Found alignment patterns in slave symbol %d:", slave_symbol->index))
    for(jab_int32 i=0; i<4; i++)
    {
        JAB_REPORT_INFO(("x: %6.1f\ty: %6.1f\tcount: %d\ttype: %d\tsize: %.2f", aps[i].center.x, aps[i].center.y, aps[i].found_count, aps[i].type, aps[i].module_size))
    }
    drawFoundFinderPatterns((jab_finder_pattern*)aps, 4, 0xff0000);
	saveImage(test_mode_bitmap, "jab_detector_result_slave.png");
#endif

    free(aps);
    return JAB_SUCCESS;
}

/**
 * @brief Get the nearest valid side size to a given size
 * @param size the input size
 * @param flag the flag indicating the magnitude of error
 * @return the nearest valid side size | -1: invalid side size
*/
jab_int32 getSideSize(jab_int32 size, jab_int32* flag)
{
	*flag = 1;
    switch (size & 0x03) //mod 4
    {
    case 0:
        size++;
        break;
    case 2:
        size--;
        break;
    case 3:
        size += 2;	//error is bigger than 1, guess the next version and try anyway
        *flag = 0;
        break;
    }
    if(size < 21)
	{
		size = -1;
		*flag = -1;
	}
	else if(size > 145)
	{
		size = -1;
		*flag = -1;
	}
    return size;
}

/**
 * @brief Choose the side size according to the detection reliability
 * @param size1 the first side size
 * @param flag1 the detection flag of the first size
 * @param size2 the second side size
 * @param flag2 the detection flag of the second size
 * @return the chosen side size
*/
jab_int32 chooseSideSize(jab_int32 size1, jab_int32 flag1, jab_int32 size2, jab_int32 flag2)
{
	if(flag1 == -1 && flag2 == -1)
	{
		return -1;
	}
    else if(flag1 == flag2)
		return MAX(size1, size2);
	else
	{
		if(flag1 > flag2)
			return size1;
		else
			return size2;
	}
}

/**
 * @brief Calculate the number of modules between two finder/alignment patterns
 * @param fp1 the first finder/alignment pattern
 * @param fp2 the second finder/alignment pattern
 * @return the number of modules
*/
jab_int32 calculateModuleNumber(jab_finder_pattern fp1, jab_finder_pattern fp2)
{
    jab_float dist = DIST(fp1.center.x, fp1.center.y, fp2.center.x, fp2.center.y);
    //the angle between the scanline and the connection between finder/alignment patterns
	jab_float cos_theta = MAX(fabs(fp2.center.x - fp1.center.x), fabs(fp2.center.y - fp1.center.y)) / dist;
    jab_float mean = (fp1.module_size + fp2.module_size)*cos_theta / 2.0f;
    jab_int32 number = (jab_int32)(dist / mean + 0.5f);
    return number;
}

/**
 * @brief Calculate the side sizes of master symbol
 * @param fps the finder patterns
 * @return the horizontal and vertical side sizes
*/
jab_vector2d calculateSideSize(jab_finder_pattern* fps)
{
    /* finder pattern type layout
        0	1
        3	2
    */
	jab_vector2d side_size = {-1, -1};
	jab_int32 flag1, flag2;

	//calculate the horizontal side size
    jab_int32 size_x_top = calculateModuleNumber(fps[0], fps[1]) + 7;
    size_x_top = getSideSize(size_x_top, &flag1);
    jab_int32 size_x_bottom = calculateModuleNumber(fps[3], fps[2]) + 7;
	size_x_bottom = getSideSize(size_x_bottom, &flag2);

	side_size.x = chooseSideSize(size_x_top, flag1, size_x_bottom, flag2);

	//calculate the vertical side size
	jab_int32 size_y_left = calculateModuleNumber(fps[0], fps[3]) + 7;
    size_y_left = getSideSize(size_y_left, &flag1);
    jab_int32 size_y_right = calculateModuleNumber(fps[1], fps[2]) + 7;
    size_y_right = getSideSize(size_y_right, &flag2);

    side_size.y = chooseSideSize(size_y_left, flag1, size_y_right, flag2);

    return side_size;
}

/**
 * @brief Get the nearest valid position of the first alignment pattern
 * @param pos the input position
 * @return the nearest valid position | -1: invalid position
*/
jab_int32 getFirstAPPos(jab_int32 pos)
{
    switch (pos % 3)
    {
    case 0:
        pos--;
        break;
    case 1:
        pos++;
        break;
    }
    if(pos < 14 || pos > 26)
        pos = -1;
    return pos;
}

/**
 * @brief Detect the first alignment pattern between two finder patterns
 * @param ch the binarized color channels of the image
 * @param side_version the side version
 * @param fp1 the first finder pattern
 * @param fp2 the second finder pattern
 * @return the position of the found alignment pattern | JAB_FAILURE: if not found
*/
jab_int32 detectFirstAP(jab_bitmap* ch[], jab_int32 side_version, jab_finder_pattern fp1, jab_finder_pattern fp2)
{
    jab_alignment_pattern ap;
     //direction: from FP1 to FP2
    jab_float distx = fp2.center.x - fp1.center.x;
    jab_float disty = fp2.center.y - fp1.center.y;
    jab_float alpha = atan2(disty, distx);

    jab_int32 next_version = side_version;
    jab_int32 dir = 1;
    jab_int32 up = 0, down = 0;
    do
    {
        //distance to FP1
        jab_float distance = fp1.module_size * (jab_ap_pos[next_version-1][1] - jab_ap_pos[next_version-1][0]);
        //estimate the coordinate of the first AP
        ap.center.x = fp1.center.x + distance * cos(alpha);
        ap.center.y = fp1.center.y + distance * sin(alpha);
        ap.module_size = fp1.module_size;
        //detect AP
        ap.found_count = 0;
        ap = findAlignmentPattern(ch, ap.center.x, ap.center.y, ap.module_size, APX);
        if(ap.found_count > 0)
        {
            jab_int32 pos = getFirstAPPos(4 + calculateModuleNumber(fp1, ap));
            if(pos > 0)
                return pos;
        }

        //try next version
        dir = -dir;
        if(dir == -1)
        {
            up++;
            next_version = up * dir + side_version;
            if(next_version < 6 || next_version > 32)
            {
                dir = -dir;
                up--;
                down++;
                next_version = down * dir + side_version;
            }
        }
        else
        {
            down++;
            next_version = down * dir + side_version;
            if(next_version < 6 || next_version > 32)
            {
                dir = -dir;
                down--;
                up++;
                next_version = up * dir + side_version;
            }
        }
    }while((up+down) < 5);

    return JAB_FAILURE;

}

/**
 * @brief Confirm the side version by alignment pattern's positions
 * @param side_version the side version
 * @param found_ap_number the number of the found alignment patterns
 * @param ap_positions the positions of the found alignment patterns
 * @return the confirmed side version | JAB_FAILURE: if can not be confirmed
*/
jab_int32 confirmSideVersion(jab_int32 side_version, jab_int32 first_ap_pos)
{
    if(first_ap_pos <= 0)
    {
#if TEST_MODE
        JAB_REPORT_ERROR(("Invalid position of the first AP."))
#endif
        return JAB_FAILURE;
    }

    jab_int32 v = side_version;
    jab_int32 k = 1, sign = -1;
    jab_boolean flag = 0;
    do
    {
        if(first_ap_pos == jab_ap_pos[v-1][1])
        {
            flag = 1;
            break;
        }
        v = side_version + sign*k;
        if(sign > 0) k++;
        sign = -sign;
    }while(v>=6 && v<=32);

    if(flag) return v;
    else     return JAB_FAILURE;
}

/**
 * @brief Confirm the symbol size by alignment patterns
 * @param ch the binarized color channels of the image
 * @param fps the finder patterns
 * @param symbol the symbol
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_int32 confirmSymbolSize(jab_bitmap* ch[], jab_finder_pattern* fps, jab_decoded_symbol* symbol)
{
 	jab_int32 first_ap_pos;

	//side version x: scan the line between FP0 and FP1
    first_ap_pos = detectFirstAP(ch, symbol->metadata.side_version.x, fps[0], fps[1]);
#if TEST_MODE
    JAB_REPORT_INFO(("The position of the first AP between FP0 and FP1 is %d", first_ap_pos))
#endif // TEST_MODE
    jab_int32 side_version_x = confirmSideVersion(symbol->metadata.side_version.x, first_ap_pos);
    if(side_version_x == 0) //if failed, try the line between FP3 and FP2
    {
        first_ap_pos = detectFirstAP(ch, symbol->metadata.side_version.x, fps[3], fps[2]);
#if TEST_MODE
        JAB_REPORT_INFO(("The position of the first AP between FP3 and FP2 is %d", first_ap_pos))
#endif // TEST_MODE
        side_version_x = confirmSideVersion(symbol->metadata.side_version.x, first_ap_pos);
        if(side_version_x == 0)
        {
#if TEST_MODE
            JAB_REPORT_ERROR(("Confirming side version x failed."))
#endif
            return JAB_FAILURE;
        }
    }
    symbol->metadata.side_version.x = side_version_x;
    symbol->side_size.x = VERSION2SIZE(side_version_x);

    //side version y: scan the line between FP0 and FP3
    first_ap_pos = detectFirstAP(ch, symbol->metadata.side_version.y, fps[0], fps[3]);
#if TEST_MODE
    JAB_REPORT_INFO(("The position of the first AP between FP0 and FP3 is %d", first_ap_pos))
#endif // TEST_MODE
    jab_int32 side_version_y = confirmSideVersion(symbol->metadata.side_version.y, first_ap_pos);
    if(side_version_y == 0) //if failed, try the line between FP1 and FP2
    {
        first_ap_pos = detectFirstAP(ch, symbol->metadata.side_version.y, fps[1], fps[2]);
#if TEST_MODE
        JAB_REPORT_INFO(("The position of the first AP between FP1 and FP2 is %d", first_ap_pos))
#endif // TEST_MODE
        side_version_y = confirmSideVersion(symbol->metadata.side_version.y, first_ap_pos);
        if(side_version_y == 0)
        {
#if TEST_MODE
            JAB_REPORT_ERROR(("Confirming side version y failed."))
#endif
            return JAB_FAILURE;
        }
    }
    symbol->metadata.side_version.y = side_version_y;
    symbol->side_size.y = VERSION2SIZE(side_version_y);

    return JAB_SUCCESS;
}

/**
 * @brief Sample a symbol using alignment patterns
 * @param bitmap the image bitmap
 * @param ch the binarized color channels of the image
 * @param symbol the symbol to be sampled
 * @param fps the finder patterns
 * @return the sampled symbol matrix | NULL if failed
*/
jab_bitmap* sampleSymbolByAlignmentPattern(jab_bitmap* bitmap, jab_bitmap* ch[], jab_decoded_symbol* symbol, jab_finder_pattern* fps)
{
	//if no alignment pattern available, abort
    if(symbol->metadata.side_version.x < 6 && symbol->metadata.side_version.y < 6)
	{
		reportError("No alignment pattern is available");
		return NULL;
	}

	//For default mode, first confirm the symbol side size
	if(symbol->metadata.default_mode)
    {
        if(!confirmSymbolSize(ch, fps, symbol))
        {
            reportError("The symbol size can not be recognized.");
            return NULL;
        }
#if TEST_MODE
        JAB_REPORT_INFO(("Side sizes confirmed by APs: %d %d", symbol->side_size.x, symbol->side_size.y))
#endif
    }

    jab_int32 side_ver_x_index = symbol->metadata.side_version.x - 1;
	jab_int32 side_ver_y_index = symbol->metadata.side_version.y - 1;
	jab_int32 number_of_ap_x = jab_ap_num[side_ver_x_index];
    jab_int32 number_of_ap_y = jab_ap_num[side_ver_y_index];

    //buffer for all possible alignment patterns
	jab_alignment_pattern* aps = (jab_alignment_pattern *)malloc(number_of_ap_x * number_of_ap_y *sizeof(jab_alignment_pattern));
	if(aps == NULL)
	{
		reportError("Memory allocation for alignment patterns failed");
		return NULL;
	}
    //detect all APs
	for(jab_int32 i=0; i<number_of_ap_y; i++)
	{
		for(jab_int32 j=0; j<number_of_ap_x; j++)
		{
			jab_int32 index = i * number_of_ap_x + j;
			if(i == 0 && j == 0)
				aps[index] = fps[0];
			else if(i == 0 && j == number_of_ap_x - 1)
				aps[index] = fps[1];
			else if(i == number_of_ap_y - 1 && j == number_of_ap_x - 1)
				aps[index] = fps[2];
			else if(i == number_of_ap_y - 1 && j == 0)
				aps[index] = fps[3];
			else
			{
				if(i == 0)
				{
					//direction: from aps[0][j-1] to fps[1]
					jab_float distx = fps[1].center.x - aps[j-1].center.x;
					jab_float disty = fps[1].center.y - aps[j-1].center.y;
					jab_float alpha = atan2(disty, distx);
					//distance:  aps[0][j-1].module_size * module_number_between_APs
					jab_float distance = aps[j-1].module_size * (jab_ap_pos[side_ver_x_index][j] - jab_ap_pos[side_ver_x_index][j-1]);
					//calculate the coordinate of ap[index]
					aps[index].center.x = aps[j-1].center.x + distance * cos(alpha);
					aps[index].center.y = aps[j-1].center.y + distance * sin(alpha);
					aps[index].module_size = aps[j-1].module_size;
				}
				else if(j == 0)
				{
					//direction: from aps[i-1][0] to fps[3]
					jab_float distx = fps[3].center.x - aps[(i-1) * number_of_ap_x].center.x;
					jab_float disty = fps[3].center.y - aps[(i-1) * number_of_ap_x].center.y;
					jab_float alpha = atan2(disty, distx);
					//distance:  aps[i-1][0].module_size * module_number_between_APs
					jab_float distance = aps[(i-1) * number_of_ap_x].module_size * (jab_ap_pos[side_ver_y_index][i] - jab_ap_pos[side_ver_y_index][i-1]);
					//calculate the coordinate of ap[index]
					aps[index].center.x = aps[(i-1) * number_of_ap_x].center.x + distance * cos(alpha);
					aps[index].center.y = aps[(i-1) * number_of_ap_x].center.y + distance * sin(alpha);
					aps[index].module_size = aps[(i-1) * number_of_ap_x].module_size;
				}
				else
				{
					//estimate the coordinate of ap[index] from aps[i-1][j-1], aps[i-1][j] and aps[i][j-1]
					jab_int32 index_ap0 = (i-1) * number_of_ap_x + (j-1);	//ap at upper-left
					jab_int32 index_ap1 = (i-1) * number_of_ap_x + j;		//ap at upper-right
					jab_int32 index_ap3 = i * number_of_ap_x + (j-1);		//ap at left
					jab_float ave_size_ap01 = (aps[index_ap0].module_size + aps[index_ap1].module_size) / 2.0f;
					jab_float ave_size_ap13 = (aps[index_ap1].module_size + aps[index_ap3].module_size) / 2.0f;
					aps[index].center.x = (aps[index_ap1].center.x - aps[index_ap0].center.x) / ave_size_ap01 * ave_size_ap13 + aps[index_ap3].center.x;
					aps[index].center.y = (aps[index_ap1].center.y - aps[index_ap0].center.y) / ave_size_ap01 * ave_size_ap13 + aps[index_ap3].center.y;
					aps[index].module_size = ave_size_ap13;
				}
				//find aps[index]
				aps[index].found_count = 0;
				jab_alignment_pattern tmp = aps[index];
				aps[index] = findAlignmentPattern(ch, aps[index].center.x, aps[index].center.y, aps[index].module_size, APX);
				if(aps[index].found_count == 0)
				{
					aps[index] = tmp;	//recover the estimated one
#if TEST_MODE
					JAB_REPORT_ERROR(("The alignment pattern (index: %d) at (X: %f, Y: %f) not found", index, aps[index].center.x, aps[index].center.y))
#endif
				}
			}
		}
	}

#if TEST_MODE
	drawFoundFinderPatterns((jab_finder_pattern*)aps, number_of_ap_x * number_of_ap_y, 0x0000ff);
#endif

	//determine the minimal sampling rectangle for each block
	jab_int32 block_number = (number_of_ap_x-1) * (number_of_ap_y-1);
	jab_vector2d rect[block_number * 2];
	jab_int32 rect_index = 0;
	for(jab_int32 i=0; i<number_of_ap_y-1; i++)
	{
		for(jab_int32 j=0; j<number_of_ap_x-1; j++)
		{
			jab_vector2d tl={0,0};
			jab_vector2d br={0,0};
			jab_boolean flag = 1;

			for(jab_int32 delta=0; delta<=(number_of_ap_x-2 + number_of_ap_y-2) && flag; delta++)
			{
				for(jab_int32 dy=0; dy<=MIN(delta, number_of_ap_y-2) && flag; dy++)
				{
					jab_int32 dx = MIN(delta - dy, number_of_ap_x-2);
					for(jab_int32 dy1=0; dy1<=dy && flag; dy1++)
					{
						jab_int32 dy2 = dy - dy1;
						for(jab_int32 dx1=0; dx1<=dx && flag; dx1++)
						{
							jab_int32 dx2 = dx - dx1;

							tl.x = MAX(j - dx1, 0);
							tl.y = MAX(i - dy1, 0);
							br.x = MIN(j + 1 + dx2, number_of_ap_x - 1);
							br.y = MIN(i + 1 + dy2, number_of_ap_y - 1);
							if(aps[tl.y*number_of_ap_x + tl.x].found_count > 0 &&
							   aps[tl.y*number_of_ap_x + br.x].found_count > 0 &&
							   aps[br.y*number_of_ap_x + tl.x].found_count > 0 &&
							   aps[br.y*number_of_ap_x + br.x].found_count > 0)
							{
								flag = 0;
							}
						}
					}
				}
			}
			//save the minimal rectangle if there is no duplicate
			jab_boolean dup_flag = 0;
			for(jab_int32 k=0; k<rect_index; k+=2)
			{
				if(rect[k].x == tl.x && rect[k].y == tl.y && rect[k+1].x == br.x && rect[k+1].y == br.y)
				{
					dup_flag = 1;
					break;
				}
			}
			if(!dup_flag)
			{
				rect[rect_index] = tl;
				rect[rect_index+1] = br;
				rect_index += 2;
			}
		}
	}
	//sort the rectangles in descending order according to the size
	for(jab_int32 i=0; i<rect_index-2; i+=2)
	{
		for(jab_int32 j=0; j<rect_index-2-i; j+=2)
		{
			jab_int32 size0 = (rect[j+1].x - rect[j].x) * (rect[j+1].y - rect[j].y);
			jab_int32 size1 = (rect[j+2+1].x - rect[j+2].x) * (rect[j+2+1].y - rect[j+2].y);
			if(size1 > size0)
			{
				jab_vector2d tmp;
				//swap top-left
				tmp = rect[j];
				rect[j] = rect[j+2];
				rect[j+2] = tmp;
				//swap bottom-right
				tmp = rect[j+1];
				rect[j+1] = rect[j+2+1];
				rect[j+2+1] = tmp;
			}
		}
	}

	//allocate the buffer for the sampled matrix of the symbol
    jab_int32 width = symbol->side_size.x;
	jab_int32 height= symbol->side_size.y;
	jab_int32 mtx_bytes_per_pixel = bitmap->bits_per_pixel / 8;
	jab_int32 mtx_bytes_per_row = width * mtx_bytes_per_pixel;
	jab_bitmap* matrix = (jab_bitmap*)malloc(sizeof(jab_bitmap) + width*height*mtx_bytes_per_pixel*sizeof(jab_byte));
	if(matrix == NULL)
	{
		reportError("Memory allocation for symbol bitmap matrix failed");
		return NULL;
	}
	matrix->channel_count = bitmap->channel_count;
	matrix->bits_per_channel = bitmap->bits_per_channel;
	matrix->bits_per_pixel = matrix->bits_per_channel * matrix->channel_count;
	matrix->width = width;
	matrix->height= height;

	for(jab_int32 i=0; i<rect_index; i+=2)
	{
		jab_vector2d blk_size;
		jab_point p0, p1, p2, p3;

		//middle blocks
		blk_size.x = jab_ap_pos[side_ver_x_index][rect[i+1].x] - jab_ap_pos[side_ver_x_index][rect[i].x] + 1;
		blk_size.y = jab_ap_pos[side_ver_y_index][rect[i+1].y] - jab_ap_pos[side_ver_y_index][rect[i].y] + 1;
		p0.x = 0.5f;
		p0.y = 0.5f;
		p1.x = (jab_float)blk_size.x - 0.5f;
		p1.y = 0.5f;
		p2.x = (jab_float)blk_size.x - 0.5f;
		p2.y = (jab_float)blk_size.y - 0.5f;
		p3.x = 0.5f;
		p3.y = (jab_float)blk_size.y - 0.5f;
		//blocks on the top border row
		if(rect[i].y == 0)
		{
			blk_size.y += (DISTANCE_TO_BORDER - 1);
			p0.y = 3.5f;
			p1.y = 3.5f;
			p2.y = (jab_float)blk_size.y - 0.5f;
			p3.y = (jab_float)blk_size.y - 0.5f;
		}
		//blocks on the bottom border row
		if(rect[i+1].y == (number_of_ap_y-1))
		{
			blk_size.y += (DISTANCE_TO_BORDER - 1);
			p2.y = (jab_float)blk_size.y - 3.5f;
			p3.y = (jab_float)blk_size.y - 3.5f;
		}
		//blocks on the left border column
		if(rect[i].x == 0)
		{
			blk_size.x += (DISTANCE_TO_BORDER - 1);
			p0.x = 3.5f;
			p1.x = (jab_float)blk_size.x - 0.5f;
			p2.x = (jab_float)blk_size.x - 0.5f;
			p3.x = 3.5f;
		}
		//blocks on the right border column
		if(rect[i+1].x == (number_of_ap_x-1))
		{
			blk_size.x += (DISTANCE_TO_BORDER - 1);
			p1.x = (jab_float)blk_size.x - 3.5f;
			p2.x = (jab_float)blk_size.x - 3.5f;
		}
		//calculate perspective transform matrix for the current block
		jab_perspective_transform* pt = perspectiveTransform(
					p0.x, p0.y,
					p1.x, p1.y,
					p2.x, p2.y,
					p3.x, p3.y,
					aps[rect[i+0].y*number_of_ap_x + rect[i+0].x].center.x, aps[rect[i+0].y*number_of_ap_x + rect[i+0].x].center.y,
					aps[rect[i+0].y*number_of_ap_x + rect[i+1].x].center.x, aps[rect[i+0].y*number_of_ap_x + rect[i+1].x].center.y,
					aps[rect[i+1].y*number_of_ap_x + rect[i+1].x].center.x, aps[rect[i+1].y*number_of_ap_x + rect[i+1].x].center.y,
					aps[rect[i+1].y*number_of_ap_x + rect[i+0].x].center.x, aps[rect[i+1].y*number_of_ap_x + rect[i+0].x].center.y);
		if(pt == NULL)
		{
			free(aps);
			free(matrix);
			return NULL;
		}
		//sample the current block
#if TEST_MODE
		test_mode_color = 0;
#endif
		jab_bitmap* block = sampleSymbol(bitmap, pt, blk_size);
		free(pt);
		if(block == NULL)
		{
			reportError("Sampling block failed");
			free(aps);
			free(matrix);
			return NULL;
		}
		//save the sampled block in the matrix
		jab_int32 start_x = jab_ap_pos[side_ver_x_index][rect[i].x] - 1;
		jab_int32 start_y = jab_ap_pos[side_ver_y_index][rect[i].y] - 1;
		if(rect[i].x == 0)
			start_x = 0;
		if(rect[i].y == 0)
			start_y = 0;
		jab_int32 blk_bytes_per_pixel = block->bits_per_pixel / 8;
		jab_int32 blk_bytes_per_row = blk_size.x * mtx_bytes_per_pixel;
		for(jab_int32 y=0, mtx_y=start_y; y<blk_size.y && mtx_y<height; y++, mtx_y++)
		{
			for(jab_int32 x=0, mtx_x=start_x; x<blk_size.x && mtx_x<width; x++, mtx_x++)
			{
				jab_int32 mtx_offset = mtx_y * mtx_bytes_per_row + mtx_x * mtx_bytes_per_pixel;
				jab_int32 blk_offset = y * blk_bytes_per_row + x * blk_bytes_per_pixel;
				matrix->pixel[mtx_offset] 	  = block->pixel[blk_offset];
				matrix->pixel[mtx_offset + 1] = block->pixel[blk_offset + 1];
				matrix->pixel[mtx_offset + 2] = block->pixel[blk_offset + 2];
				matrix->pixel[mtx_offset + 3] = block->pixel[blk_offset + 3];
			}
		}
		free(block);
	}
#if TEST_MODE
    saveImage(test_mode_bitmap, "jab_sample_pos_ap.png");
#endif

	free(aps);
	return matrix;
}

/**
 * @brief Get the average pixel value around the found finder patterns
 * @param bitmap the image bitmap
 * @param fps the finder patterns
 * @param rgb_ave the average pixel value
*/
void getAveragePixelValue(jab_bitmap* bitmap, jab_finder_pattern* fps, jab_float* rgb_ave)
{
    jab_float r_ave[4] = {0, 0, 0, 0};
    jab_float g_ave[4] = {0, 0, 0, 0};
    jab_float b_ave[4] = {0, 0, 0, 0};

    //calculate average pixel value around each found FP
    for(jab_int32 i=0; i<4; i++)
    {
        if(fps[i].found_count <= 0) continue;

        jab_float radius = fps[i].module_size * 4;
        jab_int32 start_x = (fps[i].center.x - radius) >= 0 ? (fps[i].center.x - radius) : 0;
        jab_int32 start_y = (fps[i].center.y - radius) >= 0 ? (fps[i].center.y - radius) : 0;
        jab_int32 end_x	  = (fps[i].center.x + radius) <= (bitmap->width - 1) ? (fps[i].center.x + radius) : (bitmap->width - 1);
        jab_int32 end_y   = (fps[i].center.y + radius) <= (bitmap->height- 1) ? (fps[i].center.y + radius) : (bitmap->height- 1);
        jab_int32 area_width = end_x - start_x;
        jab_int32 area_height= end_y - start_y;

        jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
        jab_int32 bytes_per_row = bitmap->width * bytes_per_pixel;
        for(jab_int32 y=start_y; y<end_y; y++)
        {
            for(jab_int32 x=start_x; x<end_x; x++)
            {
                jab_int32 offset = y * bytes_per_row + x * bytes_per_pixel;
                r_ave[i] += bitmap->pixel[offset + 0];
                g_ave[i] += bitmap->pixel[offset + 1];
                b_ave[i] += bitmap->pixel[offset + 2];
            }
        }
        r_ave[i] /= (jab_float)(area_width * area_height);
        g_ave[i] /= (jab_float)(area_width * area_height);
        b_ave[i] /= (jab_float)(area_width * area_height);
    }

    //calculate the average values of the average pixel values
    jab_float rgb_sum[3] = {0, 0, 0};
    jab_int32 rgb_count[3] = {0, 0, 0};
    for(jab_int32 i=0; i<4; i++)
    {
        if(r_ave[i] > 0)
        {
            rgb_sum[0] += r_ave[i];
            rgb_count[0]++;
        }
        if(g_ave[i] > 0)
        {
            rgb_sum[1] += g_ave[i];
            rgb_count[1]++;
        }
        if(b_ave[i] > 0)
        {
            rgb_sum[2] += b_ave[i];
            rgb_count[2]++;
        }
    }
    if(rgb_count[0] > 0) rgb_ave[0] = rgb_sum[0] / (jab_float)rgb_count[0];
    if(rgb_count[1] > 0) rgb_ave[1] = rgb_sum[1] / (jab_float)rgb_count[1];
    if(rgb_count[2] > 0) rgb_ave[2] = rgb_sum[2] / (jab_float)rgb_count[2];
}

/**
 * @brief Detect and decode a master symbol
 * @param bitmap the image bitmap
 * @param ch the binarized color channels of the image
 * @param master_symbol the master symbol
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean detectMaster(jab_bitmap* bitmap, jab_bitmap* ch[], jab_decoded_symbol* master_symbol)
{
    //find master symbol
    jab_finder_pattern* fps;
    jab_int32 status;
    fps = findMasterSymbol(bitmap, ch, INTENSIVE_DETECT, &status);
    if(status == FATAL_ERROR) return JAB_FAILURE;
    else if(status == JAB_FAILURE)
    {
#if TEST_MODE
        JAB_REPORT_INFO(("Trying to detect more finder patterns based on the found ones"))
#endif
        //calculate the average pixel value around the found FPs
        jab_float rgb_ave[3];
        getAveragePixelValue(bitmap, fps, rgb_ave);
        free(fps);
        //binarize the bitmap using the average pixel values as thresholds
        for(jab_int32 i=0; i<3; free(ch[i++]));
        if(!binarizerRGB(bitmap, ch, rgb_ave))
        {
            return JAB_FAILURE;
        }
        //find master symbol
        fps = findMasterSymbol(bitmap, ch, INTENSIVE_DETECT, &status);
        if(status == JAB_FAILURE || status == FATAL_ERROR)
        {
            free(fps);
            return JAB_FAILURE;
        }
    }

    //calculate the master symbol side size
    jab_vector2d side_size = calculateSideSize(fps);
    if(side_size.x == -1 || side_size.y == -1)
    {
		reportError("Calculating side size failed");
        free(fps);
		return JAB_FAILURE;
    }
#if TEST_MODE
	JAB_REPORT_INFO(("Side sizes: %d %d", side_size.x, side_size.y))
#endif
    //try decoding using only finder patterns
	//calculate perspective transform matrix
	jab_perspective_transform* pt = getPerspectiveTransform(fps[0].center, fps[1].center,
															fps[2].center, fps[3].center,
															side_size);
	if(pt == NULL)
	{
		free(fps);
		return JAB_FAILURE;
	}

	//sample master symbol
#if TEST_MODE
	test_mode_color = 255;
#endif
	jab_bitmap* matrix = sampleSymbol(bitmap, pt, side_size);
	free(pt);
#if TEST_MODE
	saveImage(test_mode_bitmap, "jab_sample_pos_fp.png");
#endif
	if(matrix == NULL)
	{
		reportError("Sampling master symbol failed");
		free(fps);
		return JAB_FAILURE;
	}

	//save the detection result
	master_symbol->index = 0;
	master_symbol->host_index = 0;
	master_symbol->side_size = side_size;
	master_symbol->module_size = (fps[0].module_size + fps[1].module_size + fps[2].module_size + fps[3].module_size) / 4.0f;
	master_symbol->pattern_positions[0] = fps[0].center;
	master_symbol->pattern_positions[1] = fps[1].center;
	master_symbol->pattern_positions[2] = fps[2].center;
	master_symbol->pattern_positions[3] = fps[3].center;

	//decode master symbol
	jab_int32 decode_result = decodeMaster(matrix, master_symbol);
	free(matrix);
	if(decode_result == JAB_SUCCESS)
	{
		free(fps);
		return JAB_SUCCESS;
	}
	else if(decode_result < 0)	//fatal error occurred
	{
		free(fps);
		return JAB_FAILURE;
	}
	else	//if decoding using only finder patterns failed, try decoding using alignment patterns
	{
#if TEST_MODE
		JAB_REPORT_INFO(("Trying to sample master symbol using alignment pattern..."))
#endif // TEST_MODE
		master_symbol->side_size.x = VERSION2SIZE(master_symbol->metadata.side_version.x);
		master_symbol->side_size.y = VERSION2SIZE(master_symbol->metadata.side_version.y);
		matrix = sampleSymbolByAlignmentPattern(bitmap, ch, master_symbol, fps);
		free(fps);
		if(matrix == NULL)
		{
#if TEST_MODE
			reportError("Sampling master symbol using alignment pattern failed");
#endif // TEST_MODE
			return JAB_FAILURE;
		}
		decode_result = decodeMaster(matrix, master_symbol);
		free(matrix);
		if(decode_result == JAB_SUCCESS)
			return JAB_SUCCESS;
		else
			return JAB_FAILURE;
	}
}

/**
 * @brief Detect a slave symbol
 * @param bitmap the image bitmap
 * @param ch the binarized color channels of the image
 * @param host_symbol the host symbol
 * @param slave_symbol the slave symbol
 * @param docked_position the docked position
 * @return the sampled slave symbol matrix | NULL if failed
 *
*/
jab_bitmap* detectSlave(jab_bitmap* bitmap, jab_bitmap* ch[], jab_decoded_symbol* host_symbol, jab_decoded_symbol* slave_symbol, jab_int32 docked_position)
{
    if(docked_position < 0 || docked_position > 3)
    {
        reportError("Wrong docking position");
        return NULL;
    }

    //find slave symbol next to the host symbol
    if(!findSlaveSymbol(bitmap, ch, host_symbol, slave_symbol, docked_position))
    {
        JAB_REPORT_ERROR(("Slave symbol %d not found", slave_symbol->index))
        return NULL;
    }

    //calculate perspective transform matrix
    jab_perspective_transform* pt = getPerspectiveTransform(slave_symbol->pattern_positions[0], slave_symbol->pattern_positions[1],
                                                            slave_symbol->pattern_positions[2], slave_symbol->pattern_positions[3],
                                                            slave_symbol->side_size);
    if(pt == NULL)
    {
        return NULL;
    }

    //sample slave symbol
#if TEST_MODE
	test_mode_color = 255;
#endif
    jab_bitmap* matrix = sampleSymbol(bitmap, pt, slave_symbol->side_size);
    if(matrix == NULL)
    {
        JAB_REPORT_ERROR(("Sampling slave symbol %d failed", slave_symbol->index))
        free(pt);
        return JAB_FAILURE;
    }

    free(pt);
    return matrix;
}

/**
 * @brief Decode docked slave symbols around a host symbol
 * @param bitmap the image bitmap
 * @param ch the binarized color channels of the image
 * @param symbols the symbol list
 * @param host_index the index number of the host symbol
 * @param total the number of symbols in the list
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean decodeDockedSlaves(jab_bitmap* bitmap, jab_bitmap* ch[], jab_decoded_symbol* symbols, jab_int32 host_index, jab_int32* total)
{
    jab_int32 docked_positions[4] = {0};
    docked_positions[0] = symbols[host_index].metadata.docked_position & 0x08;
    docked_positions[1] = symbols[host_index].metadata.docked_position & 0x04;
    docked_positions[2] = symbols[host_index].metadata.docked_position & 0x02;
    docked_positions[3] = symbols[host_index].metadata.docked_position & 0x01;

    for(jab_int32 j=0; j<4; j++)
    {
        if(docked_positions[j] > 0 && (*total)<MAX_SYMBOL_NUMBER)
        {
            symbols[*total].index = *total;
            symbols[*total].host_index = host_index;
            symbols[*total].metadata = symbols[host_index].slave_metadata[j];
            jab_bitmap* matrix = detectSlave(bitmap, ch, &symbols[host_index], &symbols[*total], j);
            if(matrix == NULL)
            {
                JAB_REPORT_ERROR(("Detecting slave symbol %d failed", symbols[*total].index))
                return JAB_FAILURE;
            }
            if(decodeSlave(matrix, &symbols[*total]) > 0)
            {
                (*total)++;
                free(matrix);
            }
            else
            {
                free(matrix);
                return JAB_FAILURE;
            }
        }
    }
    return JAB_SUCCESS;
}

/**
 * @brief Extended function to decode a JAB Code
 * @param bitmap the image bitmap
 * @param mode the decoding mode(NORMAL_DECODE: only output completely decoded data when all symbols are correctly decoded
 *								 COMPATIBLE_DECODE: also output partly decoded data even if some symbols are not correctly decoded
 * @param status the decoding status code (0: not detectable, 1: not decodable, 2: partly decoded with COMPATIBLE_DECODE mode, 3: fully decoded)
 * @param symbols the decoded symbols
 * @param max_symbol_number the maximal possible number of symbols to be decoded
 * @return the decoded data | NULL if failed
*/
jab_data* decodeJABCodeEx(jab_bitmap* bitmap, jab_int32 mode, jab_int32* status, jab_decoded_symbol* symbols, jab_int32 max_symbol_number)
{
	if(status) *status = 0;
	if(!symbols)
	{
		reportError("Invalid symbol buffer");
		return NULL;
	}

	//binarize r, g, b channels
	jab_bitmap* ch[3];
	balanceRGB(bitmap);
    if(!binarizerRGB(bitmap, ch, 0))
	{
		return NULL;
	}
#if TEST_MODE
    saveImage(bitmap, "jab_balanced.png");
#endif // TEST_MODE

#if TEST_MODE
    test_mode_bitmap = (jab_bitmap*)malloc(sizeof(jab_bitmap) + bitmap->width * bitmap->height * bitmap->channel_count * (bitmap->bits_per_channel/8));
    test_mode_bitmap->bits_per_channel = bitmap->bits_per_channel;
    test_mode_bitmap->bits_per_pixel   = bitmap->bits_per_pixel;
    test_mode_bitmap->channel_count	  = bitmap->channel_count;
    test_mode_bitmap->height 		  = bitmap->height;
    test_mode_bitmap->width			  = bitmap->width;
    memcpy(test_mode_bitmap->pixel, bitmap->pixel, bitmap->width * bitmap->height * bitmap->channel_count * (bitmap->bits_per_channel/8));
    saveImage(ch[0], "jab_r.png");
    saveImage(ch[1], "jab_g.png");
    saveImage(ch[2], "jab_b.png");
#endif

	//initialize symbols buffer
    memset(symbols, 0, max_symbol_number * sizeof(jab_decoded_symbol));
    jab_int32 total = 0;	//total number of decoded symbols
    jab_boolean res = 1;

    //detect and decode master symbol
    if(detectMaster(bitmap, ch, &symbols[0]))
	{
		total++;
	}
    //detect and decode docked slave symbols recursively
    if(total>0)
    {
        for(jab_int32 i=0; i<total && total<max_symbol_number; i++)
        {
            if(!decodeDockedSlaves(bitmap, ch, symbols, i, &total))
            {
                res = 0;
                break;
            }
        }
    }

    //check result
	if(total == 0 || (mode == NORMAL_DECODE && res == 0 ))
	{
		if(symbols[0].module_size > 0 && status)
			*status = 1;
		//clean memory
		for(jab_int32 i=0; i<3; free(ch[i++]));
		for(jab_int32 i=0; i<=MIN(total, max_symbol_number-1); i++)
		{
			free(symbols[i].palette);
			free(symbols[i].data);
		}
        return NULL;
	}
	if(mode == COMPATIBLE_DECODE && res == 0)
	{
		if(status) *status = 2;
		res = 1;
	}

	//concatenate the decoded data
    jab_int32 total_data_length = 0;
    for(jab_int32 i=0; i<total; i++)
    {
        total_data_length += symbols[i].data->length;
    }
    jab_data* decoded_bits = (jab_data *)malloc(sizeof(jab_data) + total_data_length * sizeof(jab_char));
    if(decoded_bits == NULL){
        reportError("Memory allocation for decoded bits failed");
        if(status) *status = 1;
        return NULL;
    }
    jab_int32 offset = 0;
    for(jab_int32 i=0; i<total; i++)
    {
        jab_char* src = symbols[i].data->data;
        jab_char* dst = decoded_bits->data;
        dst += offset;
        memcpy(dst, src, symbols[i].data->length);
        offset += symbols[i].data->length;
    }
    decoded_bits->length = total_data_length;
    //decode data
    jab_data* decoded_data = decodeData(decoded_bits);
    if(decoded_data == NULL)
	{
		reportError("Decoding data failed");
		if(status) *status = 1;
		res = 0;
	}

    //clean memory
    for(jab_int32 i=0; i<3; free(ch[i++]));
    for(jab_int32 i=0; i<=MIN(total, max_symbol_number-1); i++)
    {
		free(symbols[i].palette);
		free(symbols[i].data);
    }
    free(decoded_bits);
#if TEST_MODE
	free(test_mode_bitmap);
#endif // TEST_MODE
	if(res == 0) return NULL;
	if(status)
	{
		if(*status != 2)
			*status = 3;
	}
    return decoded_data;
}

/**
 * @brief Decode a JAB Code
 * @param bitmap the image bitmap
 * @param mode the decoding mode(NORMAL_DECODE: only output completely decoded data when all symbols are correctly decoded
 *								 COMPATIBLE_DECODE: also output partly decoded data even if some symbols are not correctly decoded
 * @param status the decoding status code (0: not detectable, 1: not decodable, 2: partly decoded with COMPATIBLE_DECODE mode, 3: fully decoded)
 * @return the decoded data | NULL if failed
*/
jab_data* decodeJABCode(jab_bitmap* bitmap, jab_int32 mode, jab_int32* status)
{
	jab_decoded_symbol symbols[MAX_SYMBOL_NUMBER];
	return decodeJABCodeEx(bitmap, mode, status, symbols, MAX_SYMBOL_NUMBER);
}
