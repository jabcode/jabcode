#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jabcode.h"

/**
 * @brief Print usage of JABCode reader
*/
void printUsage()
{
	printf("\n");
	printf("jabcodeReader (Version %s Build date: %s) - Fraunhofer SIT\n\n", VERSION, BUILD_DATE);
	printf("Usage:\n\n");
	printf("jabcodeReader input-image(png) [--output output-file]\n");
	printf("\n");
	printf("--output\tOutput file for decoded data.\n");
	printf("--help\t\tPrint this help.\n");
	printf("\n");
}

/**
 * @brief JABCode reader main function
 * @return 0: success | 255: not detectable | other non-zero: decoding failed
*/
int main(int argc, char *argv[])
{
	if(argc < 2 || (0 == strcmp(argv[1],"--help")))
	{
		printUsage();
		return 255;
	}

	jab_boolean output_as_file = 0;
	if(argc > 2)
	{
		if(0 == strcmp(argv[2], "--output"))
			output_as_file = 1;
		else
		{
			printf("Unknown parameter: %s\n", argv[2]);
			return 255;
		}
	}

	//load image
	jab_bitmap* bitmap;
	bitmap = readImage(argv[1]);
	if(bitmap == NULL)
		return 255;

	//find and decode JABCode in the image
	jab_int32 decode_status;
	jab_decoded_symbol symbols[MAX_SYMBOL_NUMBER];
	jab_data* decoded_data = decodeJABCodeEx(bitmap, NORMAL_DECODE, &decode_status, symbols, MAX_SYMBOL_NUMBER);
	if(decoded_data == NULL)
	{
		free(bitmap);
		reportError("Decoding JABCode failed");
		if(decode_status > 0)
			return (jab_int32)(symbols[0].module_size + 0.5f);
		else
			return 255;
	}

	//output warning if the code is only partly decoded with COMPATIBLE_DECODE mode
	if(decode_status == 2)
	{
		JAB_REPORT_INFO(("The code is only partly decoded. Some slave symbols have not been decoded and are ignored."))
	}

	//output result
	if(output_as_file)
	{
		FILE* output_file = fopen(argv[3], "wb");
		if(output_file == NULL)
		{
			reportError("Can not open the output file");
			return 255;
		}
		fwrite(decoded_data->data, decoded_data->length, 1, output_file);
		fclose(output_file);
	}
	else
	{
		for(jab_int32 i=0; i<decoded_data->length; i++)
			printf("%c", decoded_data->data[i]);
		printf("\n");
	}

	free(bitmap);
	free(decoded_data);
    return 0;
}
