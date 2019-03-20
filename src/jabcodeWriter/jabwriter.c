#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jabcode.h"
#include "jabwriter.h"

jab_data* 		data = 0;
jab_char* 		filename = 0;
jab_int32 		color_number = 0;
jab_int32 		symbol_number = 0;
jab_int32 		module_size = 0;
jab_int32		master_symbol_width = 0;
jab_int32 		master_symbol_height= 0;
jab_int32* 		symbol_positions = 0;
jab_int32 		symbol_positions_number = 0;
jab_vector2d* 	symbol_versions = 0;
jab_int32 		symbol_versions_number = 0;
jab_int32* 		symbol_ecc_levels = 0;
jab_int32 		symbol_ecc_levels_number = 0;

/**
 * @brief Print usage of JABCode writer
*/
void printUsage()
{
	printf("\n");
	printf("jabcodeWriter (Version %s Build date: %s) - Fraunhofer SIT\n\n", VERSION, BUILD_DATE);
	printf("Usage:\n\n");
	printf("jabcodeWriter --input message-to-encode --output output-image(png) [options]\n");
	printf("\n");
	printf("--input\t\t\tInput data (message to be encoded).\n");
	printf("--input-file\t\tInput data file.\n");
    printf("--output\t\tOutput png file.\n");
    printf("--color-number\t\tNumber of colors (4, 8, 16, 32, 64, 128, 256,\n\t\t\t"
							 "default: 8).\n");
	printf("--module-size\t\tModule size in pixel (default: 12 pixels).\n");
    printf("--symbol-width\t\tMaster symbol width in pixel.\n");
    printf("--symbol-height\t\tMaster symbol height in pixel.\n");
	printf("--symbol-number\t\tNumber of symbols (1 - 61, default: 1).\n");
    printf("--ecc-level\t\tError correction levels (1 - 10, default: 3(6%%)). If\n\t\t\t"
						  "different for each symbol, starting from master and\n\t\t\t"
						  "then slave symbols (ecc0 ecc1 ecc2 ...). For master\n\t\t\t"
						  "symbol, level 0 means using the default level, for\n\t\t\t"
						  "slaves, it means using the same level as its host.\n");
    printf("--symbol-version\tSide-Version of each symbol, starting from master and\n\t\t\t"
							 "then slave symbols (x0 y0 x1 y1 x2 y2 ...).\n");
    printf("--symbol-position\tSymbol positions (0 - 60), starting from master and\n\t\t\t"
							  "then slave symbols (p0 p1 p2 ...). Only required for\n\t\t\t"
							  "multi-symbol code.\n");
    printf("--help\t\t\tPrint this help.\n");
    printf("\n");
    printf("Example for 1-symbol-code: \n");
    printf("jabcodeWriter --input 'Hello world' --output test.png\n");
    printf("\n");
    printf("Example for 3-symbol-code: \n" );
    printf("jabcodeWriter --input 'Hello world' --output test.png --symbol-number 3 --symbol-position 0 3 2 --symbol-version 3 2 4 2 3 2\n");
    printf("\n");
}

/**
 * @brief Parse command line parameters
 * @return 1: success | 0: failure
*/
jab_boolean parseCommandLineParameters(jab_int32 para_number, jab_char* para[])
{
	//first scan
	for (jab_int32 loop=1; loop<para_number; loop++)
	{
		if (0 == strcmp(para[loop],"--input"))
        {
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", para[loop]);
				return 0;
			}
            jab_char* data_string = para[++loop];
            data = (jab_data *)malloc(sizeof(jab_data) + strlen(data_string) * sizeof(jab_char));
            if(!data)
            {
                reportError("Memory allocation for input data failed");
                return 0;
            }
			data->length = strlen(data_string);
			memcpy(data->data, data_string, strlen(data_string));
        }
        else if (0 == strcmp(para[loop],"--input-file"))
        {
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", para[loop]);
				return 0;
			}
            FILE* fp = fopen(para[++loop], "rb");
            if(!fp)
            {
				reportError("Opening input data file failed");
                return 0;
            }
			jab_int32 file_size;
			fseek(fp, 0, SEEK_END);
			file_size = ftell(fp);
            data = (jab_data *)malloc(sizeof(jab_data) + file_size * sizeof(jab_char));
            if(!data)
            {
                reportError("Memory allocation for input data failed");
                return 0;
            }
            fseek(fp, 0, SEEK_SET);
            if(fread(data->data, 1, file_size, fp) != file_size)
            {
				reportError("Reading input data file failed");
				free(data);
				fclose(fp);
				return 0;
			}
			fclose(fp);
			data->length = file_size;
        }
		else if (0 == strcmp(para[loop],"--output"))
        {
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", para[loop]);
				return 0;
			}
            filename = para[++loop];
        }
        else if (0 == strcmp(para[loop],"--color-number"))
        {
			char* option = para[loop];
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
				return 0;
			}
            char* endptr;
			color_number = strtol(para[++loop], &endptr, 10);
			if(*endptr)
			{
				printf("Invalid or missing values for option '%s'.\n", option);
				return 0;
			}
            if(color_number != 2  && color_number != 4  && color_number != 8   && color_number != 16 &&
			   color_number != 32 && color_number != 64 && color_number != 128 && color_number != 256)
            {
				reportError("Invalid color number. Valid color number includes 2, 4, 8, 16, 32, 64, 128 and 256.");
				return 0;
            }
        }
        else if (0 == strcmp(para[loop],"--module-size"))
        {
			char* option = para[loop];
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
				return 0;
			}
            char* endptr;
			module_size = strtol(para[++loop], &endptr, 10);
			if(*endptr || module_size < 0)
			{
				printf("Invalid or missing values for option '%s'.\n", option);
				return 0;
			}
        }
        else if (0 == strcmp(para[loop],"--symbol-width"))
        {
			char* option = para[loop];
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
				return 0;
			}
            char* endptr;
			master_symbol_width = strtol(para[++loop], &endptr, 10);
			if(*endptr || master_symbol_width < 0)
			{
				printf("Invalid or missing values for option '%s'.\n", option);
				return 0;
			}
        }
        else if (0 == strcmp(para[loop],"--symbol-height"))
        {
			char* option = para[loop];
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
				return 0;
			}
            char* endptr;
			master_symbol_height = strtol(para[++loop], &endptr, 10);
			if(*endptr || master_symbol_height < 0)
			{
				printf("Invalid or missing values for option '%s'.\n", option);
				return 0;
			}
        }
        else if (0 == strcmp(para[loop],"--symbol-number"))
        {
			char* option = para[loop];
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
                return 0;
			}
			char* endptr;
			symbol_number = strtol(para[++loop], &endptr, 10);
            if(*endptr)
			{
				printf("Invalid or missing values for option '%s'.\n", option);
				return 0;
			}
            if(symbol_number < 1 || symbol_number > MAX_SYMBOL_NUMBER)
            {
				reportError("Invalid symbol number (must be 1 - 61).");
				return 0;
            }
        }
	}

	//check input
    if(!data)
    {
		reportError("Input data missing");
		return 0;
    }
    else if(data->length == 0)
    {
		reportError("Input data is empty");
		return 0;
    }
    if(!filename)
    {
		reportError("Output file missing");
		return 0;
    }
    if(symbol_number == 0)
    {
		symbol_number = 1;
    }

	//second scan
    for (jab_int32 loop=1; loop<para_number; loop++)
    {
        if (0 == strcmp(para[loop],"--ecc-level"))
        {
			char* option = para[loop];
            if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
				return 0;
			}
            symbol_ecc_levels = (jab_int32 *)calloc(symbol_number, sizeof(jab_int32));
            if(!symbol_ecc_levels)
            {
                reportError("Memory allocation for symbol ecc levels failed");
                return 0;
            }
            for (jab_int32 j=0; j<symbol_number; j++)
            {
				if(loop + 1 > para_number - 1)
					break;
				char* endptr;
				symbol_ecc_levels[j] = strtol(para[++loop], &endptr, 10);
				if(*endptr)
				{
					if(symbol_ecc_levels_number == 0)
					{
						printf("Value for option '%s' missing or invalid.\n", option);
						return 0;
					}
					loop--;
					break;
				}
                if(symbol_ecc_levels[j] < 0 || symbol_ecc_levels[j] > 10)
				{
                    reportError("Invalid error correction level (must be 1 - 10).");
					return 0;
				}
				symbol_ecc_levels_number++;
			}
        }
        else if (0 == strcmp(para[loop],"--symbol-version"))
        {
			char* option = para[loop];
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
				return 0;
			}
            symbol_versions = (jab_vector2d *)calloc(symbol_number, sizeof(jab_vector2d));
            if(!symbol_versions)
            {
                reportError("Memory allocation for symbol versions failed");
                return 0;
            }
			for(jab_int32 j=0; j<symbol_number; j++)
			{
				if(loop + 1 > para_number - 1)
				{
					printf("Too few values for option '%s'.\n", option);
					return 0;
				}
				char* endptr;
				symbol_versions[j].x = strtol(para[++loop], &endptr, 10);
				if(*endptr)
				{
					printf("Invalid or missing values for option '%s'.\n", option);
					return 0;
				}
				if(loop + 1 > para_number - 1)
				{
					printf("Too few values for option '%s'.\n", option);
					return 0;
				}
				symbol_versions[j].y = strtol(para[++loop], &endptr, 10);
				if(*endptr)
				{
					printf("Invalid or missing values for option '%s'.\n", option);
					return 0;
				}
				if(symbol_versions[j].x < 1 || symbol_versions[j].x > 32 || symbol_versions[j].y < 1 || symbol_versions[j].y > 32)
				{
					reportError("Invalid symbol side version (must be 1 - 32).");
					return 0;
				}
				symbol_versions_number++;
			}
        }
        else if (0 == strcmp(para[loop],"--symbol-position"))
        {
			char* option = para[loop];
			if(loop + 1 > para_number - 1)
			{
				printf("Value for option '%s' missing.\n", option);
				return 0;
			}
            symbol_positions = (jab_int32 *)calloc(symbol_number, sizeof(jab_int32));
            if(!symbol_positions)
            {
                reportError("Memory allocation for symbol positions failed");
                return 0;
            }
            for(jab_int32 j=0; j<symbol_number; j++)
            {
				if(loop + 1 > para_number - 1)
				{
					printf("Too few values for option '%s'.\n", option);
					return 0;
				}
				char* endptr;
				symbol_positions[j] = strtol(para[++loop], &endptr, 10);
				if(*endptr)
				{
					printf("Invalid or missing values for option '%s'.\n", option);
					return 0;
				}
				if(symbol_positions[j] < 0 || symbol_positions[j] > 60)
				{
					reportError("Invalid symbol position value (must be 0 - 60).");
					return 0;
				}
				symbol_positions_number++;
            }
        }
    }

    //check input
    if(symbol_number == 1 && symbol_positions)
    {
		if(symbol_positions[0] != 0)
		{
			reportError("Incorrect symbol position value for master symbol.");
			return 0;
		}
    }
	if(symbol_number > 1 && symbol_positions_number != symbol_number)
    {
        reportError("Symbol position information is incomplete for multi-symbol code");
        return 0;
    }
    if (symbol_number > 1 && symbol_versions_number != symbol_number)
    {
		reportError("Symbol version information is incomplete for multi-symbol code");
        return 0;
    }
    return 1;
}

/**
 * @brief Free allocated buffers
*/
void cleanMemory()
{
	free(data);
	free(symbol_positions);
	free(symbol_versions);
	free(symbol_ecc_levels);
}

/**
 * @brief JABCode writer main function
 * @return 0: success | 1: failure
*/
int main(int argc, char *argv[])
{
    if(argc < 2 || (0 == strcmp(argv[1],"--help")))
	{
		printUsage();
		return 1;
	}
	if(!parseCommandLineParameters(argc, argv))
	{
		return 1;
	}

    //create encode parameter object
    jab_encode* enc = createEncode(color_number, symbol_number);
    if(enc == NULL)
    {
		cleanMemory();
		reportError("Creating encode parameter failed");
        return 1;
    }
    if(module_size > 0)
    {
		enc->module_size = module_size;
    }
    if(master_symbol_width > 0)
    {
		enc->master_symbol_width = master_symbol_width;
    }
    if(master_symbol_height > 0)
    {
		enc->master_symbol_height = master_symbol_height;
    }
	for(jab_int32 loop=0; loop<symbol_number; loop++)
	{
		if(symbol_ecc_levels)
			enc->symbol_ecc_levels[loop] = symbol_ecc_levels[loop];
		if(symbol_versions)
			enc->symbol_versions[loop] = symbol_versions[loop];
		if(symbol_positions)
			enc->symbol_positions[loop] = symbol_positions[loop];
	}

	//generate JABCode
	if(generateJABCode(enc, data) != 0)
	{
		reportError("Creating jab code failed");
		destroyEncode(enc);
		cleanMemory();
		return 1;
	}

	//save bitmap in image file
	if(!saveImage(enc->bitmap, filename))
	{
		reportError("Saving png image failed");
		destroyEncode(enc);
		cleanMemory();
		return 1;
	}
	destroyEncode(enc);
	cleanMemory();
	return 0;
}

