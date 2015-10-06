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

#include "font.h"

#include <algorithm>
#include <vector>
#include <map>


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

static void DrawBox(int x0, int y0, int x1, int y1, uint8_t* image, int width, int height)
{
	for( int x = x0; x <= x1; ++x )
	{
		if( x < 0 || x >= width )
			continue;
		image[y0 * width + x] = 255;
		image[y1 * width + x] = 255;
	}

	for( int y = y0; y <= y1; ++y )
	{
		if( y < 0 || y >= height )
			continue;
		image[y * width + x0] = 255;
		image[y * width + x1] = 255;
	}
}



uint64_t GetFileOffset(FILE* f)
{
	return (uint64_t)ftell(f);
}

static uint64_t Align8(uint64_t pos)
{
	uint64_t remainder = pos & (8 - 1);
	if( remainder )
		return pos + 8 - remainder;
	return pos;
}

static void AlignFile8(FILE* file)
{
	uint64_t pos = GetFileOffset(file);
	uint64_t nextpos = Align8(pos);
	uint8_t c = 0;
	for( uint64_t i = pos; i < nextpos; ++i )
		fwrite(&c, 1, 1, file);
}

static int WriteFontInfo(const stbtt_fontinfo* info, const char* path, int width, int height, int fontsize,
						std::vector<SFontGlyph>& glyphs, std::vector<SFontPairKerning>& pairkernings)
{
	std::sort(glyphs.begin(), glyphs.end());

	SFontHeader header = {};
	header.magic[0] 			= 'F';
	header.magic[1] 			= 'O';
	header.magic[2] 			= 'N';
	header.magic[3] 			= 'T';
	header.texturesize_width	= width;
	header.texturesize_height	= height;
	header.fontsize				= fontsize;

	float fontscale = stbtt_ScaleForPixelHeight(info, fontsize);

	int lineascent, linedescend, linegap;
	stbtt_GetFontVMetrics(info, &lineascent, &linedescend, &linegap);

	header.line_ascend			= lineascent * fontscale;
	header.line_descend			= linedescend * fontscale;
	header.line_gap				= linegap * fontscale;
	header.num_glyphs			= (uint16_t)glyphs.size();
	header.num_pairkernings		= (uint16_t)pairkernings.size();

	// Offsets into the file where to find data (0 based, i.e from beginning of file)
	uint64_t offset = sizeof(SFontHeader);
	offset = Align8(offset);
	assert( (offset & 7) == 0 );

	header.codepoints			= offset;
	offset += header.num_glyphs * sizeof(uint32_t);
	offset = Align8(offset);
	assert( (offset & 7) == 0 );
	header.pairkeys				= offset;

	offset += header.num_pairkernings * sizeof(uint64_t);
	offset = Align8(offset);
	assert( (offset & 7) == 0 );

	header.pairvalues			= offset;
	offset += header.num_pairkernings * sizeof(float);
	offset = Align8(offset);
	assert( (offset & 7) == 0 );
	header.glyphs				= offset;

	FILE* file = fopen(path, "wb");
	if( !file )
		return 1;

	fwrite(&header, 1, sizeof(header), file);

	AlignFile8(file);

	assert( GetFileOffset(file) == header.codepoints );

	for( int i = 0; i < glyphs.size(); ++i )
	{
		const SFontGlyph& glyph = glyphs[i];
		fwrite( &glyph.codepoint, 1, sizeof(glyph.codepoint), file );

		//printf("%d: codepoint %d  bb %d, %d, %d, %d   adv %d  bx %d\n", i, glyph.codepoint, glyph.box[0], glyph.box[1], glyph.box[2], glyph.box[3], glyph.advance, glyph.bearing_x );
	}

	AlignFile8(file);

	assert( GetFileOffset(file) == header.pairkeys );

	for( int i = 0; i < pairkernings.size(); ++i )
	{
		const SFontPairKerning& pk = pairkernings[i];
		fwrite( &pk.key, 1, sizeof(pk.key), file );
	}

	AlignFile8(file);

	assert( GetFileOffset(file) == header.pairvalues );

	for( int i = 0; i < pairkernings.size(); ++i )
	{
		const SFontPairKerning& pk = pairkernings[i];

		fwrite( &pk.kerning, 1, sizeof(pk.kerning), file );
	}

	AlignFile8(file);

	assert( GetFileOffset(file) == header.glyphs );

	for( int i = 0; i < glyphs.size(); ++i )
	{
		const SFontGlyph& glyph = glyphs[i];
		fwrite( &glyph, 1, sizeof(glyph), file);
	}

	return 0;
}


static void half_size(const uint8_t* image, int width, int height, uint8_t* out)
{
	const size_t halfwidth = width/2;
	const size_t halfheight = height/2;

	for( uint32_t y = 0; y < halfheight; ++y)
	{
		for( uint32_t x = 0; x < halfwidth; ++x)
		{
			uint32_t xx = x*2;
			uint32_t yy = y*2;
			float s0 = image[ yy * width + xx ];
			float s1 = image[ yy * width + (xx+1) ];
			float s2 = image[ (yy+1) * width + xx ];
			float s3 = image[ (yy+1) * width + (xx+1) ];

			out[ y * halfwidth + x ] = uint8_t((s0 + s1 + s2 + s3) / 4.0f);
		}
	}
}

STBTT_DEF int stbtt_GetGlyphNumPairKernings(const stbtt_fontinfo* info)
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

STBTT_DEF int stbtt_GetGlyphPairKerning(const stbtt_fontinfo* info, int index, int* glyph1, int* glyph2, int* pairkerning)
{
   stbtt_uint8 *data = info->data + info->kern;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   if (ttUSHORT(data+8) != 1) // horizontal flag must be set in format
      return 0;

   if( index < 0 || index >= ttUSHORT(data+10) )
	   return 0;

   *pairkerning = ttSHORT(data+22+(index*6));
   int combination = ttULONG(data+18+(index*6)); // note: unaligned read
   *glyph1 = combination >> 16;
   *glyph2 = combination & 0xFFFF;
   return 1;
}

float clamp(float min, float max, float val)
{
	if( val < min )
		return min;
	else if( val > max )
		return max;
	return val;
}

int main(int argc, const char** argv)
{
	int fontsize = 32;
	int width = 1024;
	int height = 1024;
	int radius = 0;
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

	int numoversampling = 3;

	int bigwidth = width * (1 << numoversampling);
	int bigheight = height * (1 << numoversampling);

	size_t bigimagesize = (size_t)(bigwidth*bigheight);
	unsigned char* image = (unsigned char*)malloc(bigimagesize);
	memset(image, 0, bigimagesize);

	unsigned char* imagedist = (unsigned char*)malloc(bigimagesize);
	memset(imagedist, 0, bigimagesize);

	size_t imagesize = (size_t)(width*height);
	unsigned char* imageout = (unsigned char*)malloc(imagesize);
	memset(imageout, 0, imagesize);


	{
		stbtt_packedchar pdata[256*2];

		stbtt_pack_context pc;
		stbtt_pack_range pr[2];

		pr[0].chardata_for_range = pdata;
		pr[0].first_unicode_codepoint_in_range = 32;
		pr[0].num_chars = 1;
		pr[0].num_chars = 95;
		pr[0].array_of_unicode_codepoints = 0;
		pr[0].font_size = (float)fontsize*(1<<numoversampling);
		pr[1].chardata_for_range = pdata+256;
		pr[1].first_unicode_codepoint_in_range = 0xa0;
		pr[1].num_chars = 0x100 - 0xa0;
		pr[1].array_of_unicode_codepoints = 0;
		pr[1].font_size = (float)fontsize*(1<<numoversampling);

		stbtt_PackBegin(&pc, image, bigwidth, bigheight, 0, radius*2, NULL);
		//if( numoversampling > 1 )
		//	stbtt_PackSetOversampling(&pc, numoversampling, numoversampling);
		stbtt_PackFontRanges(&pc, fontfile, 0, pr, 2);
		stbtt_PackEnd(&pc);

		sdfBuildDistanceField(imagedist, bigwidth, (float)radius * (1 << numoversampling), image, bigwidth, bigheight, bigwidth);

		unsigned char* imagetmp1 = (unsigned char*)malloc(bigimagesize);
		unsigned char* imagetmp2 = (unsigned char*)malloc(bigimagesize);
		memcpy(imagetmp1, imagedist, bigimagesize);

		int w = width * (1 << numoversampling);
		int h = height * (1 << numoversampling);
		while( w > width )
		{
			half_size(imagetmp1, w, h, imagetmp2);
			memcpy(imagetmp1, imagetmp2, (w * h) / 4);

			w >>= 1;
			h >>= 1;
		}
		memcpy(imageout, imagetmp1, width*height);

		int glyph_to_codepoint[65536];
		memset(glyph_to_codepoint, 0, sizeof(glyph_to_codepoint));

		stbtt_fontinfo font;
		stbtt_InitFont(&font, fontfile, stbtt_GetFontOffsetForIndex(fontfile,0));

		std::vector<SFontGlyph> outglyphs;
		outglyphs.reserve( pr[0].num_chars + pr[1].num_chars );

		std::vector<SFontPairKerning> pairkernings;


		int lineascent, linedescend, linegap;
		stbtt_GetFontVMetrics(&font, &lineascent, &linedescend, &linegap);

		float fontscale = stbtt_ScaleForPixelHeight(&font, fontsize*(1 << numoversampling));

		float upfactor = 1 << numoversampling;
		float ooDownsample = 1.0f / upfactor;

		for( int r = 0; r < 2; ++r )
		{
			for( int i = 0; i < pr[r].num_chars; ++i )
			{
				stbtt_packedchar& c = pr[r].chardata_for_range[i];

				int codepoint = pr[r].first_unicode_codepoint_in_range + i;
				int glyphindex = stbtt_FindGlyphIndex(&font, codepoint);

				glyph_to_codepoint[glyphindex] = codepoint;

				int advance;
				int bearingx;
				stbtt_GetGlyphHMetrics(&font, glyphindex, &advance, &bearingx);

				SFontGlyph glyph;

				glyph.codepoint = codepoint;
				glyph.box[0]	= c.x0 * ooDownsample;
				glyph.box[1]	= c.y0 * ooDownsample;
				glyph.box[2]	= c.x1 * ooDownsample;
				glyph.box[3]	= c.y1 * ooDownsample;
				glyph.offset[0]	= c.xoff * ooDownsample;
				glyph.offset[1]	= c.yoff2 * ooDownsample;
				glyph.advance	= (advance * fontscale) * ooDownsample;
				glyph.bearing_x	= (bearingx * fontscale) * ooDownsample;
				outglyphs.push_back(glyph);

				//DrawBox(glyph.box[0], glyph.box[1], glyph.box[2], glyph.box[3], imageout, width, height);

				//printf("%c  advance: %d   packed: %f\n", codepoint, advance, c.xadvance);
				//printf("    xoff, yoff:  %f, %f    xoff2, yoff2: %f, %f\n", c.xoff, c.yoff, c.xoff2, c.yoff2);
			}
		}

		int numpairkernings = stbtt_GetGlyphNumPairKernings(&font);
		for( int i = 0; i < numpairkernings; ++i )
		{
			int glyph1, glyph2, kerning;
			stbtt_GetGlyphPairKerning(&font, i, &glyph1, &glyph2, &kerning);

			SFontPairKerning pairkerning;
			pairkerning.key		= (uint64_t(glyph_to_codepoint[glyph1]) << 32) | glyph_to_codepoint[glyph2];
			pairkerning.kerning = kerning * fontscale;
			pairkernings.push_back( pairkerning );

			//printf("pk %c %c   %d\n", glyph_to_codepoint[glyph1], glyph_to_codepoint[glyph2], kerning);
		}

		WriteFontInfo(&font, "output.font", width, height, fontsize, outglyphs, pairkernings);
	}

	char path[512];
	sprintf(path, "%s", outputfile);
	stbi_write_png(path, width, height, 1, imageout, 0);
	printf("wrote %s\n", path);

	free(image);

	return 0;
}
