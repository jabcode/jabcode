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
    if(color_number == 2)
    {
    	memcpy(palette + 0, jab_default_palette + FP0_CORE_COLOR_BW * 3, 3);
    	memcpy(palette + 3, jab_default_palette + FPX_CORE_COLOR_BW * 3, 3);
    }
    else if(color_number == 4)
    {
    	memcpy(palette + 0, jab_default_palette + FP0_CORE_COLOR * 3, 3);
    	memcpy(palette + 3, jab_default_palette + FP1_CORE_COLOR * 3, 3);
    	memcpy(palette + 6, jab_default_palette + FP2_CORE_COLOR * 3, 3);
    	memcpy(palette + 9, jab_default_palette + FP3_CORE_COLOR * 3, 3);
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
 * @param d the decimal value
 * @param encoded_data the encoded data array
 * @param start_position the position to write in encoded data array
 * @param length the length of the converted binary sequence
 */
void convert_dec_to_bin(jab_int32 d,jab_data* encoded_data,jab_int32 start_position, jab_int32 length)
{
    if(d<0)
        d=256+d;
    for (jab_int32 j=0;j<length;j++)
    {
        jab_int32 t=d%2;
        encoded_data->data[start_position+length-1-j]=(jab_char) t;
        d/=2;
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

    if(color_number != 2  && color_number != 4  && color_number != 8   && color_number != 16 &&
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
    enc->ecc_levels = (jab_byte *)calloc(symbol_number, sizeof(jab_byte));
    if(enc->ecc_levels == NULL)
    {
        reportError("Memory allocation for ecc levels failed");
        return NULL;
    }
    setDefaultEccLevels(enc->symbol_number, enc->ecc_levels);
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
    //allocate memory for docked symbols
    enc->docked_symbol = (jab_int32 *)calloc(enc->symbol_number * 4, sizeof(jab_int32)); //maximum 4 docked symbols
    if(enc->docked_symbol == NULL)
    {
        reportError("Memory allocation for docked symbols failed");
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
    if(enc->palette)			free(enc->palette);
    if(enc->symbol_versions)	free(enc->symbol_versions);
    if(enc->ecc_levels)			free(enc->ecc_levels);
    if(enc->symbol_positions)	free(enc->symbol_positions);
    if(enc->bitmap)				free(enc->bitmap);
    if(enc->docked_symbol)      free(enc->docked_symbol);
    if(enc->symbols)
    {
        for(jab_int32 i=0; i<enc->symbol_number; i++)
        {
            if(enc->symbols[i].data_map)			free(enc->symbols[i].data_map);
            if(enc->symbols[i].encoded_metadata)	free(enc->symbols[i].encoded_metadata);
            if(enc->symbols[i].matrix)				free(enc->symbols[i].matrix);
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
            else if(jab_enconing_table[tmp][j]==-18 && tmp1==10 || jab_enconing_table[tmp][j]<-18 && tmp1==32)//read next character to decide if encodalbe in current mode
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
                if(len>=curr_seq_len[(i+1)*14+j]+curr_seq_len[i*14+k]+latch_shift_to[k][j] && k<13 || k==13 && prev==j)
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
                else if (((curr_seq_len[(i+1)*14+prev_mode[curr_seq_counter*14+j]]>len ||
                        jp_to_nxt_char==1 && curr_seq_len[(i+1)*14+prev_mode[curr_seq_counter*14+j]]+character_size[(prev_mode[curr_seq_counter*14+j])%7]>len)) && j == 13 )
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
 * @brief Transform error correction level to code rate
 * @param ecc_level the error correction level
 * @return the code rate
 */
jab_float fitCoderate(jab_byte ecc_level)
{
    return convert_ecc2coderate[ecc_level];
}

/**
 * @brief Divide the message in case of multi-symbol code into parts depending on error correction level and side size of each symbol
 * @param enc the encode parameters
 * @param data_length the message length
 * @param coderate_params the code rate and ecc parameters wc and wr for each symbol
 * @param from_to the start and end point of encoded message part for each symbol
 * @return 0: success | 1: error
 */
jab_boolean divideMessage(jab_encode *enc,jab_int32 *data_length,jab_int32 *coderate_params,jab_int32 *from_to)
{
    jab_int32* net_mess=(jab_int32 *)malloc(sizeof(jab_int32)*(enc->symbol_number));
    if(net_mess == NULL){
        reportError("Memory allocation for message length failed");
        return 1;
    }
    jab_int32* capacity=(jab_int32 *)malloc(sizeof(jab_int32)*(enc->symbol_number));
    if(capacity == NULL){
        reportError("Memory allocation for message length failed");
        free(net_mess);
        return 1;
    }
    jab_int32 possible_net_mess=0;
    jab_float symbol_rate=0.0f;
    //set all ecc levels
    for(jab_int32 i=0;i<enc->symbol_number;i++)
    {
        if(i>0 && enc->ecc_levels[i]==0)
        {
            for(jab_int32 j=0;j<4*(enc->symbol_number-1);j++)
            {
                //set ecc level equal to host
                if(enc->docked_symbol[j]==i)
                    enc->ecc_levels[i]=enc->ecc_levels[(jab_int32)j/4];
            }
        }

        if(enc->ecc_levels[i]==0)
            symbol_rate=1-4.0f/7.0f; //default
        else
            symbol_rate=fitCoderate(enc->ecc_levels[i]);
        //calculate capacity of complete code
        jab_int32 side_size_x=VERSION2SIZE(enc->symbol_versions[i].x);
        jab_int32 side_size_y=VERSION2SIZE(enc->symbol_versions[i].y);
        //number of modules for alignment pattern
        jab_int32 number_of_alignment_pattern_x = (side_size_x-(DISTANCE_TO_BORDER*2-1))/MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS-1;
        jab_int32 number_of_alignment_pattern_y = (side_size_y-(DISTANCE_TO_BORDER*2-1))/MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS-1;
        if (number_of_alignment_pattern_x < 0)
            number_of_alignment_pattern_x=0;
        if (number_of_alignment_pattern_y < 0)
            number_of_alignment_pattern_y=0;
        jab_int32 nb_modules_alignmet=(number_of_alignment_pattern_x+2)*(number_of_alignment_pattern_y+2)*7;
        //number of modules for color palette
        jab_int32 nb_modules_palette=enc->color_number > 64 ? 64*2 : enc->color_number*2;
        //number of modules for metadata
        getMetadataLength(enc,i);
        jab_int32 nb_of_bpmm=enc->color_number >= 8 ? 3 : log(enc->color_number)/log(2);
        jab_int32 nb_of_bpm=log(enc->color_number)/log(2);
        jab_int32 nb_modules_metadata=enc->symbols[i].encoded_metadata->length;
        if (i==0)
        {
            if((nb_modules_metadata-6)%nb_of_bpmm!=0)
            {
                nb_modules_metadata=(nb_modules_metadata-6)/nb_of_bpmm;
                nb_modules_metadata+=1;
            }
            else
                nb_modules_metadata=(nb_modules_metadata-6)/nb_of_bpmm;
            nb_modules_metadata+=6;
            jab_int32 nb_modules_finder=4*10;
            capacity[i]=side_size_x*side_size_y-nb_modules_alignmet-nb_modules_finder-nb_modules_palette-nb_modules_metadata;
            capacity[i]*=nb_of_bpm;
        }
        else
        {
            if((nb_modules_metadata)%nb_of_bpmm!=0)
            {
                nb_modules_metadata=(nb_modules_metadata)/nb_of_bpmm;
                nb_modules_metadata+=1;
            }
            else
                nb_modules_metadata=(nb_modules_metadata)/nb_of_bpmm;
            capacity[i]=side_size_x*side_size_y-nb_modules_alignmet-nb_modules_palette-nb_modules_metadata;
            capacity[i]*=nb_of_bpm;
        }
        net_mess[i]=capacity[i]*symbol_rate;
        possible_net_mess+=net_mess[i];
    }
    //check if message fits into code
    if(possible_net_mess < *data_length)
    {
        reportError("Message does not fit into the specified symbol version. Use a higher symbol version");
        free(net_mess);
        free(capacity);
        return 1;
    }
    else
    {
        jab_float relation=*data_length/(jab_float)possible_net_mess;
        jab_int32 div_mess=0,host;
        for (jab_int32 i=0;i<enc->symbol_number;i++)
        {
//            jab_int32 E=10;
//            if(enc->symbol_versions[i].x>4 || enc->symbol_versions[i].y>4)
//                E=12;
//            if(enc->symbol_versions[i].x>8 || enc->symbol_versions[i].y>8)
//                E=14;
//            if(enc->symbol_versions[i].x>16 || enc->symbol_versions[i].y>16)
//                E=16;
            //divide message
            if(i==enc->symbol_number-1)
                net_mess[i]=*data_length-div_mess;
            else
                net_mess[i]*=relation;
            div_mess+=net_mess[i];
            //ecc level host
            jab_byte same_ecc=0;

            if(i>0 && enc->ecc_levels[i]==0)
            {
                for (jab_int32 j=0;j<4*(enc->symbol_number-1);j++)
                {
                    //set ecc level equal to host
                    if(enc->docked_symbol[j]==i && enc->ecc_levels[i]==enc->ecc_levels[(jab_int32)j/4])
                    {
                        same_ecc=1;
                        host=j/4;
                    }
                }
            }

            //update coderate parameters
            jab_float min=(jab_float)capacity[i];

            if(same_ecc==1)
            {
                jab_int32 temp=capacity[i]*(coderate_params[2*host+1]-coderate_params[2*host])/coderate_params[2*host+1];
                if(temp>=net_mess[i])
                {
                    div_mess+=temp-net_mess[i];
                    net_mess[i]=temp;
                    coderate_params[2*i]=coderate_params[2*host];
                    coderate_params[2*i+1]=coderate_params[2*host+1];
                }
                else
                {
                    jab_int32 nb_bits_metadata=enc->symbols[i].encoded_metadata->length;
                    enc->ecc_levels[i]=enc->ecc_levels[host]+1;
                    getMetadataLength(enc,i);
                    jab_int32 new_nb_bits_metadata=enc->symbols[i].encoded_metadata->length;
                    capacity[i]-=(new_nb_bits_metadata-nb_bits_metadata);
                    same_ecc=0;
                }
            }
            if(same_ecc==0)
            {
       //         for (jab_int32 k=3;k<=pow(2,E/2)+2;k++)
                for (jab_int32 k=3;k<=6+2;k++)
                {
               //     for (jab_int32 j=k+1;j<=pow(2,E/2)+3;j++)
                    for (jab_int32 j=k+1;j<=6+3;j++)
                    {
                        jab_int32 dist=(capacity[i]/j)*j-(capacity[i]/j)*k-net_mess[i];
                        if(dist<min && dist>=0)
                        {
                            coderate_params[2*i+1]=j;
                            coderate_params[2*i]=k;
                            min=dist;
                        }
                    }
                }
            }
            div_mess-=net_mess[i];
            net_mess[i]=capacity[i]/coderate_params[2*i+1];
            net_mess[i]*=coderate_params[2*i+1]-coderate_params[2*i];
            div_mess+=net_mess[i];
            if (i==0)
                from_to[2*i]=0;
            else
                from_to[2*i]=from_to[2*i-1];
            from_to[2*i+1]=from_to[2*i]+net_mess[i];
            if(div_mess>*data_length)
                *data_length=div_mess;
        }
    }
    free(net_mess);
    free(capacity);
    return 0;
}

/**
 * @brief Determine the side version and error correction level if not specified for master symbol
 * @param enc the encode parameters
 * @param data_length the message length
 * @param coderate_params the code rate and ecc parameters wc and wr
 * @return 0: success | 1: error
 */
jab_boolean determineMissingParams(jab_encode *enc,jab_int32 *data_length,jab_int32 *coderate_params)
{
    //assignDockedSymbols(enc);
    jab_float coderate=0.0f;
    jab_int32 gross_message_length=0;
    jab_int32 nb_of_bpmm=enc->color_number >= 8 ? 3 : log(enc->color_number)/log(2);
    jab_int32 nb_of_bpm=log(enc->color_number)/log(2);
    //calculate required number of data moduls depending on data_length
    if((jab_int32)enc->ecc_levels[0] > 0)
    {
        coderate=fitCoderate(enc->ecc_levels[0]);//fit code rate
        gross_message_length=ceil((*data_length)/coderate);
    }
    else
    {
        enc->ecc_levels[0]=5;
        coderate_params[0]=default_ecl.x;
        coderate_params[1]=default_ecl.y;
 //       coderate=1-coderate_params[0]/(jab_float)coderate_params[1];
        gross_message_length=ceil(( *data_length * coderate_params[1])/((jab_float)(coderate_params[1]-coderate_params[0])));
    }

    //number of modules for outer layer of finder pattern
    jab_int32 nb_modules_finder=4*17;
    if(enc->symbol_versions->x==0 || enc->symbol_versions->y==0)
    {
        //determine minimum square symbol to fit data
        for (jab_int32 i=1;i<33;i++)
        {
            jab_int32 side_size=VERSION2SIZE(i);
            //number of modules for alignment pattern
            jab_int32 number_of_alignment_pattern_x = (side_size-(DISTANCE_TO_BORDER*2-1))/MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS-1;
            if (number_of_alignment_pattern_x < 0)
                number_of_alignment_pattern_x=0;
            jab_int32 nb_modules_alignmet=((number_of_alignment_pattern_x+2)*(number_of_alignment_pattern_x+2)-4)*7;
            //number of modules for color palette
            jab_int32 nb_modules_palette=enc->color_number > 64 ? 64*2 : enc->color_number*2;
            //number of modules for metadata
            jab_int32 nb_modules_metadata=44;
            //jab_int32 E=10;
            if(i>4)
            {
                nb_modules_metadata+=4;
                //E=12;
            }
            if(i>8)
            {
                nb_modules_metadata+=6;
                //E=14;
            }
            if(i>16)
            {
                nb_modules_metadata+=6;
                //E=16;
            }
            if((nb_modules_metadata-6)%nb_of_bpmm!=0)
            {
                nb_modules_metadata=(nb_modules_metadata-6)/nb_of_bpmm;
                nb_modules_metadata+=1;
            }
            else
                nb_modules_metadata=(nb_modules_metadata-6)/nb_of_bpmm;
            nb_modules_metadata+=6;

        //    nb_modules_metadata/=log(enc->color_number)/log(2);
            //does message fit into symbol
            jab_int32 capacity=side_size*side_size-nb_modules_alignmet-nb_modules_finder-nb_modules_palette-nb_modules_metadata;
            capacity*=nb_of_bpm;
            if(capacity>=gross_message_length)
            {
                enc->symbol_versions[0].x=enc->symbol_versions[0].y=i;
                //update coderate parameter
                jab_float min=(jab_float)capacity;
//                for (jab_int32 i=3;i<=pow(2,E/2)+2;i++)
                for (jab_int32 k=3;k<=6+2;k++)
                {
          //          for (jab_int32 j=i+1;j<=pow(2,E/2)+3;j++)
                    for (jab_int32 j=k+1;j<=6+3;j++)
                    {
                  //      jab_int32 dist=((jab_int32)(capacity/((jab_float)j)))*j-((jab_int32)(capacity/((jab_float)j)))*i-*data_length;
                 //       jab_float dist=capacity*(j-i)/((jab_float)j)-*data_length;
                        jab_int32 dist=(capacity/j)*j-(capacity/j)*k-*data_length;
                        if(dist<min && dist>=0)
                        {
                            coderate_params[1]=j;
                            coderate_params[0]=k;
                            min=dist;
                        }

                    }
                }
                *data_length=(capacity/coderate_params[1])*coderate_params[1]-(capacity/coderate_params[1])*coderate_params[0];
                getMetadataLength(enc,0);
                break;
            }
            if(i>=32 && capacity<gross_message_length)
            {
                jab_int32 level=-1;
                for (jab_int32 j=0; j<11; j++)
                {
                    coderate=fitCoderate(j);//fit code rate
                    if(coderate > 0)
                    {
                        gross_message_length=ceil((*data_length)/coderate);
                        if(gross_message_length < capacity)
                            level=j;
                    }
                }
                if(level >= 0)
                {
                    JAB_REPORT_ERROR(("Message does not fit into one symbol with default ecc level. Please use an ecc level lower than %d with '--ecc-level %d'", level, level))
                    return 1;
                }
                if(*data_length >= capacity || level < 0)
                {
                    reportError("Message does not fit into one symbol. Use more symbols.");
                    return 1;
                }
            }
        }
    }
    else //check if message fits into specified symbol
    {
        jab_int32 side_size_x=VERSION2SIZE(enc->symbol_versions[0].x);
        jab_int32 side_size_y=VERSION2SIZE(enc->symbol_versions[0].y);
        //number of modules for alignment pattern
        jab_int32 number_of_alignment_pattern_x = (side_size_x-(DISTANCE_TO_BORDER*2-1))/MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS-1;
        jab_int32 number_of_alignment_pattern_y = (side_size_y-(DISTANCE_TO_BORDER*2-1))/MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS-1;
        if (number_of_alignment_pattern_x < 0)
            number_of_alignment_pattern_x=0;
        if (number_of_alignment_pattern_y < 0)
            number_of_alignment_pattern_y=0;
        jab_int32 nb_modules_alignmet=((number_of_alignment_pattern_y+2)*(number_of_alignment_pattern_x+2)-4)*7;
        //number of modules for color palette
        jab_int32 nb_modules_palette=enc->color_number > 64 ? 64*2 : enc->color_number*2;
        //number of modules for metadata
        getMetadataLength(enc,0);
        jab_int32 E=10;
        if(enc->symbol_versions[0].x>4 || enc->symbol_versions[0].y>4)
            E=12;
        if(enc->symbol_versions[0].x>8 || enc->symbol_versions[0].y>8)
            E=14;
        if(enc->symbol_versions[0].x>16 || enc->symbol_versions[0].y>16)
            E=16;
        jab_int32 nb_modules_metadata=enc->symbols[0].encoded_metadata->length;
        if((nb_modules_metadata-6)%nb_of_bpmm!=0)
        {
            nb_modules_metadata=(nb_modules_metadata-6)/nb_of_bpmm;
            nb_modules_metadata+=1;
        }
        else
            nb_modules_metadata=(nb_modules_metadata-6)/nb_of_bpmm;
        nb_modules_metadata+=6;
        jab_int32 capacity=side_size_x*side_size_y-nb_modules_alignmet-nb_modules_finder-nb_modules_palette-nb_modules_metadata;
        capacity*=nb_of_bpm;
        if(capacity<gross_message_length)
        {
            reportError("Message does not fit into user specified symbol version: Use higher symbol version");
            return 1;
        }
        else
        {
            //update coderate parameters
            jab_float min=(jab_float)capacity;
       //     for (jab_int32 i=3;i<=pow(2,E/2)+2;i++)
            for (jab_int32 i=3;i<=6+2;i++)
            {
                for (jab_int32 j=i+1;j<=6+3;j++)
    //            for (jab_int32 j=i+1;j<=pow(2,E/2)+3;j++)
                {
                   // jab_int32 dist=((jab_int32)(capacity/((jab_float)j)))*j-((jab_int32)(capacity/((jab_float)j)))*i-*data_length;
                   // jab_float dist=capacity*(j-i)/((jab_float)j)-*data_length;
                    jab_int32 dist=(capacity/j)*j-(capacity/j)*i-*data_length;
                    if(dist<min && dist>=0)
                    {
                        coderate_params[1]=j;//((pow(2,E/2)+3)*(capacity-data_length))/capacity;
                        coderate_params[0]=i;//coderate_params[1]*(capacity-data_length)/capacity;
                        min=dist;
                    }

                }
            }
         //   *data_length=((jab_int32)(capacity/(jab_float)coderate_params[1]))*coderate_params[1]-((jab_int32)(capacity/(jab_float)coderate_params[1]))*coderate_params[0];
            *data_length=(capacity/coderate_params[1])*coderate_params[1]-(capacity/coderate_params[1])*coderate_params[0];
         //   *data_length=capacity*(coderate_params[1]-coderate_params[0])/(jab_float)coderate_params[1];
        }
    }

    return 0;
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
                    convert_dec_to_bin(mode_switch[encode_seq[counter]][encode_seq[counter+1]],encoded_data,position,length);
                else
                {
                    reportError("Ecoding failure");
                    free(encoded_data);
                    return NULL;
                }
                position+=latch_shift_to[encode_seq[counter]][encode_seq[counter+1]];
                if(encode_seq[counter+1] == 6 || encode_seq[counter+1] == 13)
                    position-=4;
                //check if latch or shift
                if(encode_seq[counter+1]>6 && encode_seq[counter+1]<=13 || encode_seq[counter+1]==13 && encode_seq[counter+2]!=13)
                    shift_back=1;//remember to shift back to mode from which was invoked
            }
            //if not byte mode
            if (encode_seq[counter+1]%7 != 6)// || end_of_loop-1 == i)
            {
                if(jab_enconing_table[tmp][encode_seq[counter+1]%7]>-1 && character_size[encode_seq[counter+1]%7] < ENC_MAX)
                {
                    //encode character
                    convert_dec_to_bin(jab_enconing_table[tmp][encode_seq[counter+1]%7],encoded_data,position,character_size[encode_seq[counter+1]%7]);
                    position+=character_size[encode_seq[counter+1]%7];
                    counter++;
                }
                else if (jab_enconing_table[tmp][encode_seq[counter+1]%7]<-1)
                {
                    jab_int32 tmp1=data->data[current_encoded_length+1];
                    if (tmp1 < 0)
                        tmp1+=256;
                    //read next character to see if more efficient encoding possible
                    if (((tmp==44 || tmp== 46 || tmp==58) && tmp1==32) || tmp==13 && tmp1==10)
                        decimal_value=abs(jab_enconing_table[tmp][encode_seq[counter+1]%7]);
                    else if (tmp==13 && tmp1!=10)
                        decimal_value=18;
                    else
                    {
                        reportError("Ecoding failure");
                        free(encoded_data);
                        return NULL;
                    }
                    if (character_size[encode_seq[counter+1]%7] < ENC_MAX)
                    convert_dec_to_bin(decimal_value,encoded_data,position,character_size[encode_seq[counter+1]%7]);
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
                    convert_dec_to_bin(byte_counter > 15 ? 0 : byte_counter,encoded_data,position,4);
                    position+=4;
                    if(byte_counter > 15)
                    {
                        convert_dec_to_bin(byte_counter-15-1,encoded_data,position,13);
                        position+=13;
                    }
                    byte_offset=byte_counter;
                }
                if (character_size[encode_seq[counter+1]%7] < ENC_MAX)
                    convert_dec_to_bin(tmp,encoded_data,position,character_size[encode_seq[counter+1]%7]);
                else
                {
                    reportError("Encoding failure");
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
    jab_int32 remaining=encoded_length-position;
    if(position<encoded_length)
    {
        if (encode_seq[counter]>6)
            encode_seq[counter]=encode_seq[counter-1];
        if (encode_seq[counter]==0 && encoded_length-position>=5)
        {
            convert_dec_to_bin(28,encoded_data,position,5);
            remaining=encoded_length-position-5;
            if(encoded_length-position>=10)
            {
                convert_dec_to_bin(31,encoded_data,position+5,5);
                remaining=encoded_length-position-10;
            }
            if(encoded_length-position>=12)
            {
                convert_dec_to_bin(3,encoded_data,position+10,2);
                remaining=encoded_length-position-12;
            }
        }
        else if (encode_seq[counter]==1  && encoded_length-position>=5)
        {
            convert_dec_to_bin(31,encoded_data,position,5);
            remaining=encoded_length-position-5;
            if(encoded_length-position>=7)
            {
                convert_dec_to_bin(3,encoded_data,position+5,2);
                remaining=encoded_length-position-7;
            }
        }
        else if (encode_seq[counter]==2  && encoded_length-position>=4)
        {
            convert_dec_to_bin(15,encoded_data,position,4);
            remaining=encoded_length-position-4;
            if(encoded_length-position>=6)
            {
                convert_dec_to_bin(3,encoded_data,position+4,2);
                remaining=encoded_length-position-6;
            }
            if(encoded_length-position>=11)
            {
                convert_dec_to_bin(31,encoded_data,position+6,5);
                remaining=encoded_length-position-11;
            }
            if(encoded_length-position>=13)
            {
                convert_dec_to_bin(3,encoded_data,position+11,2);
                remaining=encoded_length-position-13;
            }
        }
        else if (encode_seq[counter]==5 && encoded_length-position>=6)
        {
            convert_dec_to_bin(63,encoded_data,position,6);
            remaining=encoded_length-position-6;
            if(encoded_length-position>=8)
            {
                convert_dec_to_bin(3,encoded_data,position+6,2);
                remaining=encoded_length-position-8;
            }
            if(encoded_length-position>=13)
            {
                convert_dec_to_bin(28,encoded_data,position+8,5);
                remaining=encoded_length-position-13;
            }
            if(encoded_length-position>=18)
            {
                convert_dec_to_bin(31,encoded_data,position+13,5);
                remaining=encoded_length-position-18;
            }
            if(encoded_length-position>=20)
            {
                convert_dec_to_bin(3,encoded_data,position+18,2);
                remaining=encoded_length-position-20;
            }
        }
        jab_int32 bit;
        for (jab_int32 i=encoded_length-remaining;i<encoded_length;i++)
        {
            bit=i%2;
            encoded_data->data[i]=(jab_char) bit;
        }
    }
    return encoded_data;
}

/**
 * @brief Encode metadata
 * @param enc the encode parameters
 * @param index the symbol index
 * @param coderate_params the wc and wr of the symbol
 * @param mask_index the mask reference
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean encodeMetadata(jab_encode* enc, jab_int32 index, jab_int32 *coderate_params, jab_int32 mask_index)
{
    jab_int32* metadata_coderate_params=(jab_int32 *)malloc(sizeof(jab_int32)*6);
    if(metadata_coderate_params == NULL){
        reportError("Memory allocation for metadata coderate parameter failed");
        return JAB_FAILURE;
    }
    jab_int32* metadata_part=(jab_int32 *)malloc(sizeof(jab_int32)*6);
    if(metadata_part == NULL){
        free(metadata_coderate_params);
        reportError("Memory allocation for metadta part failed");
        return JAB_FAILURE;
    }

    jab_int32 start_pos;
    jab_int32 d;
    if (enc->symbols[index].index==0) //if master symbol
    {
        //part I in master
        switch (enc->color_number) // set number of module colors
        {
            case 2:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 0;
                break;
            case 4:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 1;
                break;
            case 8:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 0;
                break;
            case 16:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 1;
                break;
            case 32:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 0;
                break;
            case 64:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 0;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 1;
                break;
            case 128:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 0;
                break;
            case 256:
                enc->symbols[index].encoded_metadata->data[0]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[1]=(jab_char) 1;
                enc->symbols[index].encoded_metadata->data[2]=(jab_char) 1;
                break;
        }
        metadata_coderate_params[0]=2;
        metadata_coderate_params[1]=-1;
        metadata_part[0]=0;
        metadata_part[1]=3;
        //part II in master symbol
        //variable SS, VF and V
        jab_byte is_square=0;
        jab_byte length_V=0;
        //part II SS
        if (enc->symbol_versions[0].x != enc->symbol_versions[0].y)
            enc->symbols[index].encoded_metadata->data[6]=(jab_char) 1;
        else
        {
            enc->symbols[index].encoded_metadata->data[6]=(jab_char) 0;
            is_square=1;
        }
        //part II VF
        jab_byte side_version_part=0;
        jab_int32 correction=0;
        if((is_square && enc->symbol_versions[0].x<5) || (!is_square && enc->symbol_versions[0].x<5 && enc->symbol_versions[0].y<5))
        {
            enc->symbols[index].encoded_metadata->data[7]=(jab_char) 0;
            enc->symbols[index].encoded_metadata->data[8]=(jab_char) 0;
            length_V=2;
            side_version_part=0;
        }
        else if((is_square && enc->symbol_versions[0].x<9) || (!is_square && enc->symbol_versions[0].x<9 && enc->symbol_versions[0].y<9))
        {
            enc->symbols[index].encoded_metadata->data[7]=(jab_char) 0;
            enc->symbols[index].encoded_metadata->data[8]=(jab_char) 1;
            if(!is_square)
			{
				length_V=3;
			}
            else
			{
				length_V=2;
				correction=4;
			}
            side_version_part=1;
        }
        else if((is_square && enc->symbol_versions[0].x<17) || (!is_square && enc->symbol_versions[0].x<17 && enc->symbol_versions[0].y<17))
        {
            enc->symbols[index].encoded_metadata->data[7]=(jab_char) 1;
            enc->symbols[index].encoded_metadata->data[8]=(jab_char) 0;
            if(!is_square)
			{
				length_V=4;
			}
            else
			{
				length_V=3;
				correction=8;
			}
            side_version_part=2;
        }
        else
        {
            enc->symbols[index].encoded_metadata->data[7]=(jab_char) 1;
            enc->symbols[index].encoded_metadata->data[8]=(jab_char) 1;
            if(!is_square)
			{
				length_V=5;
			}
            else
			{
				length_V=4;
				correction=16;
			}
            side_version_part=3;
        }

        //part III V
        jab_int32 counter_partIII=0;
        d = enc->symbol_versions[0].x-correction-1;
        start_pos = 20+counter_partIII;
        for(jab_int32 j=0;j<length_V;j++)
        {
            enc->symbols[index].encoded_metadata->data[start_pos+length_V-1-j]=(jab_char)d%2;
            d/=2;
            counter_partIII++;
        }
        if(!is_square)
        {
            start_pos=20+counter_partIII;
            d=enc->symbol_versions[0].y-1;
            for(jab_int32 j=0;j<length_V;j++)
            {
                enc->symbols[index].encoded_metadata->data[start_pos+length_V-1-j]=(jab_char)d%2;
                d/=2;
                counter_partIII++;
            }
        }
        //part II mask index MSK
        d=mask_index;
        for (jab_int32 j=0;j<3;j++)
        {
            enc->symbols[index].encoded_metadata->data[9+3-1-j]=(jab_char)d%2;
            d/=2;
        }
        //part II slave flag SF
        jab_byte slaves_selected=0;
        if (enc->symbol_number>1)
        {
            enc->symbols[index].encoded_metadata->data[12]=(jab_char) 1;
            slaves_selected=1;
        }
        else
            enc->symbols[index].encoded_metadata->data[12]=(jab_char) 0;
        //ldpc params for part II
        metadata_coderate_params[2]=2;
        metadata_coderate_params[3]=-1;
        metadata_part[2]=6;
        metadata_part[3]=13;
        //part III error correction parameter E
        start_pos=20+counter_partIII;
        d=coderate_params[0]-3;
        for (jab_int32 j=0;j<(10+2*side_version_part)/2;j++)
        {
            enc->symbols[index].encoded_metadata->data[start_pos+(10+2*side_version_part)/2-1-j]=(jab_char)d%2;
            d/=2;
            counter_partIII++;
        }
        start_pos=20+counter_partIII;
        d=coderate_params[1]-4;
        for (jab_int32 j=0;j<(10+2*side_version_part)/2;j++)
        {
            enc->symbols[index].encoded_metadata->data[start_pos+(10+2*side_version_part)/2-1-j]=(jab_char)d%2;
            d/=2;
            counter_partIII++;
        }
        //part III slave position S
		if(slaves_selected)
		{
			for(jab_int32 i=0; i<4; i++)
			{
				if(enc->docked_symbol[i] != 0)
					enc->symbols[index].encoded_metadata->data[20+counter_partIII]=(jab_char)1;
				else
					enc->symbols[index].encoded_metadata->data[20+counter_partIII]=(jab_char)0;
				counter_partIII++;
			}
		}
        //ldpc params for part III
        if(counter_partIII >= 36)
            metadata_coderate_params[4]=3;
        else
            metadata_coderate_params[4]=2;
        metadata_coderate_params[5]=-1;
        metadata_part[4]=20;
        metadata_part[5]=20+counter_partIII;
    }
    else //slave symbol
    {
        //part I in slave metadata
        //host symbol search
        jab_int32 host_symbol = 0;
        jab_int32 written_metadata = 0;
        for(jab_int32 i=0; i<4*enc->symbol_number; i++)
        {
            if(enc->docked_symbol[i] == index)
            {
                host_symbol = i / 4;
            }
        }
        //differnet shape and size as in host
        if(enc->symbols[index].side_size.x != enc->symbols[host_symbol].side_size.x || enc->symbols[index].side_size.y != enc->symbols[host_symbol].side_size.y)
        {
            enc->symbols[index].encoded_metadata->data[0] = (jab_char) 1;
            //write side version (not docked side) V in part II
            //find docked side
            jab_int32 dx = jab_decode_order[enc->symbol_positions[index]].x - jab_decode_order[enc->symbol_positions[host_symbol]].x;
            jab_int32 dy = jab_decode_order[enc->symbol_positions[index]].y - jab_decode_order[enc->symbol_positions[host_symbol]].y;
            if(dx == 0)
            {
                start_pos = 6 + written_metadata;
                d = enc->symbol_versions[index].y - 1;
                for(jab_int32 j=0; j<5; j++)
                {
                    enc->symbols[index].encoded_metadata->data[start_pos+5-1-j] = (jab_char)d%2;
                    d/=2;
                    written_metadata++;
                }
            }
            else if(dy == 0)
            {
                start_pos = 6 + written_metadata;
                d=enc->symbol_versions[index].x - 1;
                for (jab_int32 j=0; j<5; j++)
                {
                    enc->symbols[index].encoded_metadata->data[start_pos+5-1-j] = (jab_char)d%2;
                    d/=2;
                    written_metadata++;
                }
            }
        }
        else //same size and shape
            enc->symbols[index].encoded_metadata->data[0]=(jab_char) 0;

        //slave position SF
        if(enc->docked_symbol[4*index] > 0 || enc->docked_symbol[4*index + 1] > 0 || enc->docked_symbol[4*index + 2] >0 || enc->docked_symbol[4*index + 3] > 0)
            enc->symbols[index].encoded_metadata->data[2] = (jab_char) 1;
        else
            enc->symbols[index].encoded_metadata->data[2] = (jab_char) 0;
		//slave S in partII
        if(enc->symbols[index].encoded_metadata->data[2] > 0)
        {
			for(jab_int32 i=0; i<4; i++)
			{
				if(enc->docked_symbol[4*index + i] == -1)
					continue;
				if(enc->docked_symbol[4*index + i] != 0)
					enc->symbols[index].encoded_metadata->data[6+written_metadata]=(jab_char) 1;
				else
					enc->symbols[index].encoded_metadata->data[6+written_metadata]=(jab_char) 0;
				written_metadata++;
			}
        }
        //part I ldpc params
        metadata_coderate_params[0]=2;
        metadata_coderate_params[1]=-1;
        metadata_part[0]=0;
        metadata_part[1]=3;
        //part II ldpc
        metadata_coderate_params[2]=2;
        metadata_coderate_params[3]=-1;
        metadata_part[2]=6;
        metadata_part[3]=6+written_metadata;

        jab_int32 start_pos_partIII=6+2*written_metadata;
        written_metadata=0;
        //same wc and wr as host SE
        if(coderate_params[2*index]==coderate_params[2*host_symbol] && coderate_params[2*index+1]==coderate_params[2*host_symbol+1])
        {
            enc->symbols[index].encoded_metadata->data[1]=(jab_char) 0;
        }
        else //write wc and wr
        {
            enc->symbols[index].encoded_metadata->data[1]=(jab_char) 1;
            //pick length of E
            jab_int32 E=10;
            if(enc->symbols[index].side_size.x > 4 || enc->symbols[index].side_size.y > 4)
                E=12;
            if(enc->symbols[index].side_size.x > 8 || enc->symbols[index].side_size.y > 8)
                E=14;
            if(enc->symbols[index].side_size.x > 16 || enc->symbols[index].side_size.y > 16)
                E=16;
            start_pos = start_pos_partIII;
            d=coderate_params[2*index] - 3;
            for (jab_int32 j=0;j<E/2;j++)
            {
                enc->symbols[index].encoded_metadata->data[start_pos+E/2-1-j]=(jab_char)d%2;
                d/=2;
                written_metadata++;
            }
            start_pos = start_pos_partIII + written_metadata;
            d=coderate_params[2*index+1] - 4;
            for (jab_int32 j=0;j<E/2;j++)
            {
                enc->symbols[index].encoded_metadata->data[start_pos+E/2-1-j]=(jab_char)d%2;
                d/=2;
                written_metadata++;
            }
        }
        //part III ldpc
        if (written_metadata>=36)
            metadata_coderate_params[4]=3;
        else
            metadata_coderate_params[4]=2;
        metadata_coderate_params[5]=-1;
        metadata_part[4]=start_pos_partIII;
        metadata_part[5]=start_pos_partIII+written_metadata;
    }
    //call ldpc for metadata
    for (jab_int32 j=0;j<3;j++)
    {
        if(metadata_part[2*j+1]-metadata_part[2*j]>=3)
        {
            jab_data* ecc_encoded_metadata = encodeLDPC(enc->symbols[index].encoded_metadata,metadata_coderate_params,metadata_part,j);
            if(ecc_encoded_metadata == NULL)
            {
                reportError("LDPC encoding of metadata failed");
                return JAB_FAILURE;
            }
            jab_int32 loop=0;
            for (jab_int32 k=0;k<ecc_encoded_metadata->length;k++)
            {
                enc->symbols[index].encoded_metadata->data[metadata_part[2*j]+k]=ecc_encoded_metadata->data[loop];
                loop++;
            }
            free(ecc_encoded_metadata);
        }
    }
    free(metadata_part);
    free(metadata_coderate_params);
    return JAB_SUCCESS;
}

/**
 * @brief Place metadata after mask reference is determined
 * @param enc the encode parameter
 * @param palette_index the index in the color palette
*/
void placeMetadata(jab_encode* enc, jab_byte* palette_index)
{
    //rewrite metadata in master with mask information
    jab_int32 color_number_metadata = enc->color_number > 8 ? 8 : enc->color_number;
    jab_int32 nb_of_bits_per_metadata_mod=log(color_number_metadata)/log(2);
    jab_int32 written_metadata_part=0,module_count=0;
    jab_int32 x = MASTER_METADATA_X;
    jab_int32 y = MASTER_METADATA_Y;
    jab_int32 index=0;

    for(jab_int32 i=0;i<enc->symbols[index].encoded_metadata->length;i++)
    {
        jab_int32 index_color=0;
        if(i<6)
        {
            index_color=((jab_int32)enc->symbols[index].encoded_metadata->data[written_metadata_part])*(color_number_metadata-1);
            written_metadata_part++;
        }
        else
        {
            for(jab_int32 j=0;j<nb_of_bits_per_metadata_mod;j++)
            {
                if(written_metadata_part < enc->symbols[index].encoded_metadata->length)
                    index_color+=((jab_int32)enc->symbols[index].encoded_metadata->data[written_metadata_part]) << (nb_of_bits_per_metadata_mod-1-j); //*pow(2,nb_of_bits_per_metadata_mod-1-j);
                else //write binary zero if metadata finished but space of module not used completely
                    index_color+=0;
                written_metadata_part++;
                i++;
            }
            i--;
        }
        enc->symbols[index].matrix[y*enc->symbols[index].side_size.x+x]=palette_index[index_color];
        enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x]=0;
        module_count++;
        getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
    }
}

/**
 * @brief Interleave color palette
 * @param index the color index in the palette
 * @param size the color palette size
 * @param color_number the number of colors
*/
void interleaveColorPalette(jab_byte* index, jab_int32 size, jab_int32 color_number)
{
	if(size < 16 || size > 256) return;

	jab_byte tmp[color_number];
	for(jab_int32 i=0; i<color_number; i++)
		tmp[i] = i;

	if(color_number == 16)
	{
		memcpy(index + 4, tmp + 12, 4);
		memcpy(index + 8, tmp + 4, 8);
	}
	else if(color_number == 32)
	{
		memcpy(index + 2, tmp + 6, 2);
		memcpy(index + 4, tmp + 24, 2);
		memcpy(index + 6, tmp + 30, 2);

		memcpy(index + 8,  tmp + 2, 4);
		memcpy(index + 12, tmp + 8, 16);
		memcpy(index + 28, tmp + 26, 4);
	}
	else if(color_number == 64)
	{
		memcpy(index + 1, tmp + 3,  1);
		memcpy(index + 2, tmp + 12, 1);
		memcpy(index + 3, tmp + 15, 1);
		memcpy(index + 4, tmp + 48, 1);
		memcpy(index + 5, tmp + 51, 1);
		memcpy(index + 6, tmp + 60, 1);
		memcpy(index + 7, tmp + 63, 1);

		memcpy(index + 8,  tmp + 1, 2);
		memcpy(index + 10, tmp + 4, 8);
		memcpy(index + 18, tmp + 13, 2);
		memcpy(index + 20, tmp + 16, 32);
		memcpy(index + 52, tmp + 49, 2);
		memcpy(index + 54, tmp + 52, 8);
		memcpy(index + 62, tmp + 61, 2);
	}
	else if(color_number == 128)
	{
		memcpy(index + 1, tmp + 3,  1);
		memcpy(index + 2, tmp + 12, 1);
		memcpy(index + 3, tmp + 15, 1);
		memcpy(index + 4, tmp + 112, 1);
		memcpy(index + 5, tmp + 115, 1);
		memcpy(index + 6, tmp + 124, 1);
		memcpy(index + 7, tmp + 127, 1);

		memcpy(index + 8,  tmp + 1, 2);
		memcpy(index + 10, tmp + 4, 8);
		memcpy(index + 18, tmp + 13, 2);
		memcpy(index + 20, tmp + 32, 16);
		memcpy(index + 36, tmp + 80, 16);
		memcpy(index + 52, tmp + 113, 2);
		memcpy(index + 54, tmp + 116, 8);
		memcpy(index + 62, tmp + 125, 2);
	}
	else if(color_number == 256)
	{
		memcpy(index + 1, tmp + 3,  1);
		memcpy(index + 2, tmp + 28, 1);
		memcpy(index + 3, tmp + 31, 1);
		memcpy(index + 4, tmp + 224, 1);
		memcpy(index + 5, tmp + 227, 1);
		memcpy(index + 6, tmp + 252, 1);
		memcpy(index + 7, tmp + 255, 1);

		memcpy(index + 8,  tmp + 1,  2);
		memcpy(index + 10, tmp + 8,  4);
		memcpy(index + 14, tmp + 20, 4);
		memcpy(index + 18, tmp + 29, 2);

		memcpy(index + 20, tmp + 64, 4);
		memcpy(index + 24, tmp + 72, 4);
		memcpy(index + 28, tmp + 84, 4);
		memcpy(index + 32, tmp + 92, 4);

		memcpy(index + 36, tmp + 160, 4);
		memcpy(index + 40, tmp + 168, 4);
		memcpy(index + 44, tmp + 180, 4);
		memcpy(index + 48, tmp + 188, 4);

		memcpy(index + 52, tmp + 225, 2);
		memcpy(index + 54, tmp + 232, 4);
		memcpy(index + 58, tmp + 244, 4);
		memcpy(index + 62, tmp + 253, 2);
	}
}

/**
 * @brief Create symbol matrix
 * @param enc the encode parameter
 * @param index the symbol index
 * @param ecc_encoded_data encoded data
 * @param palette_index the index of the color palette
*/
void createMatrix(jab_encode* enc, jab_int32 index, jab_data* ecc_encoded_data, jab_byte* palette_index)
{
    //Allocate matrix
    enc->symbols[index].matrix = (jab_byte *)calloc(enc->symbols[index].side_size.x * enc->symbols[index].side_size.y, sizeof(jab_byte));
    if(enc->symbols[index].matrix == NULL)
    {
        reportError("Memory allocation for symbol matrix failed");
        return;
    }

    //Allocate boolean matrix
    enc->symbols[index].data_map = (jab_byte *)malloc(enc->symbols[index].side_size.x * enc->symbols[index].side_size.y * sizeof(jab_byte));
    if(!enc->symbols[index].data_map)
    {
        reportError("Memory allocation for boolean matrix failed");
        return;
    }
    memset(enc->symbols[index].data_map, 1, enc->symbols[index].side_size.x * enc->symbols[index].side_size.y * sizeof(jab_byte));

    //calculate number of alignment pattern depending on the side_size and their positions
    jab_int32 number_of_alignment_pattern_x = (enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER*2-1))/MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS-1;
    jab_int32 number_of_alignment_pattern_y = (enc->symbols[index].side_size.y-(DISTANCE_TO_BORDER*2-1))/MINIMUM_DISTANCE_BETWEEN_ALIGNMENTS-1;
    if (number_of_alignment_pattern_x < 0)
        number_of_alignment_pattern_x=0;
    if (number_of_alignment_pattern_y < 0)
        number_of_alignment_pattern_y=0;
    jab_float alignment_distance_x;
    jab_float alignment_distance_y;
    if(number_of_alignment_pattern_x != 0)
        alignment_distance_x = (jab_float)(enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER*2-1))/((jab_float)number_of_alignment_pattern_x+1.0f);
    else
        alignment_distance_x = (jab_float)(enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER*2-1));
    if(number_of_alignment_pattern_y!=0)
        alignment_distance_y = (jab_float)(enc->symbols[index].side_size.y-(DISTANCE_TO_BORDER*2-1))/((jab_float)number_of_alignment_pattern_y+1.0f);
    else
        alignment_distance_y = (jab_float)(enc->symbols[index].side_size.y-(DISTANCE_TO_BORDER*2-1));

    jab_int32 x_offset;
    jab_int32 y_offset;
    jab_byte left;
    //set alignment patterns
    jab_byte apx_core_color_index, apx_peri_color_index;
    if(enc->color_number ==2)
	{
		apx_core_color_index = enc->color_number - 1;
		apx_peri_color_index = 0;
	}
	else if(enc->color_number == 4)
	{
		apx_core_color_index = enc->color_number - 1;
		apx_peri_color_index = 0;
	}
	else
	{
		apx_core_color_index = palette_index[APX_CORE_COLOR];
		apx_peri_color_index = palette_index[7 - APX_CORE_COLOR];
	}
    for(jab_int32 i=0;i<=number_of_alignment_pattern_x+1;i++)
    {
        if (i%2==1 )
            left=0;
        else
            left=1;
        for(jab_int32 j=0;j<=number_of_alignment_pattern_y+1;j++)
        {
            x_offset=DISTANCE_TO_BORDER+i*alignment_distance_x;
            y_offset=DISTANCE_TO_BORDER+j*alignment_distance_y;
            //left alignment patterns
            if((i != 0 || j!=0) && (i != 0 || j!=(number_of_alignment_pattern_y+1)) && (i != (number_of_alignment_pattern_x+1) || j!=0) && (i != number_of_alignment_pattern_x+1 || j!=number_of_alignment_pattern_y+1) && left==1)
            {
                 enc->symbols[index].matrix[(y_offset-2)        *enc->symbols[index].side_size.x    +x_offset-2 ]=
                        enc->symbols[index].matrix[(y_offset-2) *enc->symbols[index].side_size.x    +x_offset-1 ]=
                        enc->symbols[index].matrix[(y_offset-1) *enc->symbols[index].side_size.x    +x_offset-2 ]=
                        enc->symbols[index].matrix[(y_offset-1) *enc->symbols[index].side_size.x    +x_offset   ]=
                        enc->symbols[index].matrix[y_offset     *enc->symbols[index].side_size.x    +x_offset-1 ]=
                        enc->symbols[index].matrix[y_offset     *enc->symbols[index].side_size.x    +x_offset   ]=apx_peri_color_index;//palette_index[7 - APX_CORE_COLOR];//0;
                 enc->symbols[index].matrix[(y_offset-1)        *enc->symbols[index].side_size.x    +x_offset-1 ]=apx_core_color_index;//palette_index[APX_CORE_COLOR];//7;

                 enc->symbols[index].data_map[(y_offset-2)        *enc->symbols[index].side_size.x    +x_offset-2 ]=
                         enc->symbols[index].data_map[(y_offset-2)*enc->symbols[index].side_size.x    +x_offset-1 ]=
                         enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x    +x_offset-2 ]=
                         enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x    +x_offset   ]=
                         enc->symbols[index].data_map[y_offset    *enc->symbols[index].side_size.x    +x_offset-1 ]=
                         enc->symbols[index].data_map[y_offset    *enc->symbols[index].side_size.x    +x_offset   ]=
                         enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x    +x_offset-1 ]=0;
            }
            //right alignment patterns
            else if((i != 0 || j!=0) && (i != 0 || j!=(number_of_alignment_pattern_y+1)) && (i != (number_of_alignment_pattern_x+1) || j!=0) && (i != number_of_alignment_pattern_x+1 || j!=number_of_alignment_pattern_y+1) && left==0)
            {
                enc->symbols[index].matrix[(y_offset-2)        *enc->symbols[index].side_size.x    +x_offset   ]=
                       enc->symbols[index].matrix[(y_offset-2) *enc->symbols[index].side_size.x    +x_offset-1 ]=
                       enc->symbols[index].matrix[(y_offset-1) *enc->symbols[index].side_size.x    +x_offset-2 ]=
                       enc->symbols[index].matrix[(y_offset-1) *enc->symbols[index].side_size.x    +x_offset   ]=
                       enc->symbols[index].matrix[y_offset     *enc->symbols[index].side_size.x    +x_offset-1 ]=
                       enc->symbols[index].matrix[y_offset     *enc->symbols[index].side_size.x    +x_offset-2 ]=apx_peri_color_index;//palette_index[7 - APX_CORE_COLOR];//0;
                enc->symbols[index].matrix[(y_offset-1)        *enc->symbols[index].side_size.x    +x_offset-1 ]=apx_core_color_index;//palette_index[APX_CORE_COLOR];//7;

                enc->symbols[index].data_map[(y_offset-2)        *enc->symbols[index].side_size.x    +x_offset   ]=
                        enc->symbols[index].data_map[(y_offset-2)*enc->symbols[index].side_size.x    +x_offset-1 ]=
                        enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x    +x_offset-2 ]=
                        enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x    +x_offset   ]=
                        enc->symbols[index].data_map[y_offset    *enc->symbols[index].side_size.x    +x_offset-1 ]=
                        enc->symbols[index].data_map[y_offset    *enc->symbols[index].side_size.x    +x_offset-2 ]=
                        enc->symbols[index].data_map[(y_offset-1)*enc->symbols[index].side_size.x    +x_offset-1 ]=0;
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
                        if(enc->color_number == 2)
						{
							fp0_color_index = palette_index[abs((k%2)-0)];
							fp1_color_index = palette_index[abs((k%2)-1)];
							fp2_color_index = palette_index[abs((k%2)-1)];
							fp3_color_index = palette_index[abs((k%2)-1)];
						}
                        else if(enc->color_number == 4 )
                        {
                        	fp0_color_index = palette_index[abs((k%2)*3-0)];
							fp1_color_index = palette_index[abs((k%2)*3-1)];
							fp2_color_index = palette_index[abs((k%2)*3-2)];
							fp3_color_index = palette_index[abs((k%2)*3-3)];
                        }
                        else
						{
							fp0_color_index = palette_index[abs((k%2)*7-FP0_CORE_COLOR)];
							fp1_color_index = palette_index[abs((k%2)*7-FP1_CORE_COLOR)];
							fp2_color_index = palette_index[abs((k%2)*7-FP2_CORE_COLOR)];
							fp3_color_index = palette_index[abs((k%2)*7-FP3_CORE_COLOR)];
						}
						//upper pattern
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
                                enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=fp0_color_index; //enc->color_number > 2 ? palette_index[abs((k%2)*7-FP0_CORE_COLOR)] : palette_index[abs((k%2)*7-FP0_CORE_COLOR_BW)];//2+(k%2)*3;
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
                                enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=0;
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=fp1_color_index;//enc->color_number > 2 ? palette_index[abs((k%2)*7-FP1_CORE_COLOR)] : palette_index[abs((k%2)*7-FPX_CORE_COLOR_BW)];//4-(k%2);
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;
                        //lower pattern
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=fp2_color_index;//enc->color_number > 2 ? palette_index[abs((k%2)*7-FP2_CORE_COLOR)] : palette_index[abs((k%2)*7-FPX_CORE_COLOR_BW)];//1+(k%2)*5;
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)-j-1]=
                                enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)+j-1]=fp3_color_index;//enc->color_number > 2 ? palette_index[abs((k%2)*7-FP3_CORE_COLOR)] : palette_index[abs((k%2)*7-FPX_CORE_COLOR_BW)];//6-(k%2)*5;
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
                        if(enc->color_number == 2)
						{
							ap0_color_index = palette_index[abs((k%2)-0)];
							ap1_color_index = palette_index[abs((k%2)-0)];
							ap2_color_index = palette_index[abs((k%2)-0)];
							ap3_color_index = palette_index[abs((k%2)-0)];
						}
                        else if(enc->color_number == 4 )
                        {
                        	ap0_color_index = palette_index[abs((k%2)*3-0)];
							ap1_color_index = palette_index[abs((k%2)*3-0)];
							ap2_color_index = palette_index[abs((k%2)*3-0)];
							ap3_color_index = palette_index[abs((k%2)*3-0)];
                        }
                        else
						{
							ap0_color_index = palette_index[abs((k%2)*7-AP0_CORE_COLOR)];
							ap1_color_index = palette_index[abs((k%2)*7-AP1_CORE_COLOR)];
							ap2_color_index = palette_index[abs((k%2)*7-AP2_CORE_COLOR)];
							ap3_color_index = palette_index[abs((k%2)*7-AP3_CORE_COLOR)];
						}
                        //upper pattern
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
                                enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=ap0_color_index;//abs((k%2)*7-AP0_CORE_COLOR);//5-(k%2)*3;
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER-j-1]=
                                enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+DISTANCE_TO_BORDER+j-1]=0;
                        enc->symbols[index].matrix[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].matrix[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=ap1_color_index;//abs((k%2)*7-AP1_CORE_COLOR);//3+(k%2);
                        enc->symbols[index].data_map[(DISTANCE_TO_BORDER-(i+1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].data_map[(DISTANCE_TO_BORDER+(i-1))*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;
                        //lower pattern
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=ap2_color_index;//abs((k%2)*7-AP2_CORE_COLOR);//5-(k%2)*3;
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)-j-1]=
                                enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+enc->symbols[index].side_size.x-(DISTANCE_TO_BORDER-1)+j-1]=0;
                        enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)-j-1]=
                                enc->symbols[index].matrix[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)+j-1]=ap3_color_index;//abs((k%2)*7-AP3_CORE_COLOR);//3+(k%2);
                        enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER+i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)-j-1]=
                                enc->symbols[index].data_map[(enc->symbols[index].side_size.y-DISTANCE_TO_BORDER-i)*enc->symbols[index].side_size.x+(DISTANCE_TO_BORDER)+j-1]=0;
                    }
                }
            }
        }
    }

    //Metadata and color palette placement
    jab_int32 nb_of_bits_per_mod=log(enc->color_number)/log(2);
    jab_int32 color_number_metadata = enc->color_number > 8 ? 8 : enc->color_number;
    jab_int32 nb_of_bits_per_metadata_mod=log(color_number_metadata)/log(2);
    jab_int32 written_metadata_part=0,module_count=0;
    jab_int32 index_color;
    jab_int32 x;
    jab_int32 y;
    if (index==0)
    {
        x = MASTER_METADATA_X;
        y = MASTER_METADATA_Y;
        for(jab_int32 i=0;i<enc->symbols[index].encoded_metadata->length;i++)
        {
            index_color=0;
            if(i<6)
            {
                index_color=((jab_int32)enc->symbols[index].encoded_metadata->data[written_metadata_part])*(color_number_metadata-1);
                written_metadata_part++;
            }
            else
            {
                for(jab_int32 j=0;j<nb_of_bits_per_metadata_mod;j++)
                {
                    if(written_metadata_part < enc->symbols[index].encoded_metadata->length)
                        index_color+=((jab_int32)enc->symbols[index].encoded_metadata->data[written_metadata_part]) << (nb_of_bits_per_metadata_mod-1-j); //*pow(2,nb_of_bits_per_metadata_mod-1-j);
                    else //write binary zero if metadata finished but space of module not used completely
                        index_color+=0;
                    written_metadata_part++;
                    i++;
                }
                i--;
            }
            enc->symbols[index].matrix[y*enc->symbols[index].side_size.x+x]=palette_index[index_color];
            enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x]=0;
            module_count++;
            getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
        }
		//color palette
		//write the first 16 colors
		jab_int32 width=enc->symbols[index].side_size.x;
		jab_int32 height=enc->symbols[index].side_size.y;
		jab_int32 max_color=enc->color_number > 16 ? 16 : enc->color_number;
		jab_int32 offset=0;
		jab_int32 lopp_index=0;
		for(jab_int32 i=0;i<max_color;i++)
		{
            if(i==8)
			{
				offset=width-7;
				lopp_index=0;
			}
            //upper
			enc->symbols[index].matrix[master_palette_position[lopp_index].x+offset+master_palette_position[lopp_index].y*width]=palette_index[i];
			enc->symbols[index].data_map[master_palette_position[lopp_index].x+offset+master_palette_position[lopp_index].y*width]=0;
            //lower
			enc->symbols[index].matrix[width-1-master_palette_position[lopp_index].x-offset+(master_palette_position[lopp_index].y+height-7)*width]=palette_index[i];
			enc->symbols[index].data_map[width-1-master_palette_position[lopp_index].x-offset+(master_palette_position[lopp_index].y+height-7)*width]=0;
			lopp_index++;
		}
        if(enc->color_number > 16)
        {
            for(jab_int32 i=16; i<(enc->color_number > 64 ? 64 : enc->color_number)-1; i+=2)
            {
                enc->symbols[index].matrix[y*enc->symbols[index].side_size.x+x]=palette_index[i];
                enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x]=0;
                module_count++;
                getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
                enc->symbols[index].matrix[y*enc->symbols[index].side_size.x+x]=palette_index[i+1];
                enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x]=0;
                module_count++;
                getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
                enc->symbols[index].matrix[y*enc->symbols[index].side_size.x+x]=palette_index[i];
                enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x]=0;
                module_count++;
                getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
                enc->symbols[index].matrix[y*enc->symbols[index].side_size.x+x]=palette_index[i+1];
                enc->symbols[index].data_map[y*enc->symbols[index].side_size.x+x]=0;
                module_count++;
                getNextMetadataModuleInMaster(enc->symbols[index].side_size.y, enc->symbols[index].side_size.x, module_count, &x, &y);
            }
        }
    }
    else//Metadata slave symbol
    {
        x = SLAVE_METADATA_X;
        y = SLAVE_METADATA_Y;
        jab_int32 host_symbol = 0;
        jab_int32 width=enc->symbols[index].side_size.x;
        jab_int32 height=enc->symbols[index].side_size.y;
        module_count=0;
        for (jab_int32 i=0;i<4*enc->symbol_number;i++)
        {
            if (enc->docked_symbol[i]==index)
                host_symbol = i/4;
        }
        jab_int32 mod_x = 0;
        jab_int32 mod_y = 0;
        jab_int32 dx=jab_decode_order[enc->symbol_positions[index]].x-jab_decode_order[enc->symbol_positions[host_symbol]].x;
        jab_int32 dy=jab_decode_order[enc->symbol_positions[index]].y-jab_decode_order[enc->symbol_positions[host_symbol]].y;

        for(jab_int32 i=0;i<enc->symbols[index].encoded_metadata->length;i++)
        {
            index_color=0;
            for(jab_int32 j=0;j<nb_of_bits_per_metadata_mod;j++)
            {
                if(written_metadata_part < enc->symbols[index].encoded_metadata->length)
                    index_color+=((jab_int32)enc->symbols[index].encoded_metadata->data[written_metadata_part]) << (nb_of_bits_per_metadata_mod-1-j); //*pow(2,nb_of_bits_per_metadata_mod-1-j);
                else //write binary zero if metadata finished but space of module not used completely
                    index_color+=0;
                written_metadata_part++;
                i++;
            }
            i--;
            if(dx==0 && dy>0) //top
            {
                mod_x=width-1-y;
                mod_y=x;
            }
            else if (dx==0 && dy<0) //bottom
            {
                mod_x=y;
                mod_y=height-1-x;
            }
            else if (dx>0 && dy==0) //left
            {
                mod_x=x;
                mod_y=y;
            }
            else if (dx<0 && dy==0) //right
            {
                mod_x=width-1-x;
                mod_y=height-1-y;
            }
            enc->symbols[index].matrix[mod_x+mod_y*width]=palette_index[index_color];
            enc->symbols[index].data_map[mod_x+mod_y*width]=0;
            module_count++;
            getNextMetadataModuleInSlave(module_count, &x, &y);
        }
        //color palette
        for (jab_int32 i=0;i<(enc->color_number > 64 ? 64 : enc->color_number)/2;i++)
        {
            if(dx==0 && dy>0 || dx==0 && dy<0) //top bottom
            {
                enc->symbols[index].matrix[width-1-slave_palette_position[i].y+slave_palette_position[i].x*width]=palette_index[i];//index_color;
                enc->symbols[index].data_map[width-1-slave_palette_position[i].y+slave_palette_position[i].x*width]=0;
                enc->symbols[index].matrix[slave_palette_position[i].y+(height-1-slave_palette_position[i].x)*width]=palette_index[i];//index_color;
                enc->symbols[index].data_map[slave_palette_position[i].y+(height-1-slave_palette_position[i].x)*width]=0;
                //left right
                if (enc->color_number > 8)
                {
                    enc->symbols[index].matrix[slave_palette_position[i].x+slave_palette_position[i].y*width]=palette_index[i+(enc->color_number > 64 ? 64 : enc->color_number)/2];//index_color;
                    enc->symbols[index].data_map[slave_palette_position[i].x+slave_palette_position[i].y*width]=0;
                    enc->symbols[index].matrix[(width-1-slave_palette_position[i].x)+(height-1-slave_palette_position[i].y)*width]=palette_index[i+(enc->color_number > 64 ? 64 : enc->color_number)/2];//index_color;
                    enc->symbols[index].data_map[(width-1-slave_palette_position[i].x)+(height-1-slave_palette_position[i].y)*width]=0;
                }
                else
                {
                    enc->symbols[index].matrix[width-1-slave_palette_position[i+enc->color_number/2].y+slave_palette_position[i+enc->color_number/2].x*width]=palette_index[i+enc->color_number/2];//index_color;
                    enc->symbols[index].data_map[width-1-slave_palette_position[i+enc->color_number/2].y+slave_palette_position[i+enc->color_number/2].x*width]=0;
                    enc->symbols[index].matrix[slave_palette_position[i+enc->color_number/2].y+(height-1-slave_palette_position[i+enc->color_number/2].x)*width]=palette_index[i+enc->color_number/2];//index_color;
                    enc->symbols[index].data_map[slave_palette_position[i+enc->color_number/2].y+(height-1-slave_palette_position[i+enc->color_number/2].x)*width]=0;
                }
            }
            else if (dx>0 && dy==0 || dx<0 && dy==0) //left right
            {
                enc->symbols[index].matrix[slave_palette_position[i].x+slave_palette_position[i].y*width]=palette_index[i];//index_color;
                enc->symbols[index].data_map[slave_palette_position[i].x+slave_palette_position[i].y*width]=0;
                enc->symbols[index].matrix[(width-1-slave_palette_position[i].x)+(height-1-slave_palette_position[i].y)*width]=palette_index[i];//index_color;
                enc->symbols[index].data_map[(width-1-slave_palette_position[i].x)+(height-1-slave_palette_position[i].y)*width]=0;
                //top bottom
                if (enc->color_number > 8)
                {
                    enc->symbols[index].matrix[width-1-slave_palette_position[i].y+slave_palette_position[i].x*width]=palette_index[i+(enc->color_number > 64 ? 64 : enc->color_number)/2];//index_color;
                    enc->symbols[index].data_map[width-1-slave_palette_position[i].y+slave_palette_position[i].x*width]=0;
                    enc->symbols[index].matrix[slave_palette_position[i].y+(height-1-slave_palette_position[i].x)*width]=palette_index[i+(enc->color_number > 64 ? 64 : enc->color_number)/2];//index_color;
                    enc->symbols[index].data_map[slave_palette_position[i].y+(height-1-slave_palette_position[i].x)*width]=0;
                }
                else
                {
                    enc->symbols[index].matrix[slave_palette_position[i+enc->color_number/2].x+slave_palette_position[i+enc->color_number/2].y*width]=palette_index[i+enc->color_number/2];//index_color;
                    enc->symbols[index].data_map[slave_palette_position[i+enc->color_number/2].x+slave_palette_position[i+enc->color_number/2].y*width]=0;
                    enc->symbols[index].matrix[(width-1-slave_palette_position[i+enc->color_number/2].x)+(height-1-slave_palette_position[i+enc->color_number/2].y)*width]=palette_index[i+enc->color_number/2];//index_color;
                    enc->symbols[index].data_map[(width-1-slave_palette_position[i+enc->color_number/2].x)+(height-1-slave_palette_position[i+enc->color_number/2].y)*width]=0;
                }
            }
        }
    }
#if TEST_MODE
	FILE* fp = fopen("enc_module_data.bin", "wb");
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
                index_color=0;
                for(jab_int32 j=0;j<nb_of_bits_per_mod;j++)
                {
                    if(written_mess_part<ecc_encoded_data->length)
                        index_color+=((jab_int32)ecc_encoded_data->data[written_mess_part]) << (nb_of_bits_per_mod-1-j);//*pow(2,nb_of_bits_per_mod-1-j);
                    else
                    {
                        index_color+=padding << (nb_of_bits_per_mod-1-j);//*pow(2,nb_of_bits_per_mod-1-j);
                        if (padding==0)
                            padding=1;
                        else
                            padding=0;
                    }
                    written_mess_part++;
                }
                enc->symbols[index].matrix[i]=(jab_char)index_color;//i % enc->color_number;
#if TEST_MODE
				fwrite(&enc->symbols[index].matrix[i], 1, 1, fp);
#endif // TEST_MODE
            }
            else if(enc->symbols[index].data_map[i]!=0) //write padding bits
            {
                index_color=0;
                for(jab_int32 j=0;j<nb_of_bits_per_mod;j++)
                {
                    index_color+=padding << (nb_of_bits_per_mod-1-j);//*pow(2,nb_of_bits_per_mod-1-j);
                    if (padding==0)
                        padding=1;
                    else
                        padding=0;
                }
                enc->symbols[index].matrix[i]=(jab_char)index_color;//i % enc->color_number;
#if TEST_MODE
				fwrite(&enc->symbols[index].matrix[i], 1, 1, fp);
#endif // TEST_MODE
            }
        }
    }
#if TEST_MODE
	fclose(fp);
#endif // TEST_MODE
}

/**
 * @brief Determine symbol metadata length
 * @param enc the encode parameters
 * @param index the symbol index
*/
void getMetadataLength(jab_encode* enc, jab_int32 index)
{
    //calculate metadata length in modules
    jab_int32 length=0;
    jab_int32 lengthPart=0;
    jab_int32 numberOfColorsInMetadata=enc->color_number;
    jab_int32 longerSide=0;
    if (numberOfColorsInMetadata>8)
        numberOfColorsInMetadata=8;

    if (index==0) //if master symbol
    {
        //part II in master symbol
        length+=14; //part II in master -> 14 bit represented with max 8 color
        //part III
        //slaves choosed
        lengthPart=enc->symbol_number>1 ? lengthPart+4 : lengthPart;
        //user defined error correction level change length
        longerSide= enc->symbol_versions[index].x > enc->symbol_versions[index].y ? enc->symbol_versions[index].x : enc->symbol_versions[index].y;
        if (enc->ecc_levels[index]>-1)
        {
            if (longerSide<5)
                lengthPart+=10;
            else if (longerSide<9)
                lengthPart+=12;
            else if (longerSide<17)
                lengthPart+=14;
            else
                lengthPart+=16;
        }
        //side version space dependend of symbol shape
        //Rectangle symbol
        if (enc->symbol_versions[index].x!=enc->symbol_versions[index].y)
        {
            if (longerSide<5)
                lengthPart+=4;
            else if (longerSide<9)
                lengthPart+=6;
            else if (longerSide<17)
                lengthPart+=8;
            else
                lengthPart+=10;
        }
        else //Square symbol
        {
            if (longerSide<9)
                lengthPart+=2;
            else if (longerSide<17)
                lengthPart+=3;
            else
                lengthPart+=4;
        }
        //consider ldpc length
        if (lengthPart==2)
            lengthPart+=3;
        else
            lengthPart*=2;
        //metadata length in modules is dependend on number of colors
        length+=lengthPart;
        length+=6;//part I in master symbol -> always 6 moduls
    }
    else //slave symbol
    {
        //part I
        length+=6;
        //part II
        lengthPart=0;
        //host symbol search
        jab_int32 host_symbol = 0;
        for(jab_int32 i=0;i<4*enc->symbol_number;i++)
        {
            if (enc->docked_symbol[i]==index)
                host_symbol = i/4;
        }
        //symbol shape and size vary compared to host symbol
        if (enc->symbol_versions[index].x != enc->symbol_versions[host_symbol].x || enc->symbol_versions[index].y != enc->symbol_versions[host_symbol].y)
            lengthPart+=10;
        //Parameter S
        jab_int32 numberOfDockedSymbols=0;
        //number of docked symbols
		for(jab_int32 i=0; i<4; i++)
        {
            if(enc->docked_symbol[4*index + i] > 0)
                numberOfDockedSymbols++;
        }
        if (numberOfDockedSymbols!=0)
            lengthPart+=6;

        length+=lengthPart;

        //part III in slave symbol
        lengthPart=0;
        //use other ecc level than host symbol
        if (enc->ecc_levels[index] != enc->ecc_levels[host_symbol])
        {
            lengthPart+=10;
            if(enc->symbol_versions[index].x>4 || enc->symbol_versions[index].y>4)
                lengthPart+=2;
            if(enc->symbol_versions[index].x>8 || enc->symbol_versions[index].y>8)
                lengthPart+=2;
            if(enc->symbol_versions[index].x>16 || enc->symbol_versions[index].y>16)
                lengthPart+=2;
        }
        length+=lengthPart;
    }
    jab_int32 size_metadata;
    if (index==0)
        size_metadata=80;
    else
        size_metadata=54;
    if(enc->symbols[index].encoded_metadata==NULL)
    {
        enc->symbols[index].encoded_metadata = (jab_data *)malloc(sizeof(jab_data) + size_metadata*sizeof(jab_char));
        if(enc->symbols[index].encoded_metadata == NULL)
        {
            reportError("Memory allocation for metadata failed");
            return;
        }
    }
    enc->symbols[index].encoded_metadata->length=length;
}

/**
 * @brief Assign docked symbols to their hosts
 * @param enc the encode parameters
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean assignDockedSymbols(jab_encode* enc)
{
    //docked symbol search
    jab_boolean* isDockedSymbol;
    isDockedSymbol = (jab_boolean *)malloc(enc->symbol_number * sizeof(jab_boolean));
    if(!isDockedSymbol)
    {
        reportError("Memory allocation for boolean array failed");
        return 0;
    }
    memset(isDockedSymbol, 1, enc->symbol_number * sizeof(jab_boolean));
    //start with master
    isDockedSymbol[0]=0;
    for(jab_int32 i=0;i<enc->symbol_number-1;i++)
    {
        for(jab_int32 k=i+1;k<enc->symbol_number;k++)
        {
			if(isDockedSymbol[k] == 1)
			{
				jab_int32 hpos = enc->symbol_positions[i];
				jab_int32 spos = enc->symbol_positions[k];
				if(jab_decode_order[hpos].x == jab_decode_order[spos].x)
				{
					if(jab_decode_order[hpos].y - 1 == jab_decode_order[spos].y)
					{
						isDockedSymbol[k] = 0;
						enc->docked_symbol[i*4 + 0] = k;
						enc->docked_symbol[k*4 + 1] = -1;
					}
					else if(jab_decode_order[hpos].y + 1 == jab_decode_order[spos].y)
					{
						isDockedSymbol[k] = 0;
						enc->docked_symbol[i*4 + 1] = k;
						enc->docked_symbol[k*4 + 0] = -1;
					}
				}
				if(jab_decode_order[hpos].y == jab_decode_order[spos].y)
				{
					if(jab_decode_order[hpos].x - 1 == jab_decode_order[spos].x)
					{
						isDockedSymbol[k] = 0;
						enc->docked_symbol[i*4 + 2] = k;
						enc->docked_symbol[k*4 + 3] = -1;
					}
					else if(jab_decode_order[hpos].x + 1 == jab_decode_order[spos].x)
					{
						isDockedSymbol[k] = 0;
						enc->docked_symbol[i*4 + 3] = k;
						enc->docked_symbol[k*4 + 2] = -1;
					}
				}
			}
        }
    }
    //check if there is undocked symbol
    for(jab_int32 i=0; i<enc->symbol_number; i++)
    {
		if(isDockedSymbol[i] == 1)
		{
			JAB_REPORT_ERROR(("Slave symbol at position %d has no host", enc->symbol_positions[i]))
			free(isDockedSymbol);
			return JAB_FAILURE;
		}
    }
    free(isDockedSymbol);
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
        if(jab_decode_order[enc->symbol_positions[i]].x < cp->min_x)
            cp->min_x = jab_decode_order[enc->symbol_positions[i]].x;
        if(jab_decode_order[enc->symbol_positions[i]].y < cp->min_y)
            cp->min_y = jab_decode_order[enc->symbol_positions[i]].y;
        //find the maximal x and y
        if(jab_decode_order[enc->symbol_positions[i]].x > max_x)
            max_x = jab_decode_order[enc->symbol_positions[i]].x;
        if(jab_decode_order[enc->symbol_positions[i]].y > max_y)
            max_y = jab_decode_order[enc->symbol_positions[i]].y;
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
            if(jab_decode_order[enc->symbol_positions[i]].x == x)
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
            if(jab_decode_order[enc->symbol_positions[i]].y == y)
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
*/
void createBitmap(jab_encode* enc, jab_code* cp)
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
        return;
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
        jab_int32 col = jab_decode_order[enc->symbol_positions[k]].x - cp->min_x;
        jab_int32 row = jab_decode_order[enc->symbol_positions[k]].y - cp->min_y;
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
			jab_int32 slave_index = enc->docked_symbol[i*4 + j];
			if(slave_index > 0)
			{
					jab_int32 hpos = enc->symbol_positions[i];
					jab_int32 spos = enc->symbol_positions[slave_index];
					jab_int32 x_diff = jab_decode_order[hpos].x - jab_decode_order[spos].x;
					jab_int32 y_diff = jab_decode_order[hpos].y - jab_decode_order[spos].y;

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
 * @brief Generate JABCode
 * @param enc the encode parameters
 * @param data the input data
 * @return JAB_SUCCESS | JAB_FAILURE
*/
jab_boolean generateJABCode(jab_encode* enc, jab_data* data)
{
    //Check data
    if(data == NULL)
    {
        reportError("No input data specified!");
        return JAB_FAILURE;
    }
    if(data->length == 0)
    {
        reportError("No input data specified!");
        return JAB_FAILURE;
    }
    //sort symbol versions and ecc levels depending on symbol positions in ascending order
    for(jab_int32 i=0; i<enc->symbol_number-1; i++)
    {
        for(jab_int32 j=i; j>=0; j--)
        {
            if(enc->symbol_positions[j] > enc->symbol_positions[j+1])
            {
                swap_int(&enc->symbol_positions[j], &enc->symbol_positions[j+1]);
                swap_int(&enc->symbol_versions[j].x, &enc->symbol_versions[j+1].x);
                swap_int(&enc->symbol_versions[j].y, &enc->symbol_versions[j+1].y);
                swap_byte(&enc->ecc_levels[j], &enc->ecc_levels[j+1]);
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
    if(enc->symbol_number > 1)
    {
        for(jab_int32 i=0; i<enc->symbol_number-1; i++)
        {
            if(enc->symbol_positions[i] == enc->symbol_positions[i+1])
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

    //get the optimal encoded length and encoding seqence
    jab_int32 encoded_length;
    jab_int32* encode_seq = analyzeInputData(data, &encoded_length);
    //used for ecc parameters
    jab_int32* coderate_params=(jab_int32 *)malloc(sizeof(jab_int32)*(enc->symbol_number * 2));
    if(coderate_params == NULL){
        reportError("Memory allocation for coderate parameters failed");
        free(encode_seq);
        return JAB_FAILURE;
    }
    jab_int32* from_to=(jab_int32 *)malloc(sizeof(jab_int32)*(enc->symbol_number * 2));
    if(from_to == NULL){
        reportError("Memory allocation for from_to array failed");
        free(coderate_params);
        free(encode_seq);
        return JAB_FAILURE;
    }
    //if symbol_versions and ecc_levels not specified for master symbol
    if(enc->symbol_number==1)
    {
        if (determineMissingParams(enc,&encoded_length,coderate_params)==1)
        {
            reportError("Calcualtion of missing parameters failed");
            free(coderate_params);
            free(encode_seq);
            free(from_to);
            return JAB_FAILURE;
        }
        from_to[0]=0;
        from_to[1]=encoded_length;
    }
    else//check if message fits into specified code, if yes -> devide message and determine all params for ecc
    {
        //if ecc_level not specified for each symbol or all equal, spread data and determine best wc and wr to get most stable data
        if(divideMessage(enc,&encoded_length,coderate_params,from_to)==1)
        {
            reportError("JabCode parameter failure, please use --help");
            free(coderate_params);
            free(encode_seq);
            free(from_to);
            return JAB_FAILURE;
        }
    }
    //encode data using optimal encoding modes
    jab_data* encoded_data = encodeData(data,encoded_length,encode_seq);
    if(encoded_data == NULL)
    {
        free(coderate_params);
        free(encode_seq);
        free(from_to);
        return JAB_FAILURE;
    }

    //determine side size for each symbol
    for(jab_int32 i=0; i<enc->symbol_number; i++)
    {
        //set symbol index
        enc->symbols[i].index = i;
        //set symbol side size
        enc->symbols[i].side_size.x = VERSION2SIZE(enc->symbol_versions[i].x);
        enc->symbols[i].side_size.y = VERSION2SIZE(enc->symbol_versions[i].y);
    }

	//Interleave color palette in case of more than 8 colors
	jab_int32 palette_index_size = enc->color_number > 64 ? 64 : enc->color_number;
	jab_byte palette_index[palette_index_size];
	for(jab_int32 i=0; i<palette_index_size; i++)
		palette_index[i] = i;
	if(enc->color_number > 8)
	{
		interleaveColorPalette(palette_index, palette_index_size, enc->color_number);
	}

    //encode each symbol in turn
    for(jab_int32 i=0; i<enc->symbol_number; i++)
    {
        //error correction for data
        jab_data* ecc_encoded_data = encodeLDPC(encoded_data,coderate_params,from_to,i);
#if TEST_MODE
		FILE* fp = fopen("enc_bit_data.bin", "wb");
		fwrite(ecc_encoded_data->data, ecc_encoded_data->length, 1, fp);
		fclose(fp);
#endif // TEST_MODE

        if(ecc_encoded_data == NULL)
        {
            JAB_REPORT_ERROR(("LDPC encoding for the data in symbol %d failed", i))
            free(coderate_params);
            free(encode_seq);
            free(from_to);
            free(encoded_data);
            return JAB_FAILURE;
        }

        //interleave
        //jab_vector2d side_version={enc->symbol_versions[i].x, enc->symbol_versions[i].y};
        interleaveData(ecc_encoded_data);
        //encode Metadata
        if(!encodeMetadata(enc, i, coderate_params, 1))
        {
			JAB_REPORT_ERROR(("Encoding metadata for symbol %d failed", i))
			free(coderate_params);
            free(encode_seq);
            free(from_to);
            free(encoded_data);
			return JAB_FAILURE;
        }
        //create Matrix
        createMatrix(enc, i, ecc_encoded_data, palette_index);
        free(ecc_encoded_data);
    }

    //mask all symbols in the code
    jab_code* cp = getCodePara(enc);
    if(!cp)
    {
		free(coderate_params);
		free(encode_seq);
		free(from_to);
		free(encoded_data);
		return JAB_FAILURE;
    }
    jab_int32 mask_reference = maskCode(enc, cp, palette_index);
    if(mask_reference == NUMBER_OF_MASK_PATTERNS)
    {
        if(cp->row_height) free(cp->row_height);
		if(cp->col_width)  free(cp->col_width);
		free(cp);
		free(coderate_params);
		free(encode_seq);
		free(from_to);
		free(encoded_data);
        return JAB_FAILURE;
	}
    //encode metadata for master symbol with mask
    if(!encodeMetadata(enc, 0, coderate_params, mask_reference))
    {
        if(cp->row_height) free(cp->row_height);
		if(cp->col_width)  free(cp->col_width);
		free(cp);
		free(coderate_params);
		free(encode_seq);
		free(from_to);
		free(encoded_data);
		return JAB_FAILURE;
    }
    //place metadata modules in master symbol
    placeMetadata(enc, palette_index);
    //create the code bitmap
    createBitmap(enc, cp);

    //clean memory
    if(cp->row_height) free(cp->row_height);
    if(cp->col_width)  free(cp->col_width);
    free(cp);
    free(coderate_params);
    free(encode_seq);
    free(from_to);
    free(encoded_data);
    return JAB_SUCCESS;
}

/**
 * @brief Report error message
 * @param message the error message
*/
void reportError(jab_char* message)
{
    printf("JABCode Error: %s\n", message);
}
