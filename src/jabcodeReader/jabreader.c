#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jabcode.h"

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

int main(int argc, char *argv[])
{
	if(argc < 2 || (0 == strcmp(argv[1],"--help")))
	{
		printUsage();
		return 1;
	}

	jab_boolean output_as_file = 0;
	if(argc > 2)
	{
		if(0 == strcmp(argv[2], "--output"))
			output_as_file = 1;
		else
		{
			printf("Unknown parameter: %s\n", argv[2]);
			return 1;
		}
	}

	//load image
	jab_bitmap* bitmap;
	bitmap = readImage(argv[1]);
	if(bitmap == NULL)
		return 1;

	//find and decode JABCode in the image
	jab_data* decoded_data = decodeJABCode(bitmap, NORMAL_DECODE);
	if(decoded_data == NULL)
	{
		if(bitmap) free(bitmap);
		reportError("Decoding jabcode failed");
		return 1;
	}

	//output result
	if(output_as_file)
	{
		FILE* output_file = fopen(argv[3], "wb");
		if(output_file == NULL)
		{
			reportError("Can not open output file");
			return 1;
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

	if(bitmap) free(bitmap);
	if(decoded_data) free(decoded_data);
    return 0;
}
