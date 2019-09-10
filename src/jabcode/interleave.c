/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file interleave.c
 * @brief Data interleaving
 */

#include <stdlib.h>
#include <string.h>
#include "jabcode.h"
#include "encoder.h"
#include "pseudo_random.h"

#define INTERLEAVE_SEED 226759

/**
 * @brief In-place interleaving
 * @param data the input data to be interleaved
*/
void interleaveData(jab_data* data)
{
    setSeed(INTERLEAVE_SEED);
    for (jab_int32 i=0; i<data->length; i++)
    {
        jab_int32 pos = (jab_int32)( (jab_float)lcg64_temper() / (jab_float)UINT32_MAX * (data->length - i) );
        jab_char  tmp = data->data[data->length - 1 -i];
        data->data[data->length - 1 - i] = data->data[pos];
        data->data[pos] = tmp;
    }
}

/**
 * @brief In-place deinterleaving
 * @param data the input data to be deinterleaved
*/
void deinterleaveData(jab_data* data)
{
    jab_int32 * index = (jab_int32 *)malloc(data->length * sizeof(jab_int32));
    if(index == NULL)
    {
        reportError("Memory allocation for index buffer in deinterleaver failed");
        return;
    }
    for(jab_int32 i=0; i<data->length; i++)
    {
		index[i] = i;
    }
    //interleave index
    setSeed(INTERLEAVE_SEED);
    for(jab_int32 i=0; i<data->length; i++)
    {
		jab_int32 pos = (jab_int32)( (jab_float)lcg64_temper() / (jab_float)UINT32_MAX * (data->length - i) );
		jab_int32 tmp = index[data->length - 1 - i];
		index[data->length - 1 -i] = index[pos];
		index[pos] = tmp;
    }
    //deinterleave data
    jab_char* tmp_data = (jab_char *)malloc(data->length * sizeof(jab_char));
    if(tmp_data == NULL)
    {
        reportError("Memory allocation for temporary buffer in deinterleaver failed");
        return;
    }
	memcpy(tmp_data, data->data, data->length*sizeof(jab_char));
	for(jab_int32 i=0; i<data->length; i++)
    {
        data->data[index[i]] = tmp_data[i];
    }
    free(tmp_data);
    free(index);
}
