/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file encoder.c
 * @brief Symbol encoding
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jabcode.h"
#include "encoder.h"
#include "ldpc.h"
#include "detector.h"
#include "decoder.h"

/**
 * @brief Generate color palettes with more than 8 colors
 * @param color_number the number of colors
 * @param palette the color palette
*/
void genColorPalette(jab_int32 color_number, jab_byte* palette)
{
	if(color_number < 8)
		return ;

	jab_int32 vr, vg, vb;	//the number of variable colors for r, g, b channels
	switch(color_number)
	{
	case 16:
		vr = 4;
		vg = 2;
		vb = 2;
		break;
	case 32:
		vr = 4;
		vg = 4;
		vb = 2;
		break;
	case 64:
		vr = 4;
		vg = 4;
		vb = 4;
		break;
	case 128:
		vr = 8;
		vg = 4;
		vb = 4;
		break;
	case 256:
		vr = 8;
		vg = 8;
		vb = 4;
		break;
	default:
		return;
	}

	jab_float dr, dg, db;	//the pixel value interval for r, g, b channels
	dr = (vr - 1) == 3 ? 85 : 256 / (jab_float)(vr - 1);
	dg = (vg - 1) == 3 ? 85 : 256 / (jab_float)(vg - 1);
	db = (vb - 1) == 3 ? 85 : 256 / (jab_float)(vb - 1);

	jab_int32 r, g, b;		//pixel value
	jab_int32 index = 0;	//palette index
	for(jab_int32 i=0; i<vr; i++)
	{
		r = MIN((jab_int32)(dr * i), 255);
		for(jab_int32 j=0; j<vg; j++)
		{
			g = MIN((jab_int32)(dg * j), 255);
			for(jab_int32 k=0; k<vb; k++)
			{
				b = MIN((jab_int32)(db * k), 255);
				palette[index++] = (jab_byte)r;
				palette[index++] = (jab_byte)g;
				palette[index++] = (jab_byte)b;
			}
		}
	}
}

/**
 * @brief Set default color palette
 * @param color_number the number of colors
 * @param palette the color palette
 */
void setDefaultPalette(jab_int32 color_number, jab_byte* palette)
{
    if(color_number == 4)
    {
    	memcpy(palette + 0, jab_default_palette + FP0_CORE_COLOR * 3, 3);	//black   000 for 00
    	memcpy(palette + 3, jab_default_palette + 5 * 3, 3);				//magenta 101 for 01
    	memcpy(palette + 6, jab_default_palette + FP2_CORE_COLOR * 3, 3);	//yellow  110 for 10
    	memcpy(palette + 9, jab_default_palette + FP3_CORE_COLOR * 3, 3);	//cyan    011 for 11
    }
    else if(color_number == 8)
    {
        for(jab_int32 i=0; i<color_number*3; i++)
        {
            palette[i] = jab_default_palette[i];
        }
    }
    else
    {
    	genColorPalette(color_number, palette);
    }
}

/**
 * @brief Set default error correction levels
 * @param symbol_number the number of symbols
 * @param ecc_levels the ecc_level for each symbol
 */
void setDefaultEccLevels(jab_int32 symbol_number, jab_byte* ecc_levels)
{
    memset(ecc_levels, 0, symbol_number*sizeof(jab_byte));
}

/**
 * @brief Swap two integer elements
 * @param a the first element
 * @param b the second element
 */
void swap_int(jab_int32 * a, jab_int32 * b)
{
    jab_int32 temp=*a;
    *a=*b;
    *b=temp;
}

/**
 * @brief Swap two byte elements
 * @param a the first element
 * @param b the second element
 */
void swap_byte(jab_byte * a, jab_byte * b)
{
    jab_byte temp=*a;
    *a=*b;
    *b=temp;
}

/**
 * @brief Convert decimal to binary
 * @param dec the decimal value
 * @param bin the data in binary representation
 * @param start_position the position to write in encoded data array
 * @param length the length of the converted binary sequence
 */
void convert_dec_to_bin(jab_int32 dec, jab_char* bin, jab_int32 start_position, jab_int32 length)
{
    if(dec < 0) dec += 256;
    for (jab_int32 j=0; j<length; j++)
    {
        jab_int32 t = dec % 2;
        bin[start_position+length-1-j] = (jab_char)t;
        dec /= 2;
    }
}

/**
 * @brief Create encode object
 * @param color_number the number of module colors
 * @param symbol_number the number of symbols
 * @return the created encode parameter object | NULL: fatal error (out of memory)
 */
jab_encode* createEncode(jab_int32 color_number, jab_int32 symbol_number)
{
    jab_encode *enc;
    enc = (jab_encode *)calloc(1, sizeof(jab_encode));
    if(enc == NULL)
        return NULL;

    if(color_number != 4  && color_number != 8   && color_number != 16 &&
       color_number != 32 && color_number != 64 && color_number != 128 && color_number != 256)
    {
        color_number = DEFAULT_COLOR_NUMBER;
    }
    if(symbol_number < 1 || symbol_number > MAX_SYMBOL_NUMBER)
        symbol_number = DEFAULT_SYMBOL_NUMBER;

    enc->color_number 		 = color_number;
    enc->symbol_number 		 = symbol_number;
    enc->master_symbol_width = 0;
    enc->master_symbol_height= 0;
    enc->module_size 		 = DEFAULT_MODULE_SIZE;

    //set default color palette
	enc->palette = (jab_byte *)calloc(color_number * 3, sizeof(jab_byte));
    if(enc->palette == NULL)
    {
        reportError("Memory allocation for palette failed");
        return NULL;
    }
    setDefaultPalette(enc->color_number, enc->palette);
    //allocate memory for symbol versions
    enc->symbol_versions = (jab_vector2d *)calloc(symbol_number, sizeof(jab_vector2d));
    if(enc->symbol_versions == NULL)
    {
        reportError("Memory allocation for symbol versions failed");
        return NULL;
    }
    //set default error correction levels
    enc->symbol_ecc_levels = (jab_byte *)calloc(symbol_number, sizeof(jab_byte));
    if(enc->symbol_ecc_levels == NULL)
    {
        reportError("Memory allocation for ecc levels failed");
        return NULL;
    }
    setDefaultEccLevels(enc->symbol_number, enc->symbol_ecc_levels);
    //allocate memory for symbol positions
    enc->symbol_positions= (jab_int32 *)calloc(symbol_number, sizeof(jab_int32));
    if(enc->symbol_positions == NULL)
    {
        reportError("Memory allocation for symbol positions failed");
        return NULL;
    }
    //allocate memory for symbols
    enc->symbols = (jab_symbol *)calloc(symbol_number, sizeof(jab_symbol));
    if(enc->symbols == NULL)
    {
        reportError("Memory allocation for symbols failed");
        return NULL;
    }
    return enc;
}

/**
 * @brief Destroy encode object
 * @param enc the encode object
 */
void destroyEncode(jab_encode* enc)
{
    free(enc->palette);
    free(enc->symbol_versions);
    free(enc->symbol_ecc_levels);
    free(enc->symbol_positions);
    free(enc->bitmap);
    if(enc->symbols)
    {
        for(jab_int32 i=0; i<enc->symbol_number; i++)
        {
        	free(enc->symbols[i].data);
            free(enc->symbols[i].data_map);
            free(enc->symbols[i].metadata);
            free(enc->symbols[i].matrix);
        }
        free(enc->symbols);
    }
    free(enc);
}

/**
 * @brief Analyze the input data and determine the optimal encoding modes for each character
 * @param input the input character data
 * @param encoded_length the shortest encoding length
 * @return the optimal encoding sequence | NULL: fatal error (out of memory)
 */
jab_int32* analyzeInputData(jab_data* input, jab_int32* encoded_length)
{
    jab_int32 encode_seq_length=ENC_MAX;
    jab_char* seq = (jab_char *)malloc(sizeof(jab_char)*input->length);
    if(seq == NULL) {
        reportError("Memory allocation for sequence failed");
        return NULL;
    }
    jab_int32* curr_seq_len=(jab_int32 *)malloc(sizeof(jab_int32)*((input->length+2)*14));
    if(curr_seq_len == NULL){
        reportError("Memory allocation for current sequence length failed");
        free(seq);
        return NULL;
    }
    jab_int32* prev_mode=(jab_int32 *)malloc(sizeof(jab_int32)*(2*input->length+2)*14);
    if(prev_mode == NULL){
        reportError("Memory allocation for previous mode failed");
        free(seq);
        free(curr_seq_len);
        return NULL;
    }
    for (jab_int32 i=0; i < (2*input->length+2)*14; i++)
        prev_mode[i] = ENC_MAX/2;

    jab_int32* switch_mode = (jab_int32 *)malloc(sizeof(jab_int32) * 28);
    if(switch_mode == NULL){
        reportError("Memory allocation for mode switch failed");
        free(seq);
        free(curr_seq_len);
        free(prev_mode);
        return NULL;
    }
    for (jab_int32 i=0; i < 28; i++)
        switch_mode[i] = ENC_MAX/2;
    jab_int32* temp_switch_mode = (jab_int32 *)malloc(sizeof(jab_int32) * 28);
    if(temp_switch_mode == NULL){
        reportError("Memory allocation for mode switch failed");
        free(seq);
        free(curr_seq_len);
        free(prev_mode);
        free(switch_mode);
        return NULL;
    }
    for (jab_int32 i=0; i < 28; i++)
        temp_switch_mode[i] = ENC_MAX/2;

    //calculate the shortest encoding sequence
    //initialize start in upper case mode; no previous mode available
    for (jab_int32 k=0;k<7;k++)
    {
        curr_seq_len[k]=curr_seq_len[k+7]=ENC_MAX;
        prev_mode[k]=prev_mode[k+7]=ENC_MAX;
    }

    curr_seq_len[0]=0;
    jab_byte jp_to_nxt_char=0, confirm=0;
    jab_int32 curr_seq_counter=0;
    jab_boolean is_shift=0;
    jab_int32 nb_char=0;
    jab_int32 end_of_loop=input->length;
    jab_int32 prev_mode_index=0;
    for (jab_int32 i=0;i<end_of_loop;i++)
    {
        jab_int32 tmp=input->data[nb_char];
        jab_int32 tmp1=0;
        if(nb_char+1 < input->length)
            tmp1=input->data[nb_char+1];
        if(tmp<0)
            tmp=256+tmp;
        if(tmp1<0)
            tmp1=256+tmp1;
        curr_seq_counter++;
        for (jab_int32 j=0;j<JAB_ENCODING_MODES;j++)
        {
            if (jab_enconing_table[tmp][j]>-1 && jab_enconing_table[tmp][j]<64) //check if character is in encoding table
                curr_seq_len[(i+1)*14+j]=curr_seq_len[(i+1)*14+j+7]=character_size[j];
            else if((jab_enconing_table[tmp][j]==-18 && tmp1==10) || (jab_enconing_table[tmp][j]<-18 && tmp1==32))//read next character to decide if encodalbe in current mode
            {
                curr_seq_len[(i+1)*14+j]=curr_seq_len[(i+1)*14+j+7]=character_size[j];
                jp_to_nxt_char=1; //jump to next character
            }
            else //not encodable in this mode
                curr_seq_len[(i+1)*14+j]=curr_seq_len[(i+1)*14+j+7]=ENC_MAX;
        }
        curr_seq_len[(i+1)*14+6]=curr_seq_len[(i+1)*14+13]=character_size[6]; //input sequence can always be encoded by byte mode
        is_shift=0;
        for (jab_int32 j=0;j<14;j++)
        {
            jab_int32 prev=-1;
            jab_int32 len=curr_seq_len[(i+1)*14+j]+curr_seq_len[i*14+j]+latch_shift_to[j][j];
            prev_mode[curr_seq_counter*14+j]=j;
            for (jab_int32 k=0;k<14;k++)
            {
                if((len>=curr_seq_len[(i+1)*14+j]+curr_seq_len[i*14+k]+latch_shift_to[k][j] && k<13) || (k==13 && prev==j))
                {
                    len=curr_seq_len[(i+1)*14+j]+curr_seq_len[i*14+k]+latch_shift_to[k][j];
                    if (temp_switch_mode[2*k]==k)
                        prev_mode[curr_seq_counter*14+j]=temp_switch_mode[2*k+1];
                    else
                        prev_mode[curr_seq_counter*14+j]=k;
                    if (k==13 && prev==j)
                        prev=-1;
                }
            }
            curr_seq_len[(i+1)*14+j]=len;
            //shift back to mode if shift is used
            if (j>6)
            {
                if ((curr_seq_len[(i+1)*14+prev_mode[curr_seq_counter*14+j]]>len ||
                    (jp_to_nxt_char==1 && curr_seq_len[(i+1)*14+prev_mode[curr_seq_counter*14+j]]+character_size[(prev_mode[curr_seq_counter*14+j])%7]>len)) &&
                     j != 13)
                {
                    jab_int32 index=prev_mode[curr_seq_counter*14+j];
                    jab_int32 loop=1;
                    while (index>6 && curr_seq_counter-loop >= 0)
                    {
                        index=prev_mode[(curr_seq_counter-loop)*14+index];
                        loop++;
                    }

                    curr_seq_len[(i+1)*14+index]=len;
                    prev_mode[(curr_seq_counter+1)*14+index]=j;
                    switch_mode[2*index]=index;
                    switch_mode[2*index+1]=j;
                    is_shift=1;
                    if(jp_to_nxt_char==1 && j==11)
                    {
                        confirm=1;
                        prev_mode_index=index;
                    }
                }
                else if ((curr_seq_len[(i+1)*14+prev_mode[curr_seq_counter*14+j]]>len ||
                        (jp_to_nxt_char==1 && curr_seq_len[(i+1)*14+prev_mode[curr_seq_counter*14+j]]+character_size[prev_mode[curr_seq_counter*14+j]%7]>len)) && j == 13 )
                   {
                       curr_seq_len[(i+1)*14+prev_mode[curr_seq_counter*14+j]]=len;
                       prev_mode[(curr_seq_counter+1)*14+prev_mode[curr_seq_counter*14+j]]=j;
                       switch_mode[2*prev_mode[curr_seq_counter*14+j]]=prev_mode[curr_seq_counter*14+j];
                       switch_mode[2*prev_mode[curr_seq_counter*14+j]+1]=j;
                       is_shift=1;
                   }
                if(j!=13)
                    curr_seq_len[(i+1)*14+j]=ENC_MAX;
                else
                    prev=prev_mode[curr_seq_counter*14+j];
            }
        }
        for (jab_int32 j=0;j<28;j++)
        {
            temp_switch_mode[j]=switch_mode[j];
            switch_mode[j]=ENC_MAX/2;
        }

        if(jp_to_nxt_char==1 && confirm==1)
        {
            for (jab_int32 j=0;j<=2*JAB_ENCODING_MODES+1;j++)
            {
                if(j != prev_mode_index)
                    curr_seq_len[(i+1)*14+j]=ENC_MAX;
            }
            nb_char++;
            end_of_loop--;

        }
        jp_to_nxt_char=0;
        confirm=0;
        nb_char++;
    }

    //pick smallest number in last step
    jab_int32 current_mode=0;
    for (jab_int32 j=0;j<=2*JAB_ENCODING_MODES+1;j++)
    {
        if (encode_seq_length>curr_seq_len[(nb_char-(input->length-end_of_loop))*14+j])
        {
            encode_seq_length=curr_seq_len[(nb_char-(input->length-end_of_loop))*14+j];
            current_mode=j;
        }
    }
    if(current_mode>6)
        is_shift=1;
    if (is_shift && temp_switch_mode[2*current_mode+1]<14)
        current_mode=temp_switch_mode[2*current_mode+1];

    jab_int32* encode_seq = (jab_int32 *)malloc(sizeof(jab_int32) * (curr_seq_counter+1+is_shift));
    if(encode_seq == NULL)
    {
        reportError("Memory allocation for encode sequence failed");
        return NULL;
    }

    //check if byte mode is used more than 15 times in sequence
    //->>length will be increased by 13
    jab_int32 counter=0;
    jab_int32 seq_len=0;
	jab_int32 modeswitch=0;
    encode_seq[curr_seq_counter]=current_mode;//prev_mode[(curr_seq_counter)*14+current_mode];//prev_mode[(curr_seq_counter+is_shift-1)*14+current_mode];
    seq_len+=character_size[encode_seq[curr_seq_counter]%7];
    for (jab_int32 i=curr_seq_counter;i>0;i--)
    {
        if (encode_seq[i]==13 || encode_seq[i]==6)
            counter++;
        else
        {
            if(counter>15)
            {
                encode_seq_length+=13;
                seq_len+=13;

				//--------------------------------
				if(counter>8207) //2^13+15
				{
					if (encode_seq[i]==0 || encode_seq[i]==1 || encode_seq[i]==7 || encode_seq[i]==8)
						modeswitch=11;
					if (encode_seq[i]==2 || encode_seq[i]==9)
						modeswitch=10;
					if (encode_seq[i]==5 || encode_seq[i]==12)
						modeswitch=12;
					jab_int32 remain_in_byte_mode=counter/8207;
					jab_int32 remain_in_byte_mode_residual=counter%8207;
					encode_seq_length+=(remain_in_byte_mode) * modeswitch;
					seq_len+=(remain_in_byte_mode) * modeswitch;
					if(remain_in_byte_mode_residual<16)
					{
						encode_seq_length+=(remain_in_byte_mode-1) * 13;
						seq_len+=(remain_in_byte_mode-1) * 13;
					}
					else
					{
						encode_seq_length+=remain_in_byte_mode * 13;
						seq_len+=remain_in_byte_mode * 13;
					}
					if(remain_in_byte_mode_residual==0)
					{
						encode_seq_length-= modeswitch;
						seq_len-= modeswitch;
					}
				}
				//--------------------------------
				counter=0;
            }
        }
        if (encode_seq[i]<14 && i-1!=0)
        {
            encode_seq[i-1]=prev_mode[i*14+encode_seq[i]];
            seq_len+=character_size[encode_seq[i-1]%7];
            if(encode_seq[i-1]!=encode_seq[i])
                seq_len+=latch_shift_to[encode_seq[i-1]][encode_seq[i]];
        }
        else if (i-1==0)
        {
            encode_seq[i-1]=0;
            if(encode_seq[i-1]!=encode_seq[i])
                seq_len+=latch_shift_to[encode_seq[i-1]][encode_seq[i]];
            if(counter>15)
            {
                encode_seq_length+=13;
                seq_len+=13;

				//--------------------------------
				if(counter>8207) //2^13+15
				{
					modeswitch=11;
					jab_int32 remain_in_byte_mode=counter/8207;
					jab_int32 remain_in_byte_mode_residual=counter%8207;
					encode_seq_length+=remain_in_byte_mode * modeswitch;
					seq_len+=remain_in_byte_mode * modeswitch;
					if(remain_in_byte_mode_residual<16)
					{
						encode_seq_length+=(remain_in_byte_mode-1) * 13;
						seq_len+=(remain_in_byte_mode-1) * 13;
					}
					else
					{
						encode_seq_length+=remain_in_byte_mode * 13;
						seq_len+=remain_in_byte_mode * 13;
					}
					if(remain_in_byte_mode_residual==0)
					{
						encode_seq_length-= modeswitch;
						seq_len-= modeswitch;
					}
				}
				//--------------------------------
				counter=0;
            }
        }
        else
            return NULL;
    }
    *encoded_length=encode_seq_length;
    free(seq);
    free(curr_seq_len);
    free(prev_mode);
    free(switch_mode);
    free(temp_switch_mode);
    return encode_seq;
}

/**
 * @brief Check if master symbol shall be encoded in default mode
 * @param enc the encode parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean isDefaultMode(jab_encode* enc)
{
	if(enc->color_number == 8 && (enc->symbol_ecc_levels[0] == 0 || enc->symbol_ecc_levels[0] == DEFAULT_ECC_LEVEL))
	{
		return JAB_SUCCESS;
	}
	return JAB_FAILURE;
}

/**
 * @brief Calculate the (encoded) metadata length
 * @param enc the encode parameters
 * @param index the symbol index
 * @return the metadata length (encoded length for master symbol)
*/
jab_int32 getMetadataLength(jab_encode* enc, jab_int32 index)
{
    jab_int32 length = 0;

    if (index == 0) //master symbol, the encoded length
    {
    	//default mode, no metadata
    	if(isDefaultMode(enc))
		{
			length = 0;
		}
		else
		{
			//Part I
			length += MASTER_METADATA_PART1_LENGTH;
			//Part II
			length += MASTER_METADATA_PART2_LENGTH;
		}
    }
    else //slave symbol, the original net length
    {
    	//Part I
        length += 2;
        //Part II
        jab_int32 host_index = enc->symbols[index].host;
        //V in Part II, compare symbol shape and size with host symbol
        if (enc->symbol_versions[index].x != enc->symbol_versions[host_index].x || enc->symbol_versions[index].y != enc->symbol_versions[host_index].y)
		{
			length += 5;
		}
        //E in Part II
        if (enc->symbol_ecc_levels[index] != enc->symbol_ecc_levels[host_index])
        {
            length += 6;
        }
    }
    return length;
}

/**
 * @brief Calculate the data capacity of a symbol
 * @param enc the encode parameters
 * @param index the symbol index
 * @return the data capacity
 */
jab_int32 getSymbolCapacity(jab_encode* enc, jab_int32 index)
{
	//number of modules for finder patterns
    jab_int32 nb_modules_fp;
    if(index == 0)	//master symbol
	{
		nb_modules_fp = 4 * 17;
	}
	else			//slave symbol
	{
		nb_modules_fp = 4 * 7;
	}
    //number of modules for color palette
    jab_int32 nb_modules_palette = enc->color_number > 64 ? (64-2)*COLOR_PALETTE_NUMBER : (enc->color_number-2)*COLOR_PALETTE_NUMBER;
	//number of modules for alignment pattern
	jab_int32 side_size_x = VERSION2SIZE(enc->symbol_versions[index].x);
	jab_int32 side_size_y = VERSION2SIZE(enc->symbol_versions[index].y);
	jab_int32 number_of_aps_x = jab_ap_num[enc->symbol_versions[index].x - 1];
	jab_int32 number_of_aps_y = jab_ap_num[enc->symbol_versions[index].y - 1];
	jab_int32 nb_modules_ap = (number_of_aps_x * number_of_aps_y - 4) * 7;
	//number of modules for metadata
	jab_int32 nb_of_bpm = log(enc->color_number) / log(2);
	jab_int32 nb_modules_metadata = 0;
	if(index == 0)	//master symbol
	{
		jab_int32 nb_metadata_bits = getMetadataLength(enc, index);
		if(nb_metadata_bits > 0)
		{
			nb_modules_metadata = (nb_metadata_bits - MASTER_METADATA_PART1_LENGTH) / nb_of_bpm; //only modules for PartII
			if((nb_metadata_bits - MASTER_METADATA_PART1_LENGTH) % nb_of_bpm != 0)
			{
				nb_modules_metadata++;
			}
			nb_modules_metadata += MASTER_METADATA_PART1_MODULE_NUMBER; //add modules for PartI
		}
	}
	jab_int32 capacity = (side_size_x*side_size_y - nb_modules_fp - nb_modules_ap - nb_modules_palette - nb_modules_metadata) * nb_of_bpm;
	return capacity;
}

/**
 * @brief Get the optimal error correction capability
 * @param capacity the symbol capacity
 * @param net_data_length the original data length
 * @param wcwr the LPDC parameters wc and wr
 * @return JAB_SUCCESS | JAB_FAILURE
 */
void getOptimalECC(jab_int32 capacity, jab_int32 net_data_length, jab_int32* wcwr)
{
	jab_float min = capacity;
	for (jab_int32 k=3; k<=6+2; k++)
	{
		for (jab_int32 j=k+1; j<=6+3; j++)
		{
			jab_int32 dist = (capacity/j)*j - (capacity/j)*k - net_data_length; //max_gross_payload = floor(capacity / wr) * wr
			if(dist<min && dist>=0)
			{
				wcwr[1] = j;
				wcwr[0] = k;
				min = dist;
			}
		}
	}
}

/**
 * @brief Encode the input data
 * @param data the character input data
 * @param encoded_length the optimal encoding length
 * @param encode_seq the optimal encoding sequence
 * @return the encoded data | NULL if failed
 */
jab_data* encodeData(jab_data* data, jab_int32 encoded_length,jab_int32* encode_seq)
{
    jab_data* encoded_data = (jab_data *)malloc(sizeof(jab_data) + encoded_length*sizeof(jab_char));
    if(encoded_data == NULL)
    {
        reportError("Memory allocation for encoded data failed");
        return NULL;
    }
    encoded_data->length = encoded_length;

    jab_int32 counter=0;
    jab_boolean shift_back=0;
    jab_int32 position=0;
    jab_int32 current_encoded_length=0;
    jab_int32 end_of_loop=data->length;
    jab_int32 byte_offset=0;
    jab_int32 byte_counter=0;
	jab_int32 factor=1;
    //encoding starts in upper case mode
    for (jab_int32 i=0;i<end_of_loop;i++)
    {
        jab_int32 tmp=data->data[current_encoded_length];
        if (tmp < 0)
            tmp+=256;
        if (position<encoded_length)
        {
            jab_int32 decimal_value;
            //check if mode is switched
            if (encode_seq[counter] != encode_seq[counter+1])
            {
                //encode mode switch
                jab_int32 length=latch_shift_to[encode_seq[counter]][encode_seq[counter+1]];
                if(encode_seq[counter+1] == 6 || encode_seq[counter+1] == 13)
                    length-=4;
                if(length < ENC_MAX)
                    convert_dec_to_bin(mode_switch[encode_seq[counter]][encode_seq[counter+1]],encoded_data->data,position,length);
                else
                {
                    reportError("Encoding data failed");
                    free(encoded_data);
                    return NULL;
                }
                position+=latch_shift_to[encode_seq[counter]][encode_seq[counter+1]];
                if(encode_seq[counter+1] == 6 || encode_seq[counter+1] == 13)
                    position-=4;
                //check if latch or shift
                if((encode_seq[counter+1]>6 && encode_seq[counter+1]<=13) || (encode_seq[counter+1]==13 && encode_seq[counter+2]!=13))
                    shift_back=1;//remember to shift back to mode from which was invoked
            }
            //if not byte mode
            if (encode_seq[counter+1]%7 != 6)// || end_of_loop-1 == i)
            {
                if(jab_enconing_table[tmp][encode_seq[counter+1]%7]>-1 && character_size[encode_seq[counter+1]%7] < ENC_MAX)
                {
                    //encode character
                    convert_dec_to_bin(jab_enconing_table[tmp][encode_seq[counter+1]%7],encoded_data->data,position,character_size[encode_seq[counter+1]%7]);
                    position+=character_size[encode_seq[counter+1]%7];
                    counter++;
                }
                else if (jab_enconing_table[tmp][encode_seq[counter+1]%7]<-1)
                {
                    jab_int32 tmp1=data->data[current_encoded_length+1];
                    if (tmp1 < 0)
                        tmp1+=256;
                    //read next character to see if more efficient encoding possible
                    if (((tmp==44 || tmp== 46 || tmp==58) && tmp1==32) || (tmp==13 && tmp1==10))
                        decimal_value=abs(jab_enconing_table[tmp][encode_seq[counter+1]%7]);
                    else if (tmp==13 && tmp1!=10)
                        decimal_value=18;
                    else
                    {
                        reportError("Encoding data failed");
                        free(encoded_data);
                        return NULL;
                    }
                    if (character_size[encode_seq[counter+1]%7] < ENC_MAX)
                    convert_dec_to_bin(decimal_value,encoded_data->data,position,character_size[encode_seq[counter+1]%7]);
                    position+=character_size[encode_seq[counter+1]%7];
                    counter++;
                    end_of_loop--;
                    current_encoded_length++;
                }
                else
                {
                    reportError("Encoding data failed");
                    free(encoded_data);
                    return NULL;
                }
            }
            else
            {
                //byte mode
                if(encode_seq[counter] != encode_seq[counter+1])
                {
                    //loop over sequence to check how many characters in byte mode follow
                    byte_counter=0;
                    for(jab_int32 byte_loop=counter+1;byte_loop<=end_of_loop;byte_loop++)
                    {
                        if(encode_seq[byte_loop]==6 || encode_seq[byte_loop]==13)
                            byte_counter++;
                        else
                            break;
                    }
                    convert_dec_to_bin(byte_counter > 15 ? 0 : byte_counter,encoded_data->data,position,4);
                    position+=4;
                    if(byte_counter > 15)
                    {
						if(byte_counter <= 8207)//8207=2^13+15; if number of bytes exceeds 8207, encoder shall shift to byte mode again from upper case mode && byte_counter < 8207
						{
							convert_dec_to_bin(byte_counter-15-1,encoded_data->data,position,13);
						}
						else
						{
							convert_dec_to_bin(8191,encoded_data->data,position,13);
						}
                        position+=13;
                    }
                    byte_offset=byte_counter;
                }
				if(byte_offset-byte_counter==factor*8207) //byte mode exceeds 2^13 + 15
				{
					if(encode_seq[counter-(byte_offset-byte_counter)]==0 || encode_seq[counter-(byte_offset-byte_counter)]==7 || encode_seq[counter-(byte_offset-byte_counter)]==1|| encode_seq[counter-(byte_offset-byte_counter)]==8)
					{
						convert_dec_to_bin(124,encoded_data->data,position,7);// shift from upper case to byte
						position+=7;
					}
					if(encode_seq[counter-(byte_offset-byte_counter)]==2 || encode_seq[counter-(byte_offset-byte_counter)]==9)
					{
						convert_dec_to_bin(60,encoded_data->data,position,5);// shift from numeric to byte
						position+=5;
					}
					if(encode_seq[counter-(byte_offset-byte_counter)]==5 || encode_seq[counter-(byte_offset-byte_counter)]==12)
					{
						convert_dec_to_bin(252,encoded_data->data,position,8);// shift from alphanumeric to byte
						position+=8;
					}
					convert_dec_to_bin(byte_counter > 15 ? 0 : byte_counter,encoded_data->data,position,4); //write the first 4 bits
					position+=4;
					if(byte_counter > 15) //if more than 15 bytes -> use the next 13 bits to wirte the length
					{
						if(byte_counter <= 8207)//8207=2^13+15; if number of bytes exceeds 8207, encoder shall shift to byte mode again from upper case mode && byte_counter < 8207
						{
							convert_dec_to_bin(byte_counter-15-1,encoded_data->data,position,13);
						}
						else //number exceeds 2^13 + 15
						{
							convert_dec_to_bin(8191,encoded_data->data,position,13);
						}
						position+=13;
					}
					factor++;
				}
                if (character_size[encode_seq[counter+1]%7] < ENC_MAX)
                    convert_dec_to_bin(tmp,encoded_data->data,position,character_size[encode_seq[counter+1]%7]);
                else
                {
                    reportError("Encoding data failed");
                    free(encoded_data);
                    return NULL;
                }
                position+=character_size[encode_seq[counter+1]%7];
                counter++;
                byte_counter--;
            }
            //shift back to mode from which mode was invoked
            if (shift_back && byte_counter==0)
            {
                if(byte_offset==0)
                    encode_seq[counter]=encode_seq[counter-1];
                else
                    encode_seq[counter]=encode_seq[counter-byte_offset];
                shift_back=0;
                byte_offset=0;
            }

        }
        else
        {
            reportError("Encoding data failed");
            free(encoded_data);
            return NULL;
        }
        current_encoded_length++;
    }
    return encoded_data;
}

/**
 * @brief Encode metadata
 * @param enc the encode parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean encodeMasterMetadata(jab_encode* enc)
{
	jab_int32 partI_length 	= MASTER_METADATA_PART1_LENGTH/2;	//partI net length
	jab_int32 partII_length	= MASTER_METADATA_PART2_LENGTH/2;	//partII net length
	jab_int32 V_length = 10;
	jab_int32 E_length = 6;
	jab_int32 MSK_length = 3;
	//set master metadata variables
	jab_int32 Nc = log(enc->color_number)/log(2.0) - 1;
	jab_int32 V = ((enc->symbol_versions[0].x -1) << 5) + (enc->symbol_versions[0].y - 1);
	jab_int32 E1 = enc->symbols[0].wcwr[0] - 3;
	jab_int32 E2 = enc->symbols[0].wcwr[1] - 4;
	jab_int32 MSK = DEFAULT_MASKING_REFERENCE;

	//write each part of master metadata
	//Part I
	jab_data* partI = (jab_data *)malloc(sizeof(jab_data) + partI_length*sizeof(jab_char));
	if(partI == NULL)
	{
		reportError("Memory allocation for metadata Part I in master symbol failed");
		return JAB_FAILURE;
	}
	partI->length = partI_length;
	convert_dec_to_bin(Nc, partI->data, 0, partI->length);
	//Part II
	jab_data* partII = (jab_data *)malloc(sizeof(jab_data) + partII_length*sizeof(jab_char));
	if(partII == NULL)
	{
		reportError("Memory allocation for metadata Part II in master symbol failed");
		return JAB_FAILURE;
	}
	partII->length = partII_length;
	convert_dec_to_bin(V,   partII->data, 0, V_length);
	convert_dec_to_bin(E1,  partII->data, V_length, 3);
	convert_dec_to_bin(E2,  partII->data, V_length+3, 3);
	convert_dec_to_bin(MSK, partII->data, V_length+E_length, MSK_length);

	//encode each part of master metadata
	jab_int32 wcwr[2] = {2, -1};
	//Part I
	jab_data* encoded_partI   = encodeLDPC(partI, wcwr);
	if(encoded_partI == NULL)
	{
		reportError("LDPC encoding master metadata Part I failed");
		return JAB_FAILURE;
	}
	//Part II
	jab_data* encoded_partII  = encodeLDPC(partII, wcwr);
	if(encoded_partII == NULL)
	{
		reportError("LDPC encoding master metadata Part II failed");
		return JAB_FAILURE;
	}

	jab_int32 encoded_metadata_length = encoded_partI->length + encoded_partII->length;
	enc->symbols[0].metadata = (jab_data *)malloc(sizeof(jab_data) + encoded_metadata_length*sizeof(jab_char));
	if(enc->symbols[0].metadata == NULL)
	{
		reportError("Memory allocation for encoded metadata in master symbol failed");
		return JAB_FAILURE;
	}
	enc->symbols[0].metadata->length = encoded_metadata_length;
	//copy encoded parts into metadata
	memcpy(enc->symbols[0].metadata->data, encoded_partI->data, encoded_partI->length);
	memcpy(enc->symbols[0].metadata->data+encoded_partI->length, encoded_partII->data, encoded_partII->length);

	free(partI);
	free(partII);
	free(encoded_partI);
	free(encoded_partII);
    return JAB_SUCCESS;
}

/**
 * @brief Update master symbol metadata PartII if the default masking reference is changed
 * @param enc the encode parameter
 * @param mask_ref the masking reference
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean updateMasterMetadataPartII(jab_encode* enc, jab_int32 mask_ref)
{
	jab_int32 partII_length	= MASTER_METADATA_PART2_LENGTH/2;	//partII net length
	jab_data* partII = (jab_data *)malloc(sizeof(jab_data) + partII_length*sizeof(jab_char));
	if(partII == NULL)
	{
		reportError("Memory allocation for metadata Part II in master symbol failed");
		return JAB_FAILURE;
	}
	partII->length = partII_length;

	//set V and E
	jab_int32 V_length = 10;
	jab_int32 E_length = 6;
	jab_int32 MSK_length = 3;
	jab_int32 V = ((enc->symbol_versions[0].x -1) << 5) + (enc->symbol_versions[0].y - 1);
	jab_int32 E1 = enc->symbols[0].wcwr[0] - 3;
	jab_int32 E2 = enc->symbols[0].wcwr[1] - 4;
	convert_dec_to_bin(V,   partII->data, 0, V_length);
	convert_dec_to_bin(E1,  partII->data, V_length, 3);
	convert_dec_to_bin(E2,  partII->data, V_length+3, 3);

	//update masking reference in PartII
	convert_dec_to_bin(mask_ref, partII->data, V_length+E_length, MSK_length);

	//encode new PartII
	jab_int32 wcwr[2] = {2, -1};
	jab_data* encoded_partII = encodeLDPC(partII, wcwr);
	if(encoded_partII == NULL)
	{
		reportError("LDPC encoding master metadata Part II failed");
		return JAB_FAILURE;
	}
	//update metadata
	memcpy(enc->symbols[0].metadata->data+MASTER_METADATA_PART1_LENGTH, encoded_partII->data, encoded_partII->length);

	free(partII);
	free(encoded_partII);
	return JAB_SUCCESS;
}

/**
 * @brief Update master symbol metadata PartII if the default masking reference is changed
 * @param enc the encode parameter
*/
void placeMasterMetadataPartII(jab_encode* enc)
{
    //rewrite metadata in master with mask information
    jab_int32 nb_of_bits_per_mod = log(enc->color_number)/log(2);
    jab_int32 x = MASTER_METADATA_X;
    jab_int32 y = MASTER_METADATA_Y;
    jab_int32 module_count = 0;
    //skip PartI and color palette
    jab_int32 color_palette_size = MIN(enc->color_number-2, 64-2);
    jab_int32 module_offset = MASTER_METADATA_PART1_MODULE_NUMBER + color_palette_size*COLOR_PALETTE_NUMBER;
    for(jab_int32 i=0; i<module_offset; i++)
	{
		module_count++;
        getNextMetadataModuleInMaster(enc->symbols[0].side_size.y, enc->symbols[0].side_size.x, module_count, &x, &y);
	}
	//update PartII
	jab_int32 partII_bit_start = MASTER_METADATA_PART1_LENGTH;
	jab_int32 partII_bit_end = MASTER_METADATA_PART1_LENGTH + MASTER_METADATA_PART2_LENGTH;
	jab_int32 metadata_index = partII_bit_start;
	while(metadata_index <= partII_bit_end)
	{
    	jab_byte color_index = enc->symbols[0].matrix[y*enc->symbols[0].side_size.x + x];
		for(jab_int32 j=0; j<nb_of_bits_per_mod; j++)
		{
			if(metadata_index <= partII_bit_end)
			{
				jab_byte bit = enc->symbols[0].metadata->data[metadata_index];
				if(bit == 0)
					color_index &= ~(1UL << (nb_of_bits_per_mod-1-j));
				else
					color_index |= 1UL << (nb_of_bits_per_mod-1-j);
				metadata_index++;
			}
			else
				break;
		}
        enc->symbols[0].matrix[y*enc->symbols[0].side_size.x + x] = color_index;
        module_count++;
        getNextMetadataModuleInMaster(enc->symbols[0].side_size.y, enc->symbols[0].side_size.x, module_count, &x, &y);
    }
}

/**
 * @brief Get color index for the color palette
 * @param index the color index in the palette
 * @param index_size the size of index
 * @param color_number the number of colors
*/
void getColorPaletteIndex(jab_byte* index, jab_int32 index_size, jab_int32 color_number)
{
	for(jab_int32 i=0; i<index_size; i++)
	{
		index[i] = i;
	}

	if(color_number < 128)
		return;

	jab_byte tmp[color_number];
	for(jab_int32 i=0; i<color_number; i++)
	{
		tmp[i] = i;
	}

	if(color_number == 128)
	{
		memcpy(index + 0,  tmp + 0, 16);
		memcpy(index + 16, tmp + 32, 16);
		memcpy(index + 32, tmp + 80, 16);
		memcpy(index + 48, tmp + 112, 16);
	}
	else if(color_number == 256)
	{
		memcpy(index + 0, tmp + 0,  4);
		memcpy(index + 4, tmp + 8,  4);
		memcpy(index + 8, tmp + 20, 4);
		memcpy(index + 12,tmp + 28, 4);

		memcpy(index + 16, tmp + 64, 4);
		memcpy(index + 20, tmp + 72, 4);
		memcpy(index + 24, tmp + 84, 4);
		memcpy(index + 28, tmp + 92, 4);

		memcpy(index + 32, tmp + 160, 4);
		memcpy(index + 36, tmp + 168, 4);
		memcpy(index + 40, tmp + 180, 4);
		memcpy(index + 44, tmp + 188, 4);

		memcpy(index + 48, tmp + 224, 4);
		memcpy(index + 52, tmp + 232, 4);
		memcpy(index + 56, tmp + 244, 4);
		memcpy(index + 60, tmp + 252, 4);
	}
}

/**
 * @brief Create symbol matrix
 * @param enc the encode parameter
 * @param index the symbol index
 * @param ecc_encoded_data encoded data
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean createMatrix(jab_encode* enc, jab_int32 index, jab_data* ecc_encoded_data)
{
    //Allocate matrix
    enc->symbols[index].matrix = (jab_byte *)calloc(enc->symbols[index].side_size.x * enc->symbols[index].side_size.y, sizeof(jab_byte));
    if(enc->symbols[index].matrix == NULL)
    {
        reportError("Memory allocation for symbol matrix failed");
        return JAB_FAILURE;
    }
    //Allocate boolean matrix
    enc->symbols[index].data_map = (jab_byte *)malloc(enc->symbols[index].side_size.x * enc->symbols[index].side_size.y * sizeof(jab_byte));
    if(!enc->symbols[index].data_map)
    {
        reportError("Memory allocation for data map failed");
        return JAB_FAILURE;
    }
    memset(enc->symbols[index].data_map, 1, enc->symbols[index].side_size.x * enc->symbols[index].side_size.y * sizeof(jab_byte));

    //set alignment patterns
    jab_int32 Nc = log(enc->color_number)/log(2.0) - 1;
	jab_byte apx_core_color = apx_core_color_index[Nc];
	jab_byte apx_peri_color = apn_core_color_index[Nc];
	jab_int32 side_ver_x_index = SIZE2VERSION(enc->symbols[index].side_size.x) - 1;
	jab_int32 side_ver_y_index = SIZE2VERSION(enc->symbols[index].side_size.y) - 1;
    for(jab_int32 x=0; x<jab_ap_num[side_ver_x_index]; x++)
    {
    	jab_byte left;
        if (x%2 == 1)
            left=0;
        else
            left=1;
        for(jab_int32 y=0; y<jab_ap_num[side_ver_y_index]; y++)
        {
            jab_int32 x_offset = jab_ap_pos[side_ver_x_index][x] - 1;
            jab_int32 y_offset = jab_ap_pos[side_ver_y_index][y] - 1;
            //left alignment patterns
            if(	left == 1
				&& (x != 0 || y != 0)
				&& (x != 0 || y != jab_ap_num[side_ver_y_index]-1)
				&& (x != jab_ap_num[side_ver_x_index]-1 || y != 0)
				&& (x != jab_ap_num[side_ver_x_index]-1 || y != jab_ap_num[side_ver_y_index]-1))
            {
            	enc->symbols[index].matrix[(y_offset-1)*enc->symbols[index].side_size.x + x_offset-1]=
				enc->symbols[index].matrix[(y_offset-1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].matrix[(y_offset  )*enc->symbols[index].side_size.x + x_offset-1]=
				enc->symbols[index].matrix[(y_offset  )*enc->symbols[index].side_size.x + x_offset+1]=
				enc->symbols[index].matrix[(y_offset+1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].matrix[(y_offset+1)*enc->symbols[index].side_size.x + x_offset+1]=apx_peri_color;
				enc->symbols[index].matrix[(y_offset  )*enc->symbols[index].side_size.x + x_offset  ]=apx_core_color;

				enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x + x_offset-1]=
				enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].data_map[(y_offset  )*enc->symbols[index].side_size.x + x_offset-1]=
				enc->symbols[index].data_map[(y_offset  )*enc->symbols[index].side_size.x + x_offset+1]=
				enc->symbols[index].data_map[(y_offset+1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].data_map[(y_offset+1)*enc->symbols[index].side_size.x + x_offset+1]=
				enc->symbols[index].data_map[(y_offset  )*enc->symbols[index].side_size.x + x_offset  ]=0;
            }
            //right alignment patterns
            else if(left == 0
					&& (x != 0 || y != 0)
					&& (x != 0 || y != jab_ap_num[side_ver_y_index]-1)
					&& (x != jab_ap_num[side_ver_x_index]-1 || y != 0)
					&& (x != jab_ap_num[side_ver_x_index]-1 || y != jab_ap_num[side_ver_y_index]-1))
            {
            	enc->symbols[index].matrix[(y_offset-1)*enc->symbols[index].side_size.x + x_offset+1]=
				enc->symbols[index].matrix[(y_offset-1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].matrix[(y_offset  )*enc->symbols[index].side_size.x + x_offset-1]=
				enc->symbols[index].matrix[(y_offset  )*enc->symbols[index].side_size.x + x_offset+1]=
				enc->symbols[index].matrix[(y_offset+1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].matrix[(y_offset+1)*enc->symbols[index].side_size.x + x_offset-1]=apx_peri_color;
				enc->symbols[index].matrix[(y_offset  )*enc->symbols[index].side_size.x + x_offset  ]=apx_core_color;

				enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x + x_offset+1]=
				enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].data_map[(y_offset  )*enc->symbols[index].side_size.x + x_offset-1]=
				enc->symbols[index].data_map[(y_offset  )*enc->symbols[index].side_size.x + x_offset+1]=
				enc->symbols[index].data_map[(y_offset+1)*enc->symbols[index].side_size.x + x_offset  ]=
				enc->symbols[index].data_map[(y_offset+1)*enc->symbols[index].side_size.x + x_offset-1]=
				enc->symbols[index].data_map[(y_offset  )*enc->symbols[index].side_size.x + x_offset  ]=0;
            }
            if (left==0)
                left=1;
            else
                left=0;
        }
    }

    //outer layers of finder pattern for master symbol
    if(index == 0)
    {
        //if k=0 center, k=1 first layer, k=2 second layer
        for(jab_int32 k=0;k<3;k++)
        {
            for(jab_int32 i=0;i<k+1;i++)
            {
                for(jab_int32 j=0;j<k+1;j++)
                {
                    if (i==k || j==k)
                    {
                        jab_byte fp0_color_index, fp1_color_index, fp2_color_index, fp3_color_index;
						fp0_color_index = (k%2) ? fp3_core_color_index[Nc] : fp0_core_color_index[Nc];
						fp1_color_index = (k%2) ? fp2_core_color_index[Nc] : fp1_core_color_index[Nc];
						fp2_color_index = (k%2) ? fp1_core_color_index[Nc] : fp2_core_color_index[Nc];
						fp3_color_index = (k%2) ? fp0_core_color_index[Nc] : fp3_core_color_index[Nc];

						//upper pattern
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
						enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=fp0_color_index;
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=0;

                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=fp1_color_index;
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;

                        //lower pattern
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=fp2_color_index;
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;

                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)-j-1]=
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)+j-1]=fp3_color_index;
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)-j-1]=
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)+j-1]=0;
                    }
                }
            }
        }
    }
    else //finder alignments in slave
    {
        //if k=0 center, k=1 first layer
        for(jab_int32 k=0;k<2;k++)
        {
            for(jab_int32 i=0;i<k+1;i++)
            {
                for(jab_int32 j=0;j<k+1;j++)
                {
                    if (i==k || j==k)
                    {
                    	jab_byte ap0_color_index, ap1_color_index, ap2_color_index, ap3_color_index;
                        ap0_color_index =
						ap1_color_index =
						ap2_color_index =
						ap3_color_index = (k%2) ? apx_core_color_index[Nc] : apn_core_color_index[Nc];
                        //upper pattern
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=ap0_color_index;
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=0;

                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=ap1_color_index;
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;

                        //lower pattern
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=ap2_color_index;
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
						enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;

                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)-j-1]=
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)+j-1]=ap3_color_index;
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)-j-1]=
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)+j-1]=0;
                    }
                }
            }
        }
    }

    //Metadata and color palette placement
    jab_int32 nb_of_bits_per_mod = log(enc->color_number) / log(2);
    jab_int32 color_index;
    jab_int32 module_count = 0;
    jab_int32 x;
    jab_int32 y;

    //get color index for color palette
    jab_int32 palette_index_size = enc->color_number > 64 ? 64 : enc->color_number;
	jab_byte palette_index[palette_index_size];
	getColorPaletteIndex(palette_index, palette_index_size, enc->color_number);

    if(index == 0)//place metadata and color palette in master symbol
    {
        x = MASTER_METADATA_X;
        y = MASTER_METADATA_Y;
		int metadata_index = 0;
        //metadata Part I
        if(!isDefaultMode(enc))
		{
			while(metadata_index < enc->symbols[index].metadata->length && metadata_index < MASTER_METADATA_PART1_LENGTH)
			{
				//Read 3 bits from encoded PartI each time
				jab_byte bit1 = enc->symbols[index].metadata->data[metadata_index + 0];
				jab_byte bit2 = enc->symbols[index].metadata->data[metadata_index + 1];
				jab_byte bit3 = enc->symbols[index].metadata->data[metadata_index + 2];
				jab_int32 val = (bit1 << 2) + (bit2 << 1) + bit3;
				//place two modules according to the value of every 3 bits
                for(jab_int32 i=0; i<2; i++)
				{
					color_index = nc_color_encode_table[val][i] % enc->color_number;
					enc->symbols[index].matrix  [y*enc->symbols[index].side_size.x + x] = color_index;
					enc->symbols[index].data_map[y*enc->symbols[index].side_size.x + x] = 0;
					module_count++;
					getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
				}
				metadata_index += 3;
			}
		}
		//color palette
		for(jab_int32 i=2; i<MIN(enc->color_number, 64); i++)	//skip the first two colors in finder pattern
		{
			enc->symbols[index].matrix  [y*enc->symbols[index].side_size.x+x] = palette_index[master_palette_placement_index[0][i]%enc->color_number];
			enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x] = 0;
			module_count++;
			getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);

			enc->symbols[index].matrix  [y*enc->symbols[index].side_size.x+x] = palette_index[master_palette_placement_index[1][i]%enc->color_number];
			enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x] = 0;
			module_count++;
			getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);

			enc->symbols[index].matrix  [y*enc->symbols[index].side_size.x+x] = palette_index[master_palette_placement_index[2][i]%enc->color_number];
			enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x] = 0;
			module_count++;
			getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);

			enc->symbols[index].matrix  [y*enc->symbols[index].side_size.x+x] = palette_index[master_palette_placement_index[3][i]%enc->color_number];
			enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x] = 0;
			module_count++;
			getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
		}
		//metadata PartII
		if(!isDefaultMode(enc))
		{
			while(metadata_index < enc->symbols[index].metadata->length)
			{
				color_index = 0;
				for(jab_int32 j=0; j<nb_of_bits_per_mod; j++)
				{
					if(metadata_index < enc->symbols[index].metadata->length)
					{
						color_index += ((jab_int32)enc->symbols[index].metadata->data[metadata_index]) << (nb_of_bits_per_mod-1-j);
						metadata_index++;
					}
					else
						break;
				}
				enc->symbols[index].matrix  [y*enc->symbols[index].side_size.x + x] = color_index;
				enc->symbols[index].data_map[y*enc->symbols[index].side_size.x + x] = 0;
				module_count++;
				getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
			}
		}
    }
    else//place color palette in slave symbol
    {
    	//color palette
        jab_int32 width=enc->symbols[index].side_size.x;
        jab_int32 height=enc->symbols[index].side_size.y;
        for (jab_int32 i=2; i<MIN(enc->color_number, 64); i++)	//skip the first two colors in alignment pattern
        {
        	//left
			enc->symbols[index].matrix  [slave_palette_position[i-2].y*width + slave_palette_position[i-2].x] = palette_index[slave_palette_placement_index[i]%enc->color_number];
			enc->symbols[index].data_map[slave_palette_position[i-2].y*width + slave_palette_position[i-2].x] = 0;
			//top
			enc->symbols[index].matrix  [slave_palette_position[i-2].x*width + (width-1-slave_palette_position[i-2].y)] = palette_index[slave_palette_placement_index[i]%enc->color_number];
			enc->symbols[index].data_map[slave_palette_position[i-2].x*width + (width-1-slave_palette_position[i-2].y)] = 0;
			//right
			enc->symbols[index].matrix  [(height-1-slave_palette_position[i-2].y)*width + (width-1-slave_palette_position[i-2].x)] = palette_index[slave_palette_placement_index[i]%enc->color_number];
			enc->symbols[index].data_map[(height-1-slave_palette_position[i-2].y)*width + (width-1-slave_palette_position[i-2].x)] = 0;
			//bottom
			enc->symbols[index].matrix  [(height-1-slave_palette_position[i-2].x)*width + slave_palette_position[i-2].y] = palette_index[slave_palette_placement_index[i]%enc->color_number];
			enc->symbols[index].data_map[(height-1-slave_palette_position[i-2].x)*width + slave_palette_position[i-2].y] = 0;
        }
    }
#if TEST_MODE
	FILE* fp = fopen("jab_enc_module_data.bin", "wb");
#endif // TEST_MODE
    //Data placement
    jab_int32 written_mess_part=0;
    jab_int32 padding=0;
    for(jab_int32 start_i=0;start_i<enc->symbols[index].side_size.x;start_i++)
    {
        for(jab_int32 i=start_i;i<enc->symbols[index].side_size.x*enc->symbols[index].side_size.y;i=i+enc->symbols[index].side_size.x)
        {
            if (enc->symbols[index].data_map[i]!=0 && written_mess_part<ecc_encoded_data->length)
            {
                color_index=0;
                for(jab_int32 j=0;j<nb_of_bits_per_mod;j++)
                {
                    if(written_mess_part<ecc_encoded_data->length)
                        color_index+=((jab_int32)ecc_encoded_data->data[written_mess_part]) << (nb_of_bits_per_mod-1-j);//*pow(2,nb_of_bits_per_mod-1-j);
                    else
                    {
                        color_index+=padding << (nb_of_bits_per_mod-1-j);//*pow(2,nb_of_bits_per_mod-1-j);
                        if (padding==0)
                            padding=1;
                        else
                            padding=0;
                    }
                    written_mess_part++;
                }
                enc->symbols[index].matrix[i]=(jab_char)color_index;//i % enc->color_number;
#if TEST_MODE
				fwrite(&enc->symbols[index].matrix[i], 1, 1, fp);
#endif // TEST_MODE
            }
            else if(enc->symbols[index].data_map[i]!=0) //write padding bits
            {
                color_index=0;
                for(jab_int32 j=0;j<nb_of_bits_per_mod;j++)
                {
                    color_index+=padding << (nb_of_bits_per_mod-1-j);//*pow(2,nb_of_bits_per_mod-1-j);
                    if (padding==0)
                        padding=1;
                    else
                        padding=0;
                }
                enc->symbols[index].matrix[i]=(jab_char)color_index;//i % enc->color_number;
#if TEST_MODE
				fwrite(&enc->symbols[index].matrix[i], 1, 1, fp);
#endif // TEST_MODE
            }
        }
    }
#if TEST_MODE
	fclose(fp);
#endif // TEST_MODE
	return JAB_SUCCESS;
}

/**
 * @brief Swap two symbols
 * @param enc the encode parameters
 * @param index1 the index number of the first symbol
 * @param index2 the index number of the second symbol
*/
void swap_symbols(jab_encode* enc, jab_int32 index1, jab_int32 index2)
{
	swap_int(&enc->symbol_positions[index1],  &enc->symbol_positions[index2]);
	swap_int(&enc->symbol_versions[index1].x, &enc->symbol_versions[index2].x);
	swap_int(&enc->symbol_versions[index1].y, &enc->symbol_versions[index2].y);
	swap_byte(&enc->symbol_ecc_levels[index1],&enc->symbol_ecc_levels[index2]);
	//swap symbols
	jab_symbol s;
    s = enc->symbols[index1];
    enc->symbols[index1] = enc->symbols[index2];
    enc->symbols[index2] = s;
}

/**
 * @brief Assign docked symbols to their hosts
 * @param enc the encode parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean assignDockedSymbols(jab_encode* enc)
{
	//initialize host and slaves
	for(jab_int32 i=0; i<enc->symbol_number; i++)
	{
		//initialize symbol host index
		enc->symbols[i].host = -1;
		//initialize symbol's slave index
		for(jab_int32 j=0; j<4; j++)
			enc->symbols[i].slaves[j] = 0;	//0:no slave
	}
	//assign docked symbols
	jab_int32 assigned_slave_index = 1;
    for(jab_int32 i=0; i<enc->symbol_number-1 && assigned_slave_index<enc->symbol_number; i++)
    {
    	for(jab_int32 j=0; j<4 && assigned_slave_index<enc->symbol_number; j++)
		{
			for(jab_int32 k=i+1; k<enc->symbol_number && assigned_slave_index<enc->symbol_number; k++)
			{
				if(enc->symbols[k].host == -1)
				{
					jab_int32 hpos = enc->symbol_positions[i];
					jab_int32 spos = enc->symbol_positions[k];
					jab_boolean slave_found = JAB_FAILURE;
					switch(j)
					{
					case 0:	//top
						if(jab_symbol_pos[hpos].x == jab_symbol_pos[spos].x && jab_symbol_pos[hpos].y - 1 == jab_symbol_pos[spos].y)
						{
							enc->symbols[i].slaves[0] = assigned_slave_index;
							enc->symbols[k].slaves[1] = -1;	//-1:host position
							slave_found = JAB_SUCCESS;
						}
						break;
					case 1:	//bottom
						if(jab_symbol_pos[hpos].x == jab_symbol_pos[spos].x && jab_symbol_pos[hpos].y + 1 == jab_symbol_pos[spos].y)
						{
							enc->symbols[i].slaves[1] = assigned_slave_index;
							enc->symbols[k].slaves[0] = -1;
							slave_found = JAB_SUCCESS;
						}
						break;
					case 2:	//left
						if(jab_symbol_pos[hpos].y == jab_symbol_pos[spos].y && jab_symbol_pos[hpos].x - 1 == jab_symbol_pos[spos].x)
						{
							enc->symbols[i].slaves[2] = assigned_slave_index;
							enc->symbols[k].slaves[3] = -1;
							slave_found = JAB_SUCCESS;
						}
						break;
					case 3://right
						if(jab_symbol_pos[hpos].y == jab_symbol_pos[spos].y && jab_symbol_pos[hpos].x + 1 == jab_symbol_pos[spos].x)
						{
							enc->symbols[i].slaves[3] = assigned_slave_index;
							enc->symbols[k].slaves[2] = -1;
							slave_found = JAB_SUCCESS;
						}
						break;
					}
					if(slave_found)
					{
						swap_symbols(enc, k, assigned_slave_index);
						enc->symbols[assigned_slave_index].host = i;
						assigned_slave_index++;
					}
				}
			}
		}
    }
    //check if there is undocked symbol
    for(jab_int32 i=1; i<enc->symbol_number; i++)
    {
		if(enc->symbols[i].host == -1)
		{
			JAB_REPORT_ERROR(("Slave symbol at position %d has no host", enc->symbol_positions[i]))
			return JAB_FAILURE;
		}
    }
    return JAB_SUCCESS;
}

/**
 * @brief Calculate the code parameters according to the input symbols
 * @param enc the encode parameters
 * @return the code parameters
*/
jab_code* getCodePara(jab_encode* enc)
{
    jab_code* cp = (jab_code *)malloc(sizeof(jab_code));
    if(!cp)
    {
        reportError("Memory allocation for code parameter failed");
        return NULL;
    }
    //calculate the module size in pixel
    if(enc->master_symbol_width != 0 || enc->master_symbol_height != 0)
    {
        jab_int32 dimension_x = enc->master_symbol_width/enc->symbols[0].side_size.x;
        jab_int32 dimension_y = enc->master_symbol_height/enc->symbols[0].side_size.y;
        cp->dimension = dimension_x > dimension_y ? dimension_x : dimension_y;
        if(cp->dimension < 1)
            cp->dimension = 1;
    }
    else
    {
        cp->dimension = enc->module_size;
    }

    //find the coordinate range of symbols
    cp->min_x = 0;
    cp->min_y = 0;
    jab_int32 max_x=0, max_y=0;
    for(jab_int32 i=0; i<enc->symbol_number; i++)
    {
        //find the mininal x and y
        if(jab_symbol_pos[enc->symbol_positions[i]].x < cp->min_x)
            cp->min_x = jab_symbol_pos[enc->symbol_positions[i]].x;
        if(jab_symbol_pos[enc->symbol_positions[i]].y < cp->min_y)
            cp->min_y = jab_symbol_pos[enc->symbol_positions[i]].y;
        //find the maximal x and y
        if(jab_symbol_pos[enc->symbol_positions[i]].x > max_x)
            max_x = jab_symbol_pos[enc->symbol_positions[i]].x;
        if(jab_symbol_pos[enc->symbol_positions[i]].y > max_y)
            max_y = jab_symbol_pos[enc->symbol_positions[i]].y;
    }

    //calculate the code size
    cp->rows = max_y - cp->min_y + 1;
    cp->cols = max_x - cp->min_x + 1;
    cp->row_height = (jab_int32 *)malloc(cp->rows * sizeof(jab_int32));
    if(!cp->row_height)
    {
		free(cp);
        reportError("Memory allocation for row height in code parameter failed");
        return NULL;
    }
    cp->col_width = (jab_int32 *)malloc(cp->cols * sizeof(jab_int32));
    if(!cp->col_width)
    {
		free(cp);
        reportError("Memory allocation for column width in code parameter failed");
        return NULL;
    }
    cp->code_size.x = 0;
    cp->code_size.y = 0;
    jab_boolean flag = 0;
    for(jab_int32 x=cp->min_x; x<=max_x; x++)
    {
        flag = 0;
        for(jab_int32 i=0; i<enc->symbol_number; i++)
        {
            if(jab_symbol_pos[enc->symbol_positions[i]].x == x)
            {
                cp->col_width[x - cp->min_x] = enc->symbols[i].side_size.x;
                cp->code_size.x += cp->col_width[x - cp->min_x];
                flag = 1;
            }
            if(flag) break;
        }
    }
    for(jab_int32 y=cp->min_y; y<=max_y; y++)
    {
        flag = 0;
        for(jab_int32 i=0; i<enc->symbol_number; i++)
        {
            if(jab_symbol_pos[enc->symbol_positions[i]].y == y)
            {
                cp->row_height[y - cp->min_y] = enc->symbols[i].side_size.y;
                cp->code_size.y += cp->row_height[y - cp->min_y];
                flag = 1;
            }
            if(flag) break;
        }
    }
    return cp;
}

/**
 * @brief Create bitmap for the code
 * @param enc the encode parameters
 * @param cp the code parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean createBitmap(jab_encode* enc, jab_code* cp)
{
    //create bitmap
    jab_int32 width = cp->dimension * cp->code_size.x;
    jab_int32 height= cp->dimension * cp->code_size.y;
    jab_int32 bytes_per_pixel = BITMAP_BITS_PER_PIXEL / 8;
    jab_int32 bytes_per_row = width * bytes_per_pixel;
    enc->bitmap = (jab_bitmap *)calloc(1, sizeof(jab_bitmap) + width*height*bytes_per_pixel*sizeof(jab_byte));
    if(enc->bitmap == NULL)
    {
        reportError("Memory allocation for bitmap failed");
        return JAB_FAILURE;
    }
    enc->bitmap->width = width;
    enc->bitmap->height= height;
    enc->bitmap->bits_per_pixel = BITMAP_BITS_PER_PIXEL;
    enc->bitmap->bits_per_channel = BITMAP_BITS_PER_CHANNEL;
    enc->bitmap->channel_count = BITMAP_CHANNEL_COUNT;

    //place symbols in bitmap
    for(jab_int32 k=0; k<enc->symbol_number; k++)
    {
        //calculate the starting coordinates of the symbol matrix
        jab_int32 startx = 0, starty = 0;
        jab_int32 col = jab_symbol_pos[enc->symbol_positions[k]].x - cp->min_x;
        jab_int32 row = jab_symbol_pos[enc->symbol_positions[k]].y - cp->min_y;
        for(jab_int32 c=0; c<col; c++)
            startx += cp->col_width[c];
        for(jab_int32 r=0; r<row; r++)
            starty += cp->row_height[r];

        //place symbol in the code
        jab_int32 symbol_width = enc->symbols[k].side_size.x;
        jab_int32 symbol_height= enc->symbols[k].side_size.y;
        for(jab_int32 x=startx; x<(startx+symbol_width); x++)
        {
            for(jab_int32 y=starty; y<(starty+symbol_height); y++)
            {
                //place one module in the bitmap
                jab_int32 p_index = enc->symbols[k].matrix[(y-starty)*symbol_width + (x-startx)];
                 for(jab_int32 i=y*cp->dimension; i<(y*cp->dimension+cp->dimension); i++)
                {
                    for(jab_int32 j=x*cp->dimension; j<(x*cp->dimension+cp->dimension); j++)
                    {
                        enc->bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel]     = enc->palette[p_index * 3];	//R
                        enc->bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 1] = enc->palette[p_index * 3 + 1];//B
                        enc->bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 2] = enc->palette[p_index * 3 + 2];//G
                        enc->bitmap->pixel[i*bytes_per_row + j*bytes_per_pixel + 3] = 255; 							//A
                    }
                }
            }
        }
    }
    return JAB_SUCCESS;
}


/**
 * @brief Checks if the docked symbol sizes are valid
 * @param enc the encode parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean checkDockedSymbolSize(jab_encode* enc)
{
	for(jab_int32 i=0; i<enc->symbol_number; i++)
	{
		for(jab_int32 j=0; j<4; j++)
		{
			jab_int32 slave_index = enc->symbols[i].slaves[j];
			if(slave_index > 0)
			{
				jab_int32 hpos = enc->symbol_positions[i];
				jab_int32 spos = enc->symbol_positions[slave_index];
				jab_int32 x_diff = jab_symbol_pos[hpos].x - jab_symbol_pos[spos].x;
				jab_int32 y_diff = jab_symbol_pos[hpos].y - jab_symbol_pos[spos].y;

				if(x_diff == 0 && enc->symbol_versions[i].x != enc->symbol_versions[slave_index].x)
				{
					JAB_REPORT_ERROR(("Slave symbol at position %d has different side version in X direction as its host symbol at position %d", spos, hpos))
					return JAB_FAILURE;
				}
				if(y_diff == 0 && enc->symbol_versions[i].y != enc->symbol_versions[slave_index].y)
				{
					JAB_REPORT_ERROR(("Slave symbol at position %d has different side version in Y direction as its host symbol at position %d", spos, hpos))
					return JAB_FAILURE;
				}
			}
		}
	}
	return JAB_SUCCESS;
}

/**
 * @brief Set the minimal master symbol version
 * @param enc the encode parameters
 * @param encoded_data the encoded message
 * @return JAB_SUCCESS | JAB_FAILURE
 */
jab_boolean setMasterSymbolVersion(jab_encode *enc, jab_data* encoded_data)
{
    //calculate required number of data modules depending on data_length
    jab_int32 net_data_length = encoded_data->length;
    jab_int32 payload_length = net_data_length + 5;  //plus S and flag bit
    if(enc->symbol_ecc_levels[0] == 0) enc->symbol_ecc_levels[0] = DEFAULT_ECC_LEVEL;
    enc->symbols[0].wcwr[0] = ecclevel2wcwr[enc->symbol_ecc_levels[0]][0];
	enc->symbols[0].wcwr[1] = ecclevel2wcwr[enc->symbol_ecc_levels[0]][1];

	//determine the minimum square symbol to fit data
	jab_int32 capacity, net_capacity;
	jab_boolean found_flag = JAB_FAILURE;
	for (jab_int32 i=1; i<=32; i++)
	{
		enc->symbol_versions[0].x = i;
		enc->symbol_versions[0].y = i;
		capacity = getSymbolCapacity(enc, 0);
		net_capacity = (capacity/enc->symbols[0].wcwr[1])*enc->symbols[0].wcwr[1] - (capacity/enc->symbols[0].wcwr[1])*enc->symbols[0].wcwr[0];
		if(net_capacity >= payload_length)
		{
			found_flag = JAB_SUCCESS;
			break;
		}
	}
	if(!found_flag)
	{
		jab_int32 level = -1;
		for (jab_int32 j=(jab_int32)enc->symbol_ecc_levels[0]-1; j>0; j--)
		{
			net_capacity = (capacity/ecclevel2wcwr[j][1])*ecclevel2wcwr[j][1] - (capacity/ecclevel2wcwr[j][1])*ecclevel2wcwr[j][0];
			if(net_capacity >= payload_length)
				level = j;
		}
		if(level > 0)
		{
			JAB_REPORT_ERROR(("Message does not fit into one symbol with the given ECC level. Please use an ECC level lower than %d with '--ecc-level %d'", level, level))
			return JAB_FAILURE;
		}
		else
		{
			reportError("Message does not fit into one symbol. Use more symbols.");
			return JAB_FAILURE;
		}
	}
	//update symbol side size
    enc->symbols[0].side_size.x = VERSION2SIZE(enc->symbol_versions[0].x);
	enc->symbols[0].side_size.y = VERSION2SIZE(enc->symbol_versions[0].y);

    return JAB_SUCCESS;
}

/**
 * @brief Add variable E to slave symbol metadata the data payload for each symbol
 * @param slave the slave symbol
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean addE2SlaveMetadata(jab_symbol* slave)
{
	//copy old metadata to new metadata
	jab_int32 old_metadata_length = slave->metadata->length;
	jab_int32 new_metadata_length = old_metadata_length + 6;
	jab_data* old_metadata = slave->metadata;
	slave->metadata = (jab_data *)malloc(sizeof(jab_data) + new_metadata_length*sizeof(jab_char));
	if(slave->metadata == NULL)
	{
		reportError("Memory allocation for metadata in slave symbol failed");
		return JAB_FAILURE;
	}
	slave->metadata->length = new_metadata_length;
	memcpy(slave->metadata->data, old_metadata->data, old_metadata_length);
	free(old_metadata);

	//update SE = 1
	slave->metadata->data[1] = 1;
	//set variable E
	jab_int32 E1 = slave->wcwr[0] - 3;
	jab_int32 E2 = slave->wcwr[1] - 4;
	convert_dec_to_bin(E1, slave->metadata->data, old_metadata_length, 3);
	convert_dec_to_bin(E2, slave->metadata->data, old_metadata_length+3, 3);
	return JAB_SUCCESS;
}

/**
 * @brief Update slave metadata E in its host data stream
 * @param enc the encode parameters
 * @param host_index the host symbol index
 * @param slave_index the slave symbol index
*/
void updateSlaveMetadataE(jab_encode* enc, jab_int32 host_index, jab_int32 slave_index)
{
	jab_symbol* host = &enc->symbols[host_index];
	jab_symbol* slave= &enc->symbols[slave_index];

	jab_int32 offset = host->data->length - 1;
	//find the start flag of metadata
	while(host->data->data[offset] == 0)
	{
		offset--;
	}
	//skip the flag bit
	offset--;
	//skip host metadata S
	if(host_index == 0)
		offset -= 4;
	else
		offset -= 3;
	//skip other slave symbol's metadata
	for(jab_int32 i=0; i<4; i++)
	{
		if(host->slaves[i] == slave_index)
			break;
		else if(host->slaves[i] <= 0)
			continue;
		else
			offset -= enc->symbols[host->slaves[i]].metadata->length;
	}
	//skip SS, SE and possibly V
    if(slave->metadata->data[0] == 1)
		offset -= 7;
	else
		offset -= 2;
	//update E
	jab_char E[6];
	jab_int32 E1 = slave->wcwr[0] - 3;
	jab_int32 E2 = slave->wcwr[1] - 4;
	convert_dec_to_bin(E1, E, 0, 3);
	convert_dec_to_bin(E2, E, 3, 3);
	for(jab_int32 i=0; i<6; i++)
	{
		host->data->data[offset--] = E[i];
	}
}

/**
 * @brief Set the data payload for each symbol
 * @param enc the encode parameters
 * @param encoded_data the encoded message
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean fitDataIntoSymbols(jab_encode* enc, jab_data* encoded_data)
{
	//calculate the net capacity of each symbol and the total net capacity
	jab_int32 capacity[enc->symbol_number];
	jab_int32 net_capacity[enc->symbol_number];
	jab_int32 total_net_capacity = 0;
	for(jab_int32 i=0; i<enc->symbol_number; i++)
	{
		capacity[i] = getSymbolCapacity(enc, i);
		enc->symbols[i].wcwr[0] = ecclevel2wcwr[enc->symbol_ecc_levels[i]][0];
		enc->symbols[i].wcwr[1] = ecclevel2wcwr[enc->symbol_ecc_levels[i]][1];
		net_capacity[i] = (capacity[i]/enc->symbols[i].wcwr[1])*enc->symbols[i].wcwr[1] - (capacity[i]/enc->symbols[i].wcwr[1])*enc->symbols[i].wcwr[0];
		total_net_capacity += net_capacity[i];
	}
	//assign data into each symbol
	jab_int32 assigned_data_length = 0;
	for(jab_int32 i=0; i<enc->symbol_number; i++)
	{
		//divide data proportionally
		jab_int32 s_data_length;
		if(i == enc->symbol_number - 1)
		{
			s_data_length = encoded_data->length - assigned_data_length;
		}
		else
		{
			jab_float prop = (jab_float)net_capacity[i] / (jab_float)total_net_capacity;
			s_data_length = (jab_int32)(prop * encoded_data->length);
		}
		jab_int32 s_payload_length = s_data_length;

		//add flag bit
		s_payload_length++;
		//add host metadata S length (master metadata Part III or slave metadata Part III)
		if(i == 0)	s_payload_length += 4;
		else		s_payload_length += 3;
		//add slave metadata length
		for(jab_int32 j=0; j<4; j++)
		{
			if(enc->symbols[i].slaves[j] > 0)
			{
				s_payload_length += enc->symbols[enc->symbols[i].slaves[j]].metadata->length;
			}
		}

		//check if the full payload exceeds net capacity
		if(s_payload_length > net_capacity[i])
		{
			reportError("Message does not fit into the specified code. Use higher symbol version.");
			return JAB_FAILURE;
		}

		//add metadata E for slave symbols if free capacity available
		jab_int32 j = 0;
		while(net_capacity[i] - s_payload_length >= 6 && j < 4)
		{
			if(enc->symbols[i].slaves[j] > 0)
			{
				if(enc->symbols[enc->symbols[i].slaves[j]].metadata->data[1] == 0) //check SE
				{
					if(!addE2SlaveMetadata(&enc->symbols[enc->symbols[i].slaves[j]]))
						return JAB_FAILURE;
					s_payload_length += 6;	//add E length
				}
			}
			j++;
		}

		//get optimal code rate
		jab_int32 pn_length = s_payload_length;
		if(i == 0)
		{
			if(!isDefaultMode(enc))
			{
				getOptimalECC(capacity[i], s_payload_length, enc->symbols[i].wcwr);
				pn_length = (capacity[i]/enc->symbols[i].wcwr[1])*enc->symbols[i].wcwr[1] - (capacity[i]/enc->symbols[i].wcwr[1])*enc->symbols[i].wcwr[0];
			}
			else
				pn_length = net_capacity[i];
		}
		else
		{
			if(enc->symbols[i].metadata->data[1] == 1)	//SE = 1
			{
				getOptimalECC(capacity[i], pn_length, enc->symbols[i].wcwr);
				pn_length = (capacity[i]/enc->symbols[i].wcwr[1])*enc->symbols[i].wcwr[1] - (capacity[i]/enc->symbols[i].wcwr[1])*enc->symbols[i].wcwr[0];
				updateSlaveMetadataE(enc, enc->symbols[i].host, i);
			}
			else
				pn_length = net_capacity[i];
		}

		//start to set full payload
        enc->symbols[i].data = (jab_data *)calloc(1, sizeof(jab_data) + pn_length*sizeof(jab_char));
        if(enc->symbols[i].data == NULL)
		{
			reportError("Memory allocation for data payload in symbol failed");
			return JAB_FAILURE;
		}
		enc->symbols[i].data->length = pn_length;
		//set data
		memcpy(enc->symbols[i].data->data, &encoded_data->data[assigned_data_length], s_data_length);
		assigned_data_length += s_data_length;
		//set flag bit
		jab_int32 set_pos = s_payload_length - 1;
		enc->symbols[i].data->data[set_pos--] = 1;
		//set host metadata S
		for(jab_int32 k=0; k<4; k++)
		{
			if(enc->symbols[i].slaves[k] > 0)
			{
				enc->symbols[i].data->data[set_pos--] = 1;
			}
			else if(enc->symbols[i].slaves[k] == 0)
			{
				enc->symbols[i].data->data[set_pos--] = 0;
			}
		}
		//set slave metadata
		for(jab_int32 k=0; k<4; k++)
		{
			if(enc->symbols[i].slaves[k] > 0)
			{
				for(jab_int32 m=0; m<enc->symbols[enc->symbols[i].slaves[k]].metadata->length; m++)
				{
					enc->symbols[i].data->data[set_pos--] = enc->symbols[enc->symbols[i].slaves[k]].metadata->data[m];
				}
			}
		}
	}
	return JAB_SUCCESS;
}

/**
 * @brief Initialize symbols
 * @param enc the encode parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean InitSymbols(jab_encode* enc)
{
	//check all information for multi-symbol code are valid
	if(enc->symbol_number > 1)
	{
		for(jab_int32 i=0; i<enc->symbol_number; i++)
		{
			if(enc->symbol_versions[i].x < 1 || enc->symbol_versions[i].x > 32 || enc->symbol_versions[i].y < 1 || enc->symbol_versions[i].y > 32)
			{
				JAB_REPORT_ERROR(("Incorrect symbol version for symbol %d", i))
				return JAB_FAILURE;
			}
			if(enc->symbol_positions[i] < 0 || enc->symbol_positions[i] > MAX_SYMBOL_NUMBER)
			{
				JAB_REPORT_ERROR(("Incorrect symbol position for symbol %d", i))
				return JAB_FAILURE;
			}
		}
	}
	//move the master symbol to the first
    if(enc->symbol_number > 1 && enc->symbol_positions[0] != 0)
	{
		for(jab_int32 i=0; i<enc->symbol_number; i++)
		{
			if(enc->symbol_positions[i] == 0)
			{
				swap_int (&enc->symbol_positions[i], 	&enc->symbol_positions[0]);
				swap_int (&enc->symbol_versions[i].x, 	&enc->symbol_versions[0].x);
				swap_int (&enc->symbol_versions[i].y, 	&enc->symbol_versions[0].y);
				swap_byte(&enc->symbol_ecc_levels[i], 	&enc->symbol_ecc_levels[0]);
				break;
			}
		}
	}
    //if no master symbol exists in multi-symbol code
    if(enc->symbol_number > 1 && enc->symbol_positions[0] != 0)
    {
		reportError("Master symbol missing");
		return JAB_FAILURE;
    }
    //if only one symbol but its position is not 0 - set to zero. Everything else makes no sense.
    if(enc->symbol_number == 1 && enc->symbol_positions[0] != 0)
        enc->symbol_positions[0] = 0;
    //check if a symbol position is used twice
	for(jab_int32 i=0; i<enc->symbol_number-1; i++)
	{
		for(jab_int32 j=i+1; j<enc->symbol_number; j++)
		{
			if(enc->symbol_positions[i] == enc->symbol_positions[j])
			{
				reportError("Duplicate symbol position");
				return JAB_FAILURE;
			}
		}
	}
	//assign docked symbols to their hosts
    if(!assignDockedSymbols(enc))
		return JAB_FAILURE;
    //check if the docked symbol size matches the docked side of its host
    if(!checkDockedSymbolSize(enc))
		return JAB_FAILURE;
	//set symbol index and symbol side size
    for(jab_int32 i=0; i<enc->symbol_number; i++)
    {
    	//set symbol index
		enc->symbols[i].index = i;
        //set symbol side size
        enc->symbols[i].side_size.x = VERSION2SIZE(enc->symbol_versions[i].x);
        enc->symbols[i].side_size.y = VERSION2SIZE(enc->symbol_versions[i].y);
    }
    return JAB_SUCCESS;
}

/**
 * @brief Set metadata for slave symbols
 * @param enc the encode parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean setSlaveMetadata(jab_encode* enc)
{
	//set slave metadata variables
	for(jab_int32 i=1; i<enc->symbol_number; i++)
	{
		jab_int32 SS, SE, V, E1=0, E2=0;
		jab_int32 metadata_length = 2; //Part I length
		//SS and V
		if(enc->symbol_versions[i].x != enc->symbol_versions[enc->symbols[i].host].x)
		{
		  	SS = 1;
			V = enc->symbol_versions[i].x - 1;
			metadata_length += 5;
		}
		else if(enc->symbol_versions[i].y != enc->symbol_versions[enc->symbols[i].host].y)
		{
			SS = 1;
			V = enc->symbol_versions[i].y - 1;
			metadata_length += 5;
		}
		else
		{
			SS = 0;
		}
		//SE and E
		if(enc->symbol_ecc_levels[i] == 0 || enc->symbol_ecc_levels[i] == enc->symbol_ecc_levels[enc->symbols[i].host])
		{
			SE = 0;
		}
		else
		{
			SE = 1;
			E1 = ecclevel2wcwr[enc->symbol_ecc_levels[i]][0] - 3;
			E2 = ecclevel2wcwr[enc->symbol_ecc_levels[i]][1] - 4;
			metadata_length += 6;
		}
		//write slave metadata
		enc->symbols[i].metadata = (jab_data *)malloc(sizeof(jab_data) + metadata_length*sizeof(jab_char));
		if(enc->symbols[i].metadata == NULL)
		{
			reportError("Memory allocation for metadata in slave symbol failed");
			return JAB_FAILURE;
		}
		enc->symbols[i].metadata->length = metadata_length;
		//Part I
		enc->symbols[i].metadata->data[0] = SS;
		enc->symbols[i].metadata->data[1] = SE;
		//Part II
		if(SS == 1)
		{
			convert_dec_to_bin(V, enc->symbols[i].metadata->data, 2, 5);
		}
		if(SE == 1)
		{
			jab_int32 start_pos = (SS == 1) ? 7 : 2;
			convert_dec_to_bin(E1, enc->symbols[i].metadata->data, start_pos, 3);
			convert_dec_to_bin(E2, enc->symbols[i].metadata->data, start_pos+3, 3);
		}
	}
	return JAB_SUCCESS;
}

/**
 * @brief Generate JABCode
 * @param enc the encode parameters
 * @param data the input data
 * @return 0:success | 1: out of memory | 2:no input data | 3:incorrect symbol version or position | 4: input data too long
*/
jab_int32 generateJABCode(jab_encode* enc, jab_data* data)
{
    //Check data
    if(data == NULL)
    {
        reportError("No input data specified!");
        return 2;
    }
    if(data->length == 0)
    {
        reportError("No input data specified!");
        return 2;
    }

    //initialize symbols and set metadata in symbols
    if(!InitSymbols(enc))
		return 3;

    //get the optimal encoded length and encoding sequence
    jab_int32 encoded_length;
    jab_int32* encode_seq = analyzeInputData(data, &encoded_length);
    if(encode_seq == NULL)
	{
		reportError("Analyzing input data failed");
		return 1;
    }
	//encode data using optimal encoding modes
    jab_data* encoded_data = encodeData(data, encoded_length, encode_seq);
    free(encode_seq);
    if(encoded_data == NULL)
    {
        return 1;
    }
    //set master symbol version if not given
    if(enc->symbol_number == 1 && (enc->symbol_versions[0].x == 0 || enc->symbol_versions[0].y == 0))
    {
        if(!setMasterSymbolVersion(enc, encoded_data))
        {
        	free(encoded_data);
            return 4;
        }
    }
	//set metadata for slave symbols
	if(!setSlaveMetadata(enc))
	{
		free(encoded_data);
		return 1;
	}
	//assign encoded data into symbols
	if(!fitDataIntoSymbols(enc, encoded_data))
	{
		free(encoded_data);
		return 4;
	}
	free(encoded_data);
	//set master metadata
	if(!isDefaultMode(enc))
	{
		if(!encodeMasterMetadata(enc))
		{
			JAB_REPORT_ERROR(("Encoding master symbol metadata failed"))
            return 1;
		}
	}

    //encode each symbol in turn
    for(jab_int32 i=0; i<enc->symbol_number; i++)
    {
        //error correction for data
        jab_data* ecc_encoded_data = encodeLDPC(enc->symbols[i].data, enc->symbols[i].wcwr);
        if(ecc_encoded_data == NULL)
        {
            JAB_REPORT_ERROR(("LDPC encoding for the data in symbol %d failed", i))
            return 1;
        }
        //interleave
        interleaveData(ecc_encoded_data);
        //create Matrix
        jab_boolean cm_flag = createMatrix(enc, i, ecc_encoded_data);
        free(ecc_encoded_data);
        if(!cm_flag)
        {
			JAB_REPORT_ERROR(("Creating matrix for symbol %d failed", i))
			return 1;
		}
    }

    //mask all symbols in the code
    jab_code* cp = getCodePara(enc);
    if(!cp)
    {
		return 1;
    }
    if(isDefaultMode(enc))	//default mode
	{
		maskSymbols(enc, DEFAULT_MASKING_REFERENCE, 0, 0);
	}
	else
	{
		jab_int32 mask_reference = maskCode(enc, cp);
		if(mask_reference < 0)
		{
			free(cp->row_height);
			free(cp->col_width);
			free(cp);
			return 1;
		}
#if TEST_MODE
		JAB_REPORT_INFO(("mask reference: %d", mask_reference))
#endif
		if(mask_reference != DEFAULT_MASKING_REFERENCE)
		{
			//re-encode PartII of master symbol metadata
			updateMasterMetadataPartII(enc, mask_reference);
			//update the masking reference in master symbol metadata
			placeMasterMetadataPartII(enc);
		}
	}

    //create the code bitmap
    jab_boolean cb_flag = createBitmap(enc, cp);
    free(cp->row_height);
    free(cp->col_width);
    free(cp);
    if(!cb_flag)
	{
		JAB_REPORT_ERROR(("Creating the code bitmap failed"))
		return 1;
	}
    return 0;
}

/**
 * @brief Report error message
 * @param message the error message
*/
void reportError(jab_char* message)
{
    printf("JABCode Error: %s\n", message);
}
