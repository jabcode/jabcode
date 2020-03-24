/**
 * libjabcode - JABCode Encoding/Decoding Library
 *
 * Copyright 2016 by Fraunhofer SIT. All rights reserved.
 * See LICENSE file for full terms of use and distribution.
 *
 * Contact: Huajian Liu <liu@sit.fraunhofer.de>
 *			Waldemar Berchtold <waldemar.berchtold@sit.fraunhofer.de>
 *
 * @file detector.h
 * @brief Detector header
 */

#ifndef JABCODE_DETECTOR_H
#define JABCODE_DETECTOR_H

#define TEST_MODE			0
#if TEST_MODE
jab_bitmap* test_mode_bitmap;
jab_int32	test_mode_color;
#endif

#define MAX_MODULES 		145	//the number of modules in side-version 32
#define MAX_SYMBOL_ROWS		3
#define MAX_SYMBOL_COLUMNS	3
#define MAX_FINDER_PATTERNS 500
#define PI 					3.14159265
#define CROSS_AREA_WIDTH	14	//the width of the area across the host and slave symbols

#define DIST(x1, y1, x2, y2) (jab_float)(sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2)))

/**
 * @brief Detection modes
*/
typedef enum
{
	QUICK_DETECT = 0,
	NORMAL_DETECT,
	INTENSIVE_DETECT
}jab_detect_mode;

/**
 * @brief Finder pattern, alignment pattern
*/
typedef struct {
	jab_int32		type;
	jab_float		module_size;
	jab_point		center;			//coordinates of the center
	jab_int32		found_count;
	jab_int32 		direction;
}jab_finder_pattern, jab_alignment_pattern;

/**
 * @brief Perspective transform
*/
typedef struct {
	jab_float a11;
	jab_float a12;
	jab_float a13;
	jab_float a21;
	jab_float a22;
	jab_float a23;
	jab_float a31;
	jab_float a32;
	jab_float a33;
}jab_perspective_transform;

extern void getAveVar(jab_byte* rgb, jab_double* ave, jab_double* var);
extern void getMinMax(jab_byte* rgb, jab_byte* min, jab_byte* mid, jab_byte* max, jab_int32* index_min, jab_int32* index_mid, jab_int32* index_max);
extern void balanceRGB(jab_bitmap* bitmap);
extern jab_boolean binarizerRGB(jab_bitmap* bitmap, jab_bitmap* rgb[3], jab_float* blk_ths);
extern jab_bitmap* binarizer(jab_bitmap* bitmap, jab_int32 channel);
extern jab_bitmap* binarizerHist(jab_bitmap* bitmap, jab_int32 channel);
extern jab_bitmap* binarizerHard(jab_bitmap* bitmap, jab_int32 channel, jab_int32 threshold);
extern jab_perspective_transform* getPerspectiveTransform(jab_point p0, jab_point p1,
														  jab_point p2, jab_point p3,
														  jab_vector2d side_size);
extern jab_perspective_transform* perspectiveTransform( jab_float x0, jab_float y0,
														jab_float x1, jab_float y1,
														jab_float x2, jab_float y2,
														jab_float x3, jab_float y3,
														jab_float x0p, jab_float y0p,
														jab_float x1p, jab_float y1p,
														jab_float x2p, jab_float y2p,
														jab_float x3p, jab_float y3p);
extern void warpPoints(jab_perspective_transform* pt, jab_point* points, jab_int32 length);
extern jab_bitmap* sampleSymbol(jab_bitmap* bitmap, jab_perspective_transform* pt, jab_vector2d side_size);
extern jab_bitmap* sampleCrossArea(jab_bitmap* bitmap, jab_perspective_transform* pt);

#endif
