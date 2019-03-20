/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file mask.c
 * @brief Data module masking
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jabcode.h"
#include "encoder.h"
#include "detector.h"

#define W1	100
#define W2	3
#define W3	3

/**
 * @brief Apply mask penalty rule 1
 * @param matrix the symbol matrix
 * @param width the symbol matrix width
 * @param height the symbol matrix height
 * @param color_number the number of module colors
 * @return the penalty score
*/
jab_int32 applyRule1(jab_int32* matrix, jab_int32 width, jab_int32 height, jab_int32 color_number)
{
	jab_byte fp0_c1, fp0_c2;
	jab_byte fp1_c1, fp1_c2;
	jab_byte fp2_c1, fp2_c2;
	jab_byte fp3_c1, fp3_c2;
	if(color_number == 2)                            //two colors: black(000) white(111)
	{
		fp0_c1 = 0;	fp0_c2 = 1;
		fp1_c1 = 1;	fp1_c2 = 0;
		fp2_c1 = 1;	fp2_c2 = 0;
		fp3_c1 = 1;	fp3_c2 = 0;
	}
	else if(color_number == 4)
	{
		fp0_c1 = 0;	fp0_c2 = 3;
		fp1_c1 = 1;	fp1_c2 = 2;
		fp2_c1 = 2;	fp2_c2 = 1;
		fp3_c1 = 3;	fp3_c2 = 0;
	}
	else
	{
		fp0_c1 = FP0_CORE_COLOR;	fp0_c2 = 7 - FP0_CORE_COLOR;
		fp1_c1 = FP1_CORE_COLOR;	fp1_c2 = 7 - FP1_CORE_COLOR;
		fp2_c1 = FP2_CORE_COLOR;	fp2_c2 = 7 - FP2_CORE_COLOR;
		fp3_c1 = FP3_CORE_COLOR;	fp3_c2 = 7 - FP3_CORE_COLOR;
	}

	jab_int32 score = 0;
	for(jab_int32 i=0; i<height; i++)
	{
		for(jab_int32 j=0; j<width; j++)
		{
/*			//horizontal check
			if(j + 4 < width)
			{
				if(matrix[i * width + j] 	 == fp0_c1 &&	//finder pattern 0
				   matrix[i * width + j + 1] == fp0_c2 &&
				   matrix[i * width + j + 2] == fp0_c1 &&
				   matrix[i * width + j + 3] == fp0_c2 &&
				   matrix[i * width + j + 4] == fp0_c1)
				   score++;
				else if(									//finder pattern 1
				   matrix[i * width + j] 	 == fp1_c1 &&
				   matrix[i * width + j + 1] == fp1_c2 &&
				   matrix[i * width + j + 2] == fp1_c1 &&
				   matrix[i * width + j + 3] == fp1_c2 &&
				   matrix[i * width + j + 4] == fp1_c1)
				   score++;
				else if(									//finder pattern 2
				   matrix[i * width + j] 	 == fp2_c1 &&
				   matrix[i * width + j + 1] == fp2_c2 &&
				   matrix[i * width + j + 2] == fp2_c1 &&
				   matrix[i * width + j + 3] == fp2_c2 &&
				   matrix[i * width + j + 4] == fp2_c1)
				   score++;
				else if(									//finder pattern 3
				   matrix[i * width + j] 	 == fp3_c1 &&
				   matrix[i * width + j + 1] == fp3_c2 &&
				   matrix[i * width + j + 2] == fp3_c1 &&
				   matrix[i * width + j + 3] == fp3_c2 &&
				   matrix[i * width + j + 4] == fp3_c1)
				   score++;
			}
			//vertical check
			if(i + 4 < height)
			{
				if(matrix[i * width + j] 	   == fp0_c1 &&	//finder pattern 0
				   matrix[(i + 1) * width + j] == fp0_c2 &&
				   matrix[(i + 2) * width + j] == fp0_c1 &&
				   matrix[(i + 3) * width + j] == fp0_c2 &&
				   matrix[(i + 4) * width + j] == fp0_c1)
				   score++;
				else if(									//finder pattern 1
				   matrix[i * width + j] 	   == fp1_c1 &&
				   matrix[(i + 1) * width + j] == fp1_c2 &&
				   matrix[(i + 2) * width + j] == fp1_c1 &&
				   matrix[(i + 3) * width + j] == fp1_c2 &&
				   matrix[(i + 4) * width + j] == fp1_c1)
				   score++;
				else if(									//finder pattern 2
				   matrix[i * width + j] 	   == fp2_c1 &&
				   matrix[(i + 1) * width + j] == fp2_c2 &&
				   matrix[(i + 2) * width + j] == fp2_c1 &&
				   matrix[(i + 3) * width + j] == fp2_c2 &&
				   matrix[(i + 4) * width + j] == fp2_c1)
				   score++;
				else if(									//finder pattern 3
				   matrix[i * width + j] 	   == fp3_c1 &&
				   matrix[(i + 1) * width + j] == fp3_c2 &&
				   matrix[(i + 2) * width + j] == fp3_c1 &&
				   matrix[(i + 3) * width + j] == fp3_c2 &&
				   matrix[(i + 4) * width + j] == fp3_c1)
				   score++;
			}
*/
			if(j >= 2 && j <= width - 3 && i >= 2 && i <= height - 3)
			{
				if(matrix[i * width + j - 2] == fp0_c1 &&	//finder pattern 0
				   matrix[i * width + j - 1] == fp0_c2 &&
				   matrix[i * width + j    ] == fp0_c1 &&
				   matrix[i * width + j + 1] == fp0_c2 &&
				   matrix[i * width + j + 2] == fp0_c1 &&
				   matrix[(i - 2) * width + j] == fp0_c1 &&
				   matrix[(i - 1) * width + j] == fp0_c2 &&
				   matrix[(i    ) * width + j] == fp0_c1 &&
				   matrix[(i + 1) * width + j] == fp0_c2 &&
				   matrix[(i + 2) * width + j] == fp0_c1)
				   score++;
				else if(
				   matrix[i * width + j - 2] == fp1_c1 &&	//finder pattern 1
				   matrix[i * width + j - 1] == fp1_c2 &&
				   matrix[i * width + j    ] == fp1_c1 &&
				   matrix[i * width + j + 1] == fp1_c2 &&
				   matrix[i * width + j + 2] == fp1_c1 &&
				   matrix[(i - 2) * width + j] == fp1_c1 &&
				   matrix[(i - 1) * width + j] == fp1_c2 &&
				   matrix[(i    ) * width + j] == fp1_c1 &&
				   matrix[(i + 1) * width + j] == fp1_c2 &&
				   matrix[(i + 2) * width + j] == fp1_c1)
				   score++;
				else if(
				   matrix[i * width + j - 2] == fp2_c1 &&	//finder pattern 2
				   matrix[i * width + j - 1] == fp2_c2 &&
				   matrix[i * width + j    ] == fp2_c1 &&
				   matrix[i * width + j + 1] == fp2_c2 &&
				   matrix[i * width + j + 2] == fp2_c1 &&
				   matrix[(i - 2) * width + j] == fp2_c1 &&
				   matrix[(i - 1) * width + j] == fp2_c2 &&
				   matrix[(i    ) * width + j] == fp2_c1 &&
				   matrix[(i + 1) * width + j] == fp2_c2 &&
				   matrix[(i + 2) * width + j] == fp2_c1)
				   score++;
				else if(
				   matrix[i * width + j - 2] == fp3_c1 &&	//finder pattern 3
				   matrix[i * width + j - 1] == fp3_c2 &&
				   matrix[i * width + j    ] == fp3_c1 &&
				   matrix[i * width + j + 1] == fp3_c2 &&
				   matrix[i * width + j + 2] == fp3_c1 &&
				   matrix[(i - 2) * width + j] == fp3_c1 &&
				   matrix[(i - 1) * width + j] == fp3_c2 &&
				   matrix[(i    ) * width + j] == fp3_c1 &&
				   matrix[(i + 1) * width + j] == fp3_c2 &&
				   matrix[(i + 2) * width + j] == fp3_c1)
				   score++;
			}
		}
	}
	return W1 * score;
}

/**
 * @brief Apply mask penalty rule 2
 * @param matrix the symbol matrix
 * @param width the symbol matrix width
 * @param height the symbol matrix height
 * @return the penalty score
*/
jab_int32 applyRule2(jab_int32* matrix, jab_int32 width, jab_int32 height)
{
	jab_int32 score = 0;
	for(jab_int32 i=0; i<height-1; i++)
	{
		for(jab_int32 j=0; j<width-1; j++)
		{
			if(matrix[i * width + j] != -1 && matrix[i * width + (j + 1)] != -1 &&
			   matrix[(i + 1) * width + j] != -1 && matrix[(i + 1) * width + (j + 1)] != -1)
			{
				if(matrix[i * width + j] == matrix[i * width + (j + 1)] &&
				   matrix[i * width + j] == matrix[(i + 1) * width + j] &&
				   matrix[i * width + j] == matrix[(i + 1) * width + (j + 1)])
				   score++;
			}
		}
	}
	return W2 * score;
}

/**
 * @brief Apply mask penalty rule 3
 * @param matrix the symbol matrix
 * @param width the symbol matrix width
 * @param height the symbol matrix height
 * @return the penalty score
*/
jab_int32 applyRule3(jab_int32* matrix, jab_int32 width, jab_int32 height)
{
	jab_int32 score = 0;
	for(jab_int32 k=0; k<2; k++)
	{
		jab_int32 maxi, maxj;
		if(k == 0)	//horizontal scan
		{
			maxi = height;
			maxj = width;
		}
		else		//vertical scan
		{
			maxi = width;
			maxj = height;
		}
		for(jab_int32 i=0; i<maxi; i++)
		{
			jab_int32 same_color_count = 0;
			jab_int32 pre_color = -1;
			for(jab_int32 j=0; j<maxj; j++)
			{
				jab_int32 cur_color = ( k == 0 ? matrix[i * width + j] : matrix[j * width + i] );
				if(cur_color != -1)
				{
					if(cur_color == pre_color)
						same_color_count++;
					else
					{
						if(same_color_count >= 5)
							score += W3 + (same_color_count - 5);
						same_color_count = 1;
						pre_color = cur_color;
					}
				}
				else
				{
					if(same_color_count >= 5)
						score += W3 + (same_color_count - 5);
					same_color_count = 0;
					pre_color = -1;
				}
			}
			if(same_color_count >= 5)
				score += W3 + (same_color_count - 5);
		}
	}
	return score;
}

/**
 * @brief Evaluate masking results
 * @param matrix the symbol matrix
 * @param width the symbol matrix width
 * @param height the symbol matrix height
 * @param color_number the number of module colors
 * @return the penalty score
*/
jab_int32 evaluateMask(jab_int32* matrix, jab_int32 width, jab_int32 height, jab_int32 color_number)
{
	return applyRule1(matrix, width, height, color_number) + applyRule2(matrix, width, height) + applyRule3(matrix, width, height);
}

/**
 * @brief Mask the data modules in symbols
 * @param enc the encode parameters
 * @param mask_type the mask pattern reference
 * @param masked the masked symbol matrix
 * @param cp the code parameters
*/
void maskSymbols(jab_encode* enc, jab_int32 mask_type, jab_int32* masked, jab_code* cp)
{
	for(jab_int32 k=0; k<enc->symbol_number; k++)
	{
		jab_int32 startx = 0, starty = 0;
		if(masked && cp)
		{
			//calculate the starting coordinates of the symbol matrix
			jab_int32 col = jab_symbol_pos[enc->symbol_positions[k]].x - cp->min_x;
			jab_int32 row = jab_symbol_pos[enc->symbol_positions[k]].y - cp->min_y;
			for(jab_int32 c=0; c<col; c++)
				startx += cp->col_width[c];
			for(jab_int32 r=0; r<row; r++)
				starty += cp->row_height[r];
		}
		jab_int32 symbol_width = enc->symbols[k].side_size.x;
		jab_int32 symbol_height= enc->symbols[k].side_size.y;

        //apply mask on the symbol
		for(jab_int32 y=0; y<symbol_height; y++)
		{
			for(jab_int32 x=0; x<symbol_width; x++)
			{
				jab_int32 index = enc->symbols[k].matrix[y * symbol_width + x];
				if(enc->symbols[k].data_map[y * symbol_width + x])
				{
					switch(mask_type)
					{
						case 0:
                            index ^= (x + y) % enc->color_number;
							break;
						case 1:
                            index ^= x % enc->color_number;
							break;
						case 2:
                            index ^= y % enc->color_number;
							break;
						case 3:
                            index ^= (x / 2 + y / 3) % enc->color_number;
							break;
						case 4:
                            index ^= (x / 3 + y / 2) % enc->color_number;
							break;
						case 5:
                            index ^= ((x + y) / 2 + (x + y) / 3) % enc->color_number;
							break;
						case 6:
                            index ^= ((x*x * y) % 7 + (2*x*x + 2*y) % 19) % enc->color_number;
							break;
						case 7:
                            index ^= ((x * y*y) % 5 + (2*x + y*y) % 13) % enc->color_number;
							break;
					}
					if(masked && cp)
						masked[(y + starty) * cp->code_size.x + (x + startx)] = index;
					else
						enc->symbols[k].matrix[y * symbol_width + x] = (jab_byte)index;
				}
				else
				{
                    if(masked && cp)
                        masked[(y + starty) * cp->code_size.x + (x + startx)] = index; //copy non-data module
				}
			}
		}
	}
}

/**
 * @brief Mask modules
 * @param enc the encode parameters
 * @param cp the code parameters
 * @return the mask pattern reference | -1 if fails
*/
jab_int32 maskCode(jab_encode* enc, jab_code* cp)
{
	jab_int32 mask_type = 0;
	jab_int32 min_penalty_score = 10000;

	//allocate memory for masked code
    jab_int32* masked = (jab_int32 *)malloc(cp->code_size.x * cp->code_size.y * sizeof(jab_int32));
    if(masked == NULL)
    {
        reportError("Memory allocation for masked code failed");
        return -1;
    }
	memset(masked, -1, cp->code_size.x * cp->code_size.y * sizeof(jab_int32)); //set all bytes in masked as 0xFF

	//evaluate each mask pattern
	for(jab_int32 t=0; t<NUMBER_OF_MASK_PATTERNS; t++)
	{
		jab_int32 penalty_score = 0;
		maskSymbols(enc, t, masked, cp);
		//calculate the penalty score
		penalty_score = evaluateMask(masked, cp->code_size.x, cp->code_size.y, enc->color_number);
#if TEST_MODE
		//JAB_REPORT_INFO(("Penalty score: %d", penalty_score))
#endif
		if(penalty_score < min_penalty_score)
		{
            mask_type = t;
			min_penalty_score = penalty_score;
		}
	}

	//mask all symbols with the selected mask pattern
	maskSymbols(enc, mask_type, 0, 0);

    //clean memory
    free(masked);
	return mask_type;
}

/**
 * @brief Demask modules
 * @param data the decoded data module values
 * @param data_map the data module positions
 * @param symbol_size the symbol size in module
 * @param mask_type the mask pattern reference
 * @param color_number the number of module colors
*/
void demaskSymbol(jab_data* data, jab_byte* data_map, jab_vector2d symbol_size, jab_int32 mask_type, jab_int32 color_number)
{
	jab_int32 symbol_width = symbol_size.x;
	jab_int32 symbol_height= symbol_size.y;
    jab_int32 count = 0;
	for(jab_int32 x=0; x<symbol_width; x++)
	{
		for(jab_int32 y=0; y<symbol_height; y++)
		{
			if(data_map[y * symbol_width + x] == 0)
			{
				if(count > data->length -1) return;
				jab_int32 index = data->data[count];
				switch(mask_type)
				{
					case 0:
                        index ^= (x + y) % color_number;
						break;
					case 1:
                        index ^= x % color_number;
						break;
					case 2:
                        index ^= y % color_number;
						break;
					case 3:
                        index ^= (x / 2 + y / 3) % color_number;
						break;
					case 4:
                        index ^= (x / 3 + y / 2) % color_number;
						break;
					case 5:
                        index ^= ((x + y) / 2 + (x + y) / 3) % color_number;
						break;
					case 6:
                        index ^= ((x*x * y) % 7 + (2*x*x + 2*y) % 19) % color_number;
						break;
					case 7:
                        index ^= ((x * y*y) % 5 + (2*x + y*y) % 13) % color_number;
						break;
				}
				data->data[count] = (jab_char)index;
				count++;
			}
		}
	}
}
