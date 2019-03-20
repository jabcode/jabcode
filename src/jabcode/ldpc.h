/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file ldpc.h
 * @brief LDPC encoder/decoder header
 */

#ifndef JABCODE_LDPC_H
#define JABCODE_LDPC_H

#define LPDC_METADATA_SEED 	38545
#define LPDC_MESSAGE_SEED 	785465

//#define LDPC_DEFAULT_WC		4	//default error correction level 3
//#define LDPC_DEFAULT_WR		9	//default error correction level 3

//static const jab_vector2d default_ecl = {4, 7};	//default (wc, wr) for LDPC, corresponding to ecc level 5.
//static const jab_vector2d default_ecl = {5, 6};	//This (wc, wr) could be used, if higher robustness is preferred to capacity.

extern jab_data *encodeLDPC(jab_data* data, jab_int32* coderate_params);
extern jab_int32 decodeLDPChd(jab_byte* data, jab_int32 length, jab_int32 wc, jab_int32 wr);
extern jab_int32 decodeLDPC(jab_float* enc, jab_int32 length, jab_int32 wc, jab_int32 wr, jab_byte* dec);


#endif
