
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <assert.h>

#include "font.h"

/*

info face="Verdana"
size=90
ascent= 0
descent= 0
chars count=229
char id=0     x=980   y=847   width=8     height=8     xoffset=-3.000    yoffset=3.000     xadvance=0.000       page=0  chnl=0
char id=8     x=1013  y=1016  width=8     height=8     xoffset=-3.000    yoffset=3.000     xadvance=0.000       page=0  chnl=0
char id=9     x=1005  y=1016  width=8     height=8     xoffset=-3.000    yoffset=3.000     xadvance=31.625      page=0  chnl=0

 */

const char* tag_face="info face=";
const char* tag_size="size=";
const char* tag_ascent="ascent=";
const char* tag_descent="descent=";
const char* tag_count="chars count=";
const char* tag_char="char id=";

int str_startswith(const char* s, const char* start)
{
	int r =  strstr(s, start) == s;
	return r;
}

struct SFont
{
	int		size;
	float	lineascend;
	float	linedescend;
	float	linegap;
	int		texturesize[0];
	std::vector<SFontGlyph> glyphs;
	std::vector<SFontPairKerning> pairkernings;
};

int load_angelcode(const char* path, SFont& font)
{
	FILE* file = fopen(path, "rb");
	if( !file )
	{
		fprintf(stderr, "Failed to load %s\n", path);
		return 1;
	}

	while( !feof(file) )
	{
		char line[4096];
		if( fgets(line, sizeof(line), file) )
		{
			if( str_startswith(line, tag_face) )
				continue;
			else if( str_startswith(line, tag_size) )
				font.size = (int)atol(line+strlen(tag_size));
			else if( str_startswith(line, tag_char) )
			{
				int codepoint;
				int x, y, w, h, page, channel;
				float xoff, yoff, advance;
				int n = sscanf(line, "char id=%d     x=%d  y=%d  width=%d     height=%d     xoffset=%f    yoffset=%f     xadvance=%f      page=%d  chnl=%d",
								&codepoint, &x, &y, &w, &h, &xoff, &yoff, &advance, &page, &channel);

				assert(n == 10);

				SFontGlyph glyph = {};
				glyph.codepoint = codepoint;
				glyph.box[0] = x;
				glyph.box[1] = y;
				glyph.box[2] = x + w;
				glyph.box[3] = y + h;
				glyph.offset[0] = xoff;
				glyph.offset[1] = yoff;
				glyph.advance	= advance;
				glyph.bearing_x = 0;

				font.glyphs.push_back(glyph);
			}
		}
	}

	return 0;
}

static uint64_t GetFileOffset(FILE* f)
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

static int WriteFontInfo(const char* path, SFont& font)
{
	std::sort(font.glyphs.begin(), font.glyphs.end());

	SFontHeader header = {};
	header.magic[0] 			= 'F';
	header.magic[1] 			= 'O';
	header.magic[2] 			= 'N';
	header.magic[3] 			= 'T';
	header.texturesize_width	= 1024;//font.texturesize[0];
	header.texturesize_height	= 1024;//font.texturesize[1];
	header.fontsize				= font.size;

	header.line_ascend			= font.lineascend;
	header.line_descend			= font.linedescend;
	header.line_gap				= font.linegap;
	header.num_glyphs			= (uint16_t)font.glyphs.size();
	header.num_pairkernings		= (uint16_t)font.pairkernings.size();

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

	for( int i = 0; i < font.glyphs.size(); ++i )
	{
		const SFontGlyph& glyph = font.glyphs[i];
		fwrite( &glyph.codepoint, 1, sizeof(glyph.codepoint), file );

		//printf("%d: codepoint %d  bb %d, %d, %d, %d   adv %d  bx %d\n", i, glyph.codepoint, glyph.box[0], glyph.box[1], glyph.box[2], glyph.box[3], glyph.advance, glyph.bearing_x );
	}

	AlignFile8(file);

	assert( GetFileOffset(file) == header.pairkeys );

	for( int i = 0; i < font.pairkernings.size(); ++i )
	{
		const SFontPairKerning& pk = font.pairkernings[i];
		fwrite( &pk.key, 1, sizeof(pk.key), file );
	}

	AlignFile8(file);

	assert( GetFileOffset(file) == header.pairvalues );

	for( int i = 0; i < font.pairkernings.size(); ++i )
	{
		const SFontPairKerning& pk = font.pairkernings[i];

		fwrite( &pk.kerning, 1, sizeof(pk.kerning), file );
	}

	AlignFile8(file);

	assert( GetFileOffset(file) == header.glyphs );

	for( int i = 0; i < font.glyphs.size(); ++i )
	{
		const SFontGlyph& glyph = font.glyphs[i];
		fwrite( &glyph, 1, sizeof(glyph), file);
	}

	return 0;
}


static void Usage()
{
	fprintf(stderr, "Usage: angelcode2font <angelcode.ttf_sdf.txt>");
}

int main(int argc, const char** argv)
{
	if( argc < 2 )
	{
		Usage();
		return 1;
	}

	SFont font;

	load_angelcode(argv[1], font);

	printf("Font size %d\n", font.size);
	printf("%d glyphs\n", (int)font.glyphs.size());

	const char* outputpath = "test2.font";
	WriteFontInfo(outputpath, font);

	printf("Wrote %s\n", outputpath);
	return 0;
}
