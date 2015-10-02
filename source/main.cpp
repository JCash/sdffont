/*
 * A simple test program to display the output of the voronoi generator
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define SDF_IMPLEMENTATION
#include "sdf.h"


static void Usage()
{
	printf("Usage: sdffont [options]\n");
	printf("\t-o <outputpath> Determines output format from the suffix\n");
	printf("\t-s <font size>\n");
	printf("\t-w <image width>\n");
	printf("\t-h <image height>\n");
}

uint8_t* ReadFont(const char* path)
{
	struct stat st;
	if( stat(path, &st) == -1 )
		return 0;

	uint8_t* buffer = new uint8_t[st.st_size];

	FILE* f = fopen(path, "rb");
	if( 1 != fread(buffer, st.st_size, 1, f) )
	{
		delete[] buffer;
		return 0;
	}

	fclose(f);
	return buffer;
}

static void DrawBox(int x0, int y0, int x1, int y1, uint8_t* image, int width)
{
	for( int x = x0; x <= x1; ++x )
	{
		image[y0 * width + x] = 255;
		image[y1 * width + x] = 255;
	}

	for( int y = y0; y <= y1; ++y )
	{
		image[y * width + x0] = 255;
		image[y * width + x1] = 255;
	}
}

STBTT_DEF int stbtt_GetGlyphNumPairKernings(const stbtt_fontinfo *info)
{
   stbtt_uint8 *data = info->data + info->kern;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
      return 0;

   return ttUSHORT(data+10);
}

STBTT_DEF int stbtt_GetGlyphPairKerning(const stbtt_fontinfo *info, int index, int* glyph1, int* glyph2, int* pairkerning)
{
   stbtt_uint8 *data = info->data + info->kern;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
      return 0;

   *pairkerning = ttSHORT(data+22+(index*6));
   int combination = ttULONG(data+18+(index*6)); // note: unaligned read
   *glyph1 = combination >> 16;
   *glyph2 = combination & 0xFFFF;
   return 1;
}

int main(int argc, const char** argv)
{
	int fontsize = 32;
	int width = 1024;
	int height = 1024;
	int radius = 6;
	const char* inputfile = 0;
	const char* outputfile = "output.png";

	for( int i = 1; i < argc; ++i )
	{
		if(strcmp(argv[i], "-i") == 0)
		{
			if( i+1 < argc )
				inputfile = argv[i+1];
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "-o") == 0)
		{
			if( i+1 < argc )
				outputfile = argv[i+1];
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "-w") == 0)
		{
			if( i+1 < argc )
				width = (int)atol(argv[i+1]);
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "-h") == 0)
		{
			if( i+1 < argc )
				height = (int)atol(argv[i+1]);
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "-s") == 0)
		{
			if( i+1 < argc )
				fontsize = (int)atol(argv[i+1]);
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "-r") == 0)
		{
			if( i+1 < argc )
				radius = (int)atol(argv[i+1]);
			else
			{
				Usage();
				return 1;
			}
		}
	}
	if( !inputfile )
	{
		fprintf(stderr, "You have to specify an input file!\n");
		Usage();
		return 1;
	}

	uint8_t* fontfile = ReadFont(inputfile);
	if( !fontfile )
	{
		fprintf(stderr, "Failed to read %s\n", inputfile);
		return 1;
	}


	printf("Image Width/Height is %d, %d, target format is %s\n", width, height, ".png");
	printf("Font is %s, chosen height is %d\n", inputfile, fontsize);
	printf("Sdf radius is %d\n", radius);


	size_t imagesize = (size_t)(width*height);
	unsigned char* image = (unsigned char*)malloc(imagesize);
	memset(image, 0, imagesize);

	unsigned char* imagedist = (unsigned char*)malloc(imagesize);
	memset(imagedist, 0, imagesize);


	{
		stbtt_packedchar pdata[256*2];

		stbtt_pack_context pc;
		stbtt_pack_range pr[2];

		pr[0].chardata_for_range = pdata;
		pr[0].first_unicode_codepoint_in_range = 32;
		pr[0].num_chars = 95;
		pr[0].array_of_unicode_codepoints = 0;
		pr[0].font_size = (float)fontsize;
		pr[1].chardata_for_range = pdata+256;
		pr[1].first_unicode_codepoint_in_range = 0xa0;
		pr[1].num_chars = 0x100 - 0xa0;
		pr[1].array_of_unicode_codepoints = 0;
		pr[1].font_size = (float)fontsize;

		stbtt_PackBegin(&pc, image, width, height, 0, radius+1, NULL);
		stbtt_PackSetOversampling(&pc, 2, 2);
		stbtt_PackFontRanges(&pc, fontfile, 0, pr, 2);
		stbtt_PackEnd(&pc);

		sdfBuildDistanceField(imagedist, width, (float)radius, image, width, height, width);

		int glyph_to_codepoint[65536];
		memset(glyph_to_codepoint, 0, sizeof(glyph_to_codepoint));

		stbtt_fontinfo font;
		stbtt_InitFont(&font, fontfile, stbtt_GetFontOffsetForIndex(fontfile,0));

		for( int r = 0; r < 2; ++r )
		{
			for( int i = 0; i < pr[r].num_chars; ++i )
			{
				stbtt_packedchar& c = pr[r].chardata_for_range[i];
				DrawBox(c.x0, c.y0, c.x1, c.y1, imagedist, width);

				int codepoint = pr[r].first_unicode_codepoint_in_range + i;
				int glyph = stbtt_FindGlyphIndex(&font, codepoint);
				glyph_to_codepoint[glyph] = codepoint;
			}
		}

		//float scale = stbtt_ScaleForPixelHeight(&font, fontsize);

		int numpairkernings = stbtt_GetGlyphNumPairKernings(&font);
		for( int i = 0; i < numpairkernings; ++i )
		{
			int glyph1, glyph2, kerning;
			stbtt_GetGlyphPairKerning(&font, i, &glyph1, &glyph2, &kerning);

			//printf("k: %c, %c -> %f\n", glyph_to_codepoint[glyph1], glyph_to_codepoint[glyph2], scale*kerning);
		}
	}

	char path[512];
	sprintf(path, "%s", outputfile);
	stbi_write_png(path, width, height, 1, imagedist, 0);
	printf("wrote %s\n", path);

	free(image);

	return 0;
}
