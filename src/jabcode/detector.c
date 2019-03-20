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
jab_boolean checkModuleSize(jab_float size_r, jab_float size_g, jab_float size_b)
{
	jab_float mean= (size_r + size_g + size_b) / 3.0f;
	jab_float tolerance = mean / 2.5f;

	jab_boolean condition = fabs(mean - size_r) < tolerance &&
							fabs(mean - size_g) < tolerance &&
							fabs(mean - size_b) < tolerance;
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

    jab_float module_size_r;
    jab_float centerx_r = fp->center.x;
    jab_float centery_r = fp->center.y;
    jab_int32 dir_r = 0;
    jab_int32 dcc_r = 0;

	if(!crossCheckPatternCh(ch[0], fp->type, h_v, module_size_max, &module_size_r, &centerx_r, &centery_r, &dir_r, &dcc_r))
		return JAB_FAILURE;

	jab_float module_size_g;
    jab_float centerx_g = fp->center.x;
    jab_float centery_g = fp->center.y;
    jab_int32 dir_g = 0;
    jab_int32 dcc_g = 0;

    if(!crossCheckPatternCh(ch[1], fp->type, h_v, module_size_max, &module_size_g, &centerx_g, &centery_g, &dir_g, &dcc_g))
		return JAB_FAILURE;

	jab_float module_size_b;
	jab_float centerx_b = fp->center.x;
	jab_float centery_b = fp->center.y;
	jab_int32 dir_b = 0;
	jab_int32 dcc_b = 0;

	if(!crossCheckPatternCh(ch[2], fp->type, h_v, module_size_max, &module_size_b, &centerx_b, &centery_b, &dir_b, &dcc_b))
		return JAB_FAILURE;

	//module size must be consistent
	if(!checkModuleSize(module_size_r, module_size_g, module_size_b))
		return JAB_FAILURE;
	fp->module_size = (module_size_r + module_size_g + module_size_b) / 3.0f;

	fp->center.x = (centerx_r + centerx_g + centerx_b) / 3.0f;
	fp->center.y = (centery_r + centery_g + centery_b) / 3.0f;

	if(dcc_r == 2 || dcc_g == 2 || dcc_b == 2)
		fp->direction = 2;
	else
		fp->direction = (dir_r + dir_g + dir_b) > 0 ? 1 : -1;

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
    //add a new finder pattern
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
	//check if more than one finder pattern type not found
	jab_int32 missing_fp_type_count = 0;
	for(jab_int32 i=FP0; i<=FP3; i++)
	{
		if(fp_type_count[i] == 0)
			missing_fp_type_count++;
	}
	if(missing_fp_type_count > 1)
		return missing_fp_type_count;

    //classify finder patterns into four types
    jab_finder_pattern fps0[fp_type_count[FP0]];
    jab_finder_pattern fps1[fp_type_count[FP1]];
    jab_finder_pattern fps2[fp_type_count[FP2]];
    jab_finder_pattern fps3[fp_type_count[FP3]];
    jab_int32 counter0 = 0, counter1 = 0, counter2 = 0, counter3 = 0;

    for(jab_int32 i=0; i<fp_count; i++)
    {
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

	//check finder patterns' direction
	//if the direction of fp0 and fp1 not consistent, abandon the one with smaller found_count
/*	if(fps[0].found_count > 0 && fps[1].found_count > 0 && fps[0].found_count != fps[1].found_count && fps[0].direction != fps[1].direction)
	{
		if(fps[0].found_count < fps[1].found_count)
			fps[0].found_count = 0;
		else
			fps[1].found_count = 0;
	}
	//if the direction of fp2 and fp3 not consistent, abandon the one with smaller found_count
	if(fps[2].found_count > 0 && fps[3].found_count > 0 && fps[2].found_count != fps[3].found_count && fps[2].direction != fps[3].direction)
	{
		if(fps[2].found_count < fps[3].found_count)
			fps[2].found_count = 0;
		else
			fps[3].found_count = 0;
	}
*/
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

            starty += skip;
            endy = ch[0]->height;
            //red channel
            if(seekPattern(ch[0], -1, j, &starty, &endy, &centery_r, &module_size_r, &skip))
            {
                type_r = ch[0]->pixel[(jab_int32)(centery_r)*ch[0]->width + j] > 0 ? 255 : 0;
                //green channel
                centery_g = centery_r;
                if(crossCheckPatternVertical(ch[1], module_size_r*2, (jab_float)j, &centery_g, &module_size_g))
                {
                    type_g = ch[1]->pixel[(jab_int32)(centery_g)*ch[1]->width + j] > 0 ? 255 : 0;
                    //blue channel
                    centery_b = centery_r;
                    if(crossCheckPatternVertical(ch[2], module_size_r*2, (jab_float)j, &centery_b, &module_size_b))
                    {
                        type_b = ch[2]->pixel[(jab_int32)(centery_b)*ch[2]->width + j] > 0 ? 255 : 0;

                        if(!checkModuleSize(module_size_r, module_size_g, module_size_b)) continue;

                        jab_finder_pattern fp;
                        fp.center.x = (jab_float)j;
                        fp.center.y = (centery_r + centery_g + centery_b) / 3.0f;
                        fp.module_size = (module_size_r + module_size_g + module_size_b) / 3.0f;
                        fp.found_count = 1;
                        if( type_r == jab_default_palette[FP0_CORE_COLOR * 3]	  &&
							type_g == jab_default_palette[FP0_CORE_COLOR * 3 + 1] &&
							type_b == jab_default_palette[FP0_CORE_COLOR * 3 + 2])
                        {
                            fp.type = FP0;	//candidate for fp0
                        }
                        else if(type_r == jab_default_palette[FP1_CORE_COLOR * 3] &&
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

                        if( crossCheckPattern(ch, &fp, 1) )
                        {
                            saveFinderPattern(&fp, fps, total_finder_patterns, fp_type_count);
                            if(*total_finder_patterns >= (MAX_FINDER_PATTERNS -1) )
                            {
                                done = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }while(starty < ch[0]->height && endy < ch[0]->height);
    }
}

/**
 * @brief Find the master symbol in the image
 * @param ch the binarized color channels of the image
 * @param mode the detection mode
 * @return the finder pattern list | NULL
*/
jab_finder_pattern* findMasterSymbol(jab_bitmap* ch[], jab_detect_mode mode)
{
    //suppose the code size is minimally 1/4 image size
    jab_int32 min_module_size = ch[0]->height / (2 * MAX_SYMBOL_ROWS * MAX_MODULES);
    if(min_module_size < 1 || mode == INTENSIVE_DETECT) min_module_size = 1;

    jab_finder_pattern* fps = (jab_finder_pattern*)calloc(MAX_FINDER_PATTERNS, sizeof(jab_finder_pattern));
    if(fps == NULL)
    {
        reportError("Memory allocation for finder patterns failed");
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

            startx += skip;
            endx = ch[0]->width;
            //red channel
            if(seekPatternHorizontal(row_r, &startx, &endx, &centerx_r, &module_size_r, &skip))
            {
                type_r = row_r[(jab_int32)(centerx_r)] > 0 ? 255 : 0;
                //green channel
                centerx_g = centerx_r;
                if(crossCheckPatternHorizontal(ch[1], module_size_r*2, &centerx_g, (jab_float)i, &module_size_g))
                {
                    type_g = row_g[(jab_int32)(centerx_g)] > 0 ? 255 : 0;
                    //blue channel
                    centerx_b = centerx_r;
                    if(crossCheckPatternHorizontal(ch[2], module_size_r*2, &centerx_b, (jab_float)i, &module_size_b))
                    {
                        type_b = row_b[(jab_int32)(centerx_b)] > 0 ? 255 : 0;

                        if(!checkModuleSize(module_size_r, module_size_g, module_size_b)) continue;

                        jab_finder_pattern fp;
                        fp.center.x = (centerx_r + centerx_g + centerx_b) / 3.0f;
                        fp.center.y = (jab_float)i;
                        fp.module_size = (module_size_r + module_size_g + module_size_b) / 3.0f;
                        fp.found_count = 1;
                        if( type_r == jab_default_palette[FP0_CORE_COLOR * 3]	  &&
							type_g == jab_default_palette[FP0_CORE_COLOR * 3 + 1] &&
							type_b == jab_default_palette[FP0_CORE_COLOR * 3 + 2])
                        {
                            fp.type = FP0;	//candidate for fp0
                        }
                        else if(type_r == jab_default_palette[FP1_CORE_COLOR * 3] &&
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

                        if( crossCheckPattern(ch, &fp, 0) )
                        {
                            saveFinderPattern(&fp, fps, &total_finder_patterns, fp_type_count);
                            if(total_finder_patterns >= (MAX_FINDER_PATTERNS -1) )
                            {
                                done = 1;
                                break;
                            }
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
    saveImage(test_mode_bitmap, "detector_result.png");
#endif

    //if less than 3 finder patterns are found, detection fails
    if(total_finder_patterns < 3)
    {
        reportError("Too few finder patterns found");
        free(fps);
        return NULL;
    }

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
		reportError("Too few finder pattern types found");
		free(fps);
		return NULL;
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
			free(fps);
			return NULL;
		}
    }
#if TEST_MODE
    //output the final selected 4 patterns
    JAB_REPORT_INFO(("Final patterns:"))
    for(jab_int32 i=0; i<4; i++)
    {
        JAB_REPORT_INFO(("x:%6.1f\ty:%6.1f\tsize:%.2f\tcnt:%d\ttype:%d\tdir:%d", fps[i].center.x, fps[i].center.y, fps[i].module_size, fps[i].found_count, fps[i].type, fps[i].direction))
    }
	drawFoundFinderPatterns(fps, 4, 0xff0000);
	saveImage(test_mode_bitmap, "detector_result.png");
#endif
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
 * @param module_size the module size in horizontal direction
 * @param dir the alignment pattern direction
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean crossCheckPatternAP(jab_bitmap* ch[], jab_int32 y, jab_int32 minx, jab_int32 maxx, jab_int32 cur_x, jab_int32 ap_type, jab_float max_module_size, jab_float* centerx, jab_float* centery, jab_float* module_size, jab_int32* dir)
{
	//get row
	jab_byte* row_r = ch[0]->pixel + y*ch[0]->width;
	jab_byte* row_g = ch[1]->pixel + y*ch[1]->width;
	jab_byte* row_b = ch[2]->pixel + y*ch[2]->width;

	jab_float l_centerx[3] = {0.0f};
	jab_float l_centery[3] = {0.0f};
	jab_float l_module_size_h[3] = {0.0f};
	jab_float l_module_size_v[3] = {0.0f};

	//check r channel horizontally
	l_centerx[0] = crossCheckPatternHorizontalAP(row_r, 0, minx, maxx, cur_x, ap_type, max_module_size, &l_module_size_h[0]);
	if(l_centerx[0] < 0) return JAB_FAILURE;
	//check g channel horizontally
	l_centerx[1] = crossCheckPatternHorizontalAP(row_g, 1, minx, maxx, (jab_int32)l_centerx[0], ap_type, max_module_size, &l_module_size_h[1]);
	if(l_centerx[1] < 0) return JAB_FAILURE;
	//check b channel horizontally
	l_centerx[2] = crossCheckPatternHorizontalAP(row_b, 2, minx, maxx, (jab_int32)l_centerx[0], ap_type, max_module_size, &l_module_size_h[2]);
	if(l_centerx[2] < 0) return JAB_FAILURE;

	jab_point center;
	center.x = (l_centerx[0] + l_centerx[1] + l_centerx[2]) / 3.0f;
	center.y = y;

	//check r channel vertically
	l_centery[0] = crossCheckPatternVerticalAP(ch[0], center, max_module_size, &l_module_size_v[0]);
	if(l_centery[0] < 0) return JAB_FAILURE;
	//again horizontally
	row_r = ch[0]->pixel + ((jab_int32)l_centery[0])*ch[0]->width;
	l_centerx[0] = crossCheckPatternHorizontalAP(row_r, 0, minx, maxx, center.x, ap_type, max_module_size, &l_module_size_h[0]);
	if(l_centerx[0] < 0) return JAB_FAILURE;

	//check g channel vertically
	l_centery[1] = crossCheckPatternVerticalAP(ch[1], center, max_module_size, &l_module_size_v[1]);
	if(l_centery[1] < 0) return JAB_FAILURE;
	//again horizontally
	row_g = ch[1]->pixel + ((jab_int32)l_centery[1])*ch[1]->width;
	l_centerx[1] = crossCheckPatternHorizontalAP(row_g, 1, minx, maxx, center.x, ap_type, max_module_size, &l_module_size_h[1]);
	if(l_centerx[1] < 0) return JAB_FAILURE;

	//check b channel vertically
	l_centery[2] = crossCheckPatternVerticalAP(ch[2], center, max_module_size, &l_module_size_v[2]);
	if(l_centery[2] < 0) return JAB_FAILURE;
	//again horizontally
	row_b = ch[2]->pixel + ((jab_int32)l_centery[2])*ch[2]->width;
	l_centerx[2] = crossCheckPatternHorizontalAP(row_b, 2, minx, maxx, center.x, ap_type, max_module_size, &l_module_size_h[2]);
	if(l_centerx[2] < 0) return JAB_FAILURE;

	(*module_size) = (l_module_size_h[0] + l_module_size_h[1] + l_module_size_h[2] + l_module_size_v[0] + l_module_size_v[1] + l_module_size_v[2]) / 6.0f;
	(*centerx) = (l_centerx[0] + l_centerx[1] + l_centerx[2]) / 3.0f;
	(*centery) = (l_centery[0] + l_centery[1] + l_centery[2]) / 3.0f;

	//diagonal check
	jab_int32 l_dir[3] = {0};
	center.x = (*centerx);
	center.y = (*centery);
	if(crossCheckPatternDiagonalAP(ch[0], ap_type, (*module_size)*2, center, &l_dir[0]) < 0) return JAB_FAILURE;
	if(crossCheckPatternDiagonalAP(ch[1], ap_type, (*module_size)*2, center, &l_dir[1]) < 0) return JAB_FAILURE;
	if(crossCheckPatternDiagonalAP(ch[2], ap_type, (*module_size)*2, center, &l_dir[2]) < 0) return JAB_FAILURE;
	(*dir) = (l_dir[0] + l_dir[1] + l_dir[2]) > 0 ? 1 : -1;

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

    //get slave symbol side size from its metadata
    slave_symbol->side_size.x = VERSION2SIZE(slave_symbol->metadata.side_version.x);
    slave_symbol->side_size.y = VERSION2SIZE(slave_symbol->metadata.side_version.y);

    //estimate the module size in the slave symbol
    if(docked_position == 3 || docked_position == 2)
    {
        slave_symbol->module_size = DIST(aps[ap1].center.x, aps[ap1].center.y, aps[ap2].center.x, aps[ap2].center.y) / (slave_symbol->side_size.y - 7);
    }
    if(docked_position == 1 || docked_position == 0)
    {
        slave_symbol->module_size = DIST(aps[ap1].center.x, aps[ap1].center.y, aps[ap2].center.x, aps[ap2].center.y) / (slave_symbol->side_size.x - 7);
    }

    //calculate the coordinate of ap3
    aps[ap3].center.x = aps[ap1].center.x + sign * (slave_symbol->side_size.x - 7) * slave_symbol->module_size * cos(alpha1);
    aps[ap3].center.y = aps[ap1].center.y + sign * (slave_symbol->side_size.y - 7) * slave_symbol->module_size * sin(alpha1);
    //find alignment pattern around ap3
    aps[ap3] = findAlignmentPattern(ch, aps[ap3].center.x, aps[ap3].center.y, slave_symbol->module_size, ap3);
    //calculate the coordinate of ap4
    aps[ap4].center.x = aps[ap2].center.x + sign * (slave_symbol->side_size.x - 7) * slave_symbol->module_size * cos(alpha2);
    aps[ap4].center.y = aps[ap2].center.y + sign * (slave_symbol->side_size.y - 7) * slave_symbol->module_size * sin(alpha2);
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
	saveImage(test_mode_bitmap, "detector_result.png");
#endif

    free(aps);
    return JAB_SUCCESS;
}

/**
 * @brief Get the nearest valid side size to a given size
 * @param size the input size
 * @param flag the flag indicating the magnitude of error
 * @return the nearest valid side size
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
    return size;
}

/**
 * @brief Calculate the side sizes of master symbol
 * @param ch the binarized color channels of the image
 * @param fps the finder patterns
 * @param module_size the module size
 * @return the horizontal and vertical side sizes
*/
jab_vector2d calculateSideSize(jab_bitmap* ch[], jab_finder_pattern* fps, jab_float* module_size)
{
    //TODO: calculate module size more accurately using Bresenham's line algorithm
    jab_float total_module_size = 0;
    for(jab_int32 i=0; i<4; i++)
    {
        total_module_size += fps[i].module_size;
    }
    jab_float mean = total_module_size / 4.0f;
    *module_size = mean;

    /* finder pattern type layout
        0	1
        3	2
    */
    jab_float dist_x01 = DIST(fps[0].center.x, fps[0].center.y, fps[1].center.x, fps[1].center.y);
    jab_float dist_x32 = DIST(fps[3].center.x, fps[3].center.y, fps[2].center.x, fps[2].center.y);
    jab_float dist_y03 = DIST(fps[0].center.x, fps[0].center.y, fps[3].center.x, fps[3].center.y);
    jab_float dist_y12 = DIST(fps[1].center.x, fps[1].center.y, fps[2].center.x, fps[2].center.y);

	jab_vector2d side_size = {-1, -1};
	jab_int32 flag1, flag2;

    mean = (fps[0].module_size + fps[1].module_size) / 2.0f;
    jab_int32 size_x_top = dist_x01 / mean + 7;
    size_x_top = getSideSize(size_x_top, &flag1);

    mean = (fps[3].module_size + fps[2].module_size) / 2.0f;
    jab_int32 size_x_bottom = dist_x32 / mean + 7;
	size_x_bottom = getSideSize(size_x_bottom, &flag2);

    if(flag1 == flag2)
		side_size.x = MAX(size_x_top, size_x_bottom);
	else
	{
		if(flag1)
			side_size.x = size_x_top;
		else if(flag2)
			side_size.x = size_x_bottom;
	}

    mean = (fps[0].module_size + fps[3].module_size) / 2.0f;
    jab_int32 size_y_left = dist_y03 / mean + 7;
    size_y_left = getSideSize(size_y_left, &flag1);

    mean = (fps[1].module_size + fps[2].module_size) / 2.0f;
    jab_int32 size_y_right = dist_y12 / mean + 7;
    size_y_right = getSideSize(size_y_right, &flag2);

    if(flag1 == flag2)
		side_size.y = MAX(size_y_left, size_y_right);
	else
	{
		if(flag1)
			side_size.y = size_y_left;
		else if(flag2)
			side_size.y = size_y_right;
	}

    return side_size;
}

/**
 * @brief Sample a symbol with help of alignment patterns
 * @param bitmap the image bitmap
 * @param ch the binarized color channels of the image
 * @param symbol the symbol to be sampled
 * @param fps the finder/alignment patterns
 * @return the sampled symbol matrix | NULL if failed
*/
jab_bitmap* sampleSymbolByAlignmentPattern(jab_bitmap* bitmap, jab_bitmap* ch[], jab_decoded_symbol* symbol, jab_alignment_pattern* fps)
{
	//calculate the number of alignment patterns between the finder patterns
	jab_int32 width = symbol->side_size.x;
	jab_int32 height= symbol->side_size.y;
    jab_int32 number_of_ap_x = (width - (DISTANCE_TO_BORDER*2 - 1)) / MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS - 1;
    jab_int32 number_of_ap_y = (height- (DISTANCE_TO_BORDER*2 - 1)) / MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS - 1;
	if(number_of_ap_x < 0)
		number_of_ap_x = 0;
	if(number_of_ap_y < 0)
		number_of_ap_y = 0;
	//if no alignment pattern available, abort
	if(number_of_ap_x == 0 && number_of_ap_y == 0)
		return NULL;
	//calculate the distance between alignment patters
    jab_float ap_distance_x = 0.0f;
    jab_float ap_distance_y = 0.0f;
    ap_distance_x = (jab_float)(width - (DISTANCE_TO_BORDER*2 - 1)) / (jab_float)(number_of_ap_x + 1);
    ap_distance_y = (jab_float)(height- (DISTANCE_TO_BORDER*2 - 1)) / (jab_float)(number_of_ap_y + 1);
	//add the finder patterns
	number_of_ap_x += 2;
	number_of_ap_y += 2;

	//find all alignment patterns' positions
	jab_alignment_pattern* aps = (jab_alignment_pattern *)malloc(number_of_ap_x * number_of_ap_y *sizeof(jab_alignment_pattern));
	if(aps == NULL)
	{
		reportError("Memory allocation for alignment patterns failed");
		return NULL;
	}
	jab_int32 not_found_in_a_row = 0;
	jab_int32 not_found_total = 0;
	for(jab_int32 i=0; i<number_of_ap_y; i++)
	{
		not_found_in_a_row = 0;
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
				jab_int32 module_interval;
				jab_float distance;
				if(i == 0)
				{
					//direction: from aps[0][j-1] to fps[1]
					jab_float distx = fps[1].center.x - aps[j-1].center.x;
					jab_float disty = fps[1].center.y - aps[j-1].center.y;
					jab_float alpha = atan2(disty, distx);
					//distance:  module_interval * aps[0][j-1].module_size
					module_interval = (jab_int32)(j * ap_distance_x) - (jab_int32)((j-1) * ap_distance_x);
					distance = module_interval * aps[j-1].module_size;
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
					//distance:  module_interval * aps[i-1][0].module_size
					module_interval = (jab_int32)(i * ap_distance_y) - (jab_int32)((i-1) * ap_distance_y);
					distance = module_interval * aps[(i-1) * number_of_ap_x].module_size;
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
					not_found_in_a_row++;
					not_found_total++;
#if TEST_MODE
					JAB_REPORT_ERROR(("The alignment pattern (index: %d) at (X: %f, Y: %f) not found", index, aps[index].center.x, aps[index].center.y))
#endif // TEST_MODE
				}
				else
					not_found_in_a_row = 0;
			}
		}
	}

#if TEST_MODE
	drawFoundFinderPatterns((jab_finder_pattern*)aps, number_of_ap_x * number_of_ap_y, 0x0000ff);
	saveImage(test_mode_bitmap, "detector_result.png");
#endif // TEST_MODE

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
				for(jab_int32 dy=0; dy<=(number_of_ap_y-2) && flag; dy++)
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
		blk_size.x = (jab_int32)(rect[i+1].x * ap_distance_x) - (jab_int32)(rect[i].x * ap_distance_x) + 1;
		blk_size.y = (jab_int32)(rect[i+1].y * ap_distance_y) - (jab_int32)(rect[i].y * ap_distance_y) + 1;
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
		jab_bitmap* block;
/*
		if(rect[i].x == 0 && rect[i].y == 0)											//top-left block
			block = sampleSymbolwithNc(bitmap, pt, blk_size, 2 ,ch);
		else if(rect[i+1].x == number_of_ap_x - 1 && rect[i].y == 0)					//top-right block
			block = sampleSymbolwithNc(bitmap, pt, blk_size, 3 ,ch);
		else if(rect[i].x == 0 && rect[i+1].y == number_of_ap_y - 1)					//bottom-left block
			block = sampleSymbolwithNc(bitmap, pt, blk_size, 4 ,ch);
		else if(rect[i+1].x == number_of_ap_x - 1 && rect[i+1].y == number_of_ap_y - 1)	//bottom-right block
			block = sampleSymbolwithNc(bitmap, pt, blk_size, 5 ,ch);
		else														//other blocks
			block = sampleSymbolwithNc(bitmap, pt, blk_size, 6 ,ch);
*/
		block = sampleSymbol(bitmap, pt, blk_size);
		free(pt);
		if(block == NULL)
		{
			reportError("Sampling block failed");
			free(aps);
			free(matrix);
			return NULL;
		}
		//save the sampled block in the matrix
		jab_int32 start_x = (DISTANCE_TO_BORDER - 1) + rect[i].x * ap_distance_x;
		jab_int32 start_y = (DISTANCE_TO_BORDER - 1) + rect[i].y * ap_distance_y;
		if(rect[i].x == 0)
			start_x -= (DISTANCE_TO_BORDER - 1);
		if(rect[i].y == 0)
			start_y -= (DISTANCE_TO_BORDER - 1);
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

	free(aps);
	return matrix;
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
    fps = findMasterSymbol(ch, INTENSIVE_DETECT);
    if(fps == NULL)
    {
        return JAB_FAILURE;
    }

	//check if the code/symbol is mirrored
	//TODO: is it necessary? Perspective transform will correct the mirroring, won't it?
/*	jab_point fp01, fp03;
	fp01.x = fps[1].center.x - fps[0].center.x;	//fp01: vector from fp0 to fp1
	fp01.y = fps[1].center.y - fps[0].center.y;
	fp03.x = fps[3].center.x - fps[0].center.x;	//fp03: vector from fp0 to fp3
	fp03.y = fps[3].center.y - fps[0].center.y;
	if((fp01.x * fp03.y - fp01.y * fp03.x) < 0)	//fp01 x fp03 >0 clockwise, < 0 conterclockwise
	{
		//mirrored
	}
*/
    //calculate the master symbol side size
    jab_vector2d side_size = calculateSideSize(ch, fps, &master_symbol->module_size);
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
	jab_bitmap* matrix = sampleSymbol(bitmap, pt, side_size);
	free(pt);
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
		master_symbol->side_size.x = VERSION2SIZE(master_symbol->metadata.side_version.x);
		master_symbol->side_size.y = VERSION2SIZE(master_symbol->metadata.side_version.y);
		matrix = sampleSymbolByAlignmentPattern(bitmap, ch, master_symbol, (jab_alignment_pattern*)fps);
		free(fps);
		if(matrix == NULL)
		{
#if TEST_MODE
			reportError("Sampling master symbol by alignment pattern failed");
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

    //sample master symbol
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
 * @brief Preprocess the image
 * @param bitmap the image bitmap
*/
void preprocessImage(jab_bitmap* bitmap)
{
    jab_int32 bytes_per_pixel = bitmap->bits_per_pixel / 8;
    jab_int32 bytes_per_row = bitmap->width * bytes_per_pixel;

    for(jab_int32 i=0; i<bitmap->height; i++)
    {
        for(jab_int32 j=0; j<bitmap->width; j++)
        {
            jab_byte r = bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel];
			jab_byte g = bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 1];
            jab_byte b = bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 2];

            //jab_float mean = (jab_float)(r + g + b)/3.0f;

            jab_byte max = MAX(MAX(r, g), b);
            jab_byte min = MIN(MIN(r, g), b);
            jab_float h;

            //enhance magenta
            if(r == max)
			{
				if(g >= b)
				{
					h = (jab_float)(g - b) / (jab_float)(max - min) * 60.0f;
				}
				else if(g < b)
				{
					h = (jab_float)(g - b) / (jab_float)(max - min) * 60.0f + 360;
				}
				//set both magenta and red as magenta
				if((h < 30) || (h > 270 && h < 360))
				{
					b = r;//MIN(b*1.5, 255);
					bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 2] = b;
				}
			}
            /*
            if(r == max && g == min)
            {
            	if((b-g) > (g/2))
				{
					b = MIN(b*1.5, 255);
					bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 2] = b;
				}
            }*/
            //enhance green
            /*if(g == max || b == max)
			{
				if(g == max)
				{
					h = (jab_float)(b - r) / (jab_float)(max - min) * 60.0f + 120;
				}
				else if(b == max)
				{
					h = (jab_float)(r - g) / (jab_float)(max - min) * 60.0f + 240;
				}
				//set both green and cyan as green
				if(h > 90 && h < 210)
				{
					g = MAX(max, g) * 1.5;
					bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 1] = g;
				}

			}
			*/
            if(g == max)
			{
				if((g-r) > (r/2) && (g-b) > (b/2))
				{
					g = MIN(g*1.5, 255);
					r = r/3;
					bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 0] = r;
					bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 1] = g;
				}
			}
			//restrain blue in yellow modules
			/*if((r == max && g >= b) || (g == max) )
			{
				if(r == max && g >= b)
				{
					h = (jab_float)(g - b) / (jab_float)(max - min) * 60.0f;
				}
				else if(g == max)
				{
					h = (jab_float)(b - r) / (jab_float)(max - min) * 60.0f + 120;
				}
				//restrain blue
				if(h > 30 && h < 90)
				{
					b >>= 2;
					bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 2] = b;
				}
			}
			//enhance blue
			if(b == max)
			{
				h = (jab_float)(r - g) / (jab_float)(max - min) * 60.0f + 240;
				if(h > 210 && h < 270)
				{
					b = MIN(b*1.5, 255);
					bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 2] = b;
				}
			}*/
        }
    }
#if TEST_MODE
    saveImage(bitmap, "new.png");
#endif // TEST_MODE
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

    //make a copy of the bitmap
	jab_bitmap* bitmap_copy = (jab_bitmap *)malloc(sizeof(jab_bitmap) + bitmap->width * bitmap->height * bitmap->channel_count * (bitmap->bits_per_channel/8));
	if(bitmap_copy == NULL)
	{
		reportError("Memory allocation for bitmap copy failed");
		return NULL;
	}
    bitmap_copy->bits_per_channel = bitmap->bits_per_channel;
    bitmap_copy->bits_per_pixel   = bitmap->bits_per_pixel;
    bitmap_copy->channel_count	  = bitmap->channel_count;
    bitmap_copy->height 		  = bitmap->height;
    bitmap_copy->width			  = bitmap->width;
    memcpy(bitmap_copy->pixel, bitmap->pixel, bitmap->width * bitmap->height * bitmap->channel_count * (bitmap->bits_per_channel/8));

    //preprocess the bitmap
    preprocessImage(bitmap_copy);

	//binarize r, g, b channels
	jab_bitmap* ch[3];
    ch[0] = binarizer(bitmap_copy, 0);
    ch[1] = binarizer(bitmap_copy, 1);
    ch[2] = binarizer(bitmap_copy, 2);
    free(bitmap_copy);

#if TEST_MODE
    test_mode_bitmap = (jab_bitmap*)malloc(sizeof(jab_bitmap) + bitmap->width * bitmap->height * bitmap->channel_count * (bitmap->bits_per_channel/8));
    test_mode_bitmap->bits_per_channel = bitmap->bits_per_channel;
    test_mode_bitmap->bits_per_pixel   = bitmap->bits_per_pixel;
    test_mode_bitmap->channel_count	  = bitmap->channel_count;
    test_mode_bitmap->height 		  = bitmap->height;
    test_mode_bitmap->width			  = bitmap->width;
    memcpy(test_mode_bitmap->pixel, bitmap->pixel, bitmap->width * bitmap->height * bitmap->channel_count * (bitmap->bits_per_channel/8));
    saveImage(ch[0], "br.png");
    saveImage(ch[1], "bg.png");
    saveImage(ch[2], "bb.png");
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
