/*
 * A simple test program to display the output of the voronoi generator
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define SDF_IMPLEMENTATION
#include "sdf.h"

#include "jc_sdf.h"

#include "font.h"

#include <algorithm>
#include <vector>
#include <map>


static void Usage()
{
	printf("Usage: sdffont [options]\n");
	printf("\t-i <inputpath> THe .ttf file\n");
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

/*
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
*/


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
	std::sort(pairkernings.begin(), pairkernings.end());

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

// https://www.microsoft.com/en-us/Typography/SpecificationsOverview.aspx -> ttch02.doc

STBTT_DEF int stbtt_GetGlyphNumPairKernings(const stbtt_fontinfo* info)
{
   stbtt_uint8 *data = info->data + info->kern;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   unsigned short coverage = ttUSHORT(data+8);
   if ((coverage & 1) != 1) // horizontal flag must be set in format
      return 0;
   unsigned short format = coverage >> 8;
   if( format == 0 )
	   return ttUSHORT(data+10);
   return 0;
}

STBTT_DEF int stbtt_GetGlyphPairKerning(const stbtt_fontinfo* info, int index, int* glyph1, int* glyph2, int* pairkerning)
{
   stbtt_uint8 *data = info->data + info->kern;

   // we only look at the first table. it must be 'horizontal' and format 0.
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) // number of tables, need at least 1
      return 0;
   if ((ttUSHORT(data+8) & 1) != 1) // horizontal flag must be set in format
      return 0;

   if( index < 0 || index >= ttUSHORT(data+10) )
	   return 0;

   *pairkerning = ttSHORT(data+22+(index*6));
   int combination = ttULONG(data+18+(index*6)); // note: unaligned read
   *glyph1 = combination >> 16;
   *glyph2 = combination & 0xFFFF;
   return 1;
}

uint64_t gettime()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

float clamp(float min, float max, float val)
{
	if( val < min )
		return min;
	else if( val > max )
		return max;
	return val;
}

static int NextPowerOfTwo(int n)
{
	--n;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	++n;
	return n;
}

static void Minify2x(unsigned char* image, uint32_t width, uint32_t height)
{
	uint32_t halfwidth = width / 2;
	uint32_t halfheight = height / 2;
	for( uint32_t ty = 0; ty < halfheight; ++ty)
	{
		for( uint32_t tx = 0; tx < halfwidth; ++tx)
		{
			float a = image[(ty*2+0) * width + (tx*2+0)];
			float b = image[(ty*2+0) * width + (tx*2+1)];
			float c = image[(ty*2+1) * width + (tx*2+0)];
			float d = image[(ty*2+1) * width + (tx*2+1)];
			image[ty * halfwidth + tx] = (unsigned char)((a + b + c + d) / 4.0f);
		}
	}
}

static void CopyBitmap(unsigned char* bitmap, uint32_t bitmapwidth, uint32_t bitmapheight,
						unsigned char* image, uint32_t imagewidth, uint32_t imageheight, uint32_t x, uint32_t y)
{
	for( uint32_t sy = 0; sy < bitmapheight; ++sy)
	{
		uint32_t ty = y + sy;
		if(ty >= imageheight)
			break;
		for( uint32_t sx = 0; sx < bitmapwidth; ++sx)
		{
			uint32_t tx = x + sx;
			if(tx >= imagewidth)
				break;
			image[ty * imagewidth + tx] = bitmap[sy * bitmapwidth + sx];
		}
	}
}

static void DebugPrintGlyph(double* bitmap, uint32_t w, uint32_t h)
{
	for( int y = 0; y < h; ++y)
	{
		for( int x = 0; x < w; ++x)
		{
			printf("%.02f  ", bitmap[y * w + x]);
		}
		printf("\n\n");
	}
}

int main(int argc, const char** argv)
{
	int fontsize = 32;
	int radius = 0;
	int padding[4] = { 0 };
	int numoversampling = 1;
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
		else if(strcmp(argv[i], "--paddingleft") == 0)
		{
			if( i+1 < argc )
				padding[0] = (int)atol(argv[i+1]);
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "--paddingright") == 0)
		{
			if( i+1 < argc )
				padding[2] = (int)atol(argv[i+1]);
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "--paddingtop") == 0)
		{
			if( i+1 < argc )
				padding[1] = (int)atol(argv[i+1]);
			else
			{
				Usage();
				return 1;
			}
		}
		else if(strcmp(argv[i], "--paddingbottom") == 0)
		{
			if( i+1 < argc )
				padding[3] = (int)atol(argv[i+1]);
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
		else if(strcmp(argv[i], "--numoversampling") == 0)
		{
			if( i+1 < argc )
				numoversampling = (int)atol(argv[i+1]);
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

	printf("Font is %s, chosen height is %d\n", inputfile, fontsize);
	printf("Outline radius is %d\n", radius);

	{
		stbtt_fontinfo f;
		if( !stbtt_InitFont(&f, fontfile, 0) )
		{
			fprintf(stderr, "Failed to init font %s\n", inputfile);
			return 1;
		}
		float scale = stbtt_ScaleForPixelHeight(&f, fontsize * numoversampling);

		int ranges[] = { 32, 32+95 };

		int totalnumcodepoints = 0;
		for( int r = 0; r < sizeof(ranges)/sizeof(ranges[0])/2; ++r)
		{
			int rangestart = ranges[r*2+0];
			int rangeend = ranges[r*2+1];
			totalnumcodepoints += rangeend - rangestart;
		}

		int numrects = totalnumcodepoints;
		stbrp_rect* packrects = new stbrp_rect[numrects];
		int c = 0;
		int area = 0;
		for( int r = 0; r < sizeof(ranges)/sizeof(ranges[0])/2; ++r)
		{
			int rangestart = ranges[r*2+0];
			int rangeend = ranges[r*2+1];
			for( int codepoint = rangestart; codepoint < rangeend; ++codepoint, ++c )
			{
				packrects[c].id = codepoint;
				int glyph = stbtt_FindGlyphIndex(&f, codepoint);

				int bbox[4];
				stbtt_GetGlyphBitmapBox(&f, glyph, scale ,scale, &bbox[0], &bbox[1], &bbox[2], &bbox[3]);


				packrects[c].w = bbox[2] - bbox[0];
				packrects[c].h = bbox[3] - bbox[1];
				packrects[c].w /= numoversampling;
				packrects[c].h /= numoversampling;
				packrects[c].w += padding[0] + padding[2] + radius*2;
				packrects[c].h += padding[1] + padding[3] + radius*2;

				area += packrects[c].w * packrects[c].h;
			}
		}

		int imagewidth = (int)sqrtf(area);
		imagewidth = NextPowerOfTwo(imagewidth);
		int imageheight = 1;
		while( imagewidth * imageheight < area )
		{
			if( imagewidth <= imageheight )
				imagewidth *= 2;
			else
				imageheight *= 2;
		}
		printf("area: %d\n", area);
		printf("initial w/h: %d x %d\n", imagewidth, imageheight);

		stbrp_context packctx;
		int numnodes = imagewidth;
		stbrp_node* packnodes = new stbrp_node[numnodes];
		int numtries = 3;
		while(--numtries > 0)
		{
			stbrp_init_target(&packctx, imagewidth, imageheight, packnodes, numnodes);
			stbrp_pack_rects(&packctx, packrects, numrects);

			int notpacked = 0;
			for( int i = 0; i < numrects; ++i )
			{
				notpacked |= packrects[i].was_packed ? 0 : 1;
			}

			if( notpacked )
			{
				delete[] packnodes;
				if( imagewidth <= imageheight )
					imagewidth *= 2;
				else
					imageheight *= 2;
				numnodes = imagewidth;
				packnodes = new stbrp_node[numnodes];

				printf("Didn't fit, increased to %d x %d\n", imagewidth, imageheight);
			}
		}

		int imagesize = imagewidth * imageheight;
		unsigned char* imageout = (unsigned char*)malloc(imagesize);
		memset(imageout, 0, imagesize);

		float fontscale = stbtt_ScaleForPixelHeight(&f, fontsize*numoversampling);
		int _lineascent, _linedescend, _linegap;
		stbtt_GetFontVMetrics(&f, &_lineascent, &_linedescend, &_linegap);
		float lineascent = _lineascent * fontscale;

		std::vector<SFontGlyph> outglyphs;
		std::map<int, int> glyph_to_codepoint;

		uint64_t totaltime = 0;
		uint64_t totaltimesdf = 0;

		uint32_t maxglyphwidth = radius * 2 + padding[0] * 2 + fontsize * numoversampling;
		uint32_t maxglyphheight = radius * 2 + padding[1] * 2 + fontsize * numoversampling;
		unsigned char* sdftemp = (unsigned char*)malloc(maxglyphwidth*maxglyphheight*sizeof(float)*3);

		unsigned char* bitmap = new unsigned char[maxglyphwidth*maxglyphheight];
		unsigned char* bitmapsdf = new unsigned char[maxglyphwidth*maxglyphheight];

		for( int i = 0; i < numrects; ++i)
		{
			int codepoint = packrects[i].id;
			int glyph = stbtt_FindGlyphIndex(&f, codepoint);
			glyph_to_codepoint.insert( std::make_pair(glyph, codepoint) );

			uint32_t bitmapwidth  	= packrects[i].w * numoversampling;
			uint32_t bitmapheight	= packrects[i].h * numoversampling;
			uint32_t bitmapsize 	= bitmapwidth * bitmapheight;
			memset(bitmap, 0, bitmapsize);

			// Since the bitmap is larger than the actual glyph, we need to offset the start
			uint32_t bitmapoffset 	= ((padding[1] + radius) * numoversampling)  * bitmapwidth + (padding[0] + radius) * numoversampling;
			uint32_t glyphwidth   	= bitmapwidth - padding[0] - padding[2] - radius*2;
			uint32_t glyphheight	= bitmapheight - padding[1] - padding[3] - radius*2;
			uint64_t ts = gettime();
			stbtt_MakeGlyphBitmap(&f, bitmap + bitmapoffset, glyphwidth, glyphheight, bitmapwidth, scale, scale, glyph);

			uint64_t te = gettime();
			totaltime += te - ts;

			assert(maxglyphwidth >= bitmapwidth);
			assert(maxglyphheight >= bitmapheight);

			/*
			int bmsize = bitmapwidth*bitmapheight;
			short* distx = (short*)malloc(bmsize*sizeof(short));
			short* disty = (short*)malloc(bmsize*sizeof(short));
			double* dist = (double*)malloc(bmsize*sizeof(double));
			double* gx = (double*)malloc(bmsize*sizeof(double));
			double* gy = (double*)malloc(bmsize*sizeof(double));
			double* img = (double*)malloc(bmsize*sizeof(double));
			*/

			ts = gettime();
			//sdfBuildDistanceFieldNoAlloc(bitmapsdf, bitmapwidth, radius*numoversampling, bitmap, bitmapwidth, bitmapheight, bitmapwidth, sdftemp);
			jc_sdf_dr_eedtaa3(bitmap, bitmapwidth, bitmapheight, bitmapsdf, bitmapwidth, radius*numoversampling);

			/*
			for( int xxx = 0; xxx < bmsize; ++xxx )
			{
				img[xxx] = bitmap[xxx] / 255.0;
			}
			computegradient(img, bitmapwidth, bitmapheight, gx, gy);
			edtaa4(img, gx, gy, bitmapwidth, bitmapheight, distx, disty, dist);
			postprocess(img, gx, gy, bitmapwidth, bitmapheight, distx, disty, dist);
			for( int xxx = 0; xxx < bmsize; ++xxx )
			{
				double v = dist[xxx] / radius;
				if( v > 1.0 )
					v = 1.0;
				if( bitmap[xxx] == 255 )
					v = -v;
				bitmapsdf[xxx] = (uint8_t)((0.5 - 0.5 * v) * 255.0);
			}
			 */

			te = gettime();
			totaltimesdf += te-ts;
			totaltime += te-ts;

			for( int o = 1; o < numoversampling; ++o )
				Minify2x(bitmapsdf, bitmapwidth, bitmapheight);

			CopyBitmap(bitmapsdf, bitmapwidth/numoversampling, bitmapheight/numoversampling, imageout, imagewidth, imageheight, packrects[i].x, packrects[i].y);

			int advance;
			int bearingx;
			stbtt_GetGlyphHMetrics(&f, glyph, &advance, &bearingx);

			int x1, y1, x2, y2;
			stbtt_GetGlyphBitmapBox(&f, glyph, fontscale, fontscale, &x1, &y1, &x2, &y2);

			SFontGlyph outglyph;

			outglyph.codepoint = codepoint;
			outglyph.box[0]	= packrects[i].x;
			outglyph.box[1]	= packrects[i].y;
			outglyph.box[2]	= (packrects[i].x + packrects[i].w);
			outglyph.box[3]	= (packrects[i].y + packrects[i].h);
			outglyph.offset[0]	= 0;//padding[0] + radius;
			outglyph.offset[1]	= y2 / numoversampling;//padding[1] + radius + -y1 / numoversampling;// (lineascent - packrects[i].h);
			outglyph.advance	= (advance * scale) / numoversampling;
			outglyph.bearing_x	= (bearingx * scale) / numoversampling;
			outglyphs.push_back(outglyph);

			/*if( codepoint == 'T' || codepoint == 'a' || codepoint == 'e')
			{
				printf("%c  codepoint = %d\n", (char)codepoint, codepoint);
				printf("  box = t,l: %d, %d  b,r: %d, %d\n", outglyph.box[0], outglyph.box[1], outglyph.box[2], outglyph.box[3]);
				printf("  offset = %f, %f\n", outglyph.offset[0], outglyph.offset[1]);
				printf("  advance = %f\n", outglyph.advance);
				printf("  bearing_x = %f\n", outglyph.bearing_x);

				printf("  bbox: %d, %d   %d, %d\n", x1, y1, x2, y2);
				printf("\n");
			}*/
		}

		delete[] bitmap;
		delete[] bitmapsdf;

		printf("Max bitmap size: %d, %d\n", maxglyphwidth, maxglyphheight);
		printf("Average %llu us\n", totaltime/numrects);
		printf("Average sdf %llu us\n", totaltimesdf/numrects);
		printf("Total %llu us for %d glyphs\n", totaltime, numrects);
		printf("Total sdf %llu us\n", totaltimesdf);


		char path[512];
		sprintf(path, "%s.png", outputfile);
		stbi_write_png(path, imagewidth, imageheight, 1, imageout, 0);
		printf("Wrote %s\n", path);

		free(imageout);

		std::vector<SFontPairKerning> pairkernings;
		int numpairkernings = stbtt_GetGlyphNumPairKernings(&f);
		for( int i = 0; i < numpairkernings; ++i )
		{
			int glyph1, glyph2, kerning;
			stbtt_GetGlyphPairKerning(&f, i, &glyph1, &glyph2, &kerning);

			std::map<int, int>::const_iterator it1 = glyph_to_codepoint.find(glyph1);
			std::map<int, int>::const_iterator it2 = glyph_to_codepoint.find(glyph2);
			if( it1 == glyph_to_codepoint.end() || it2 == glyph_to_codepoint.end() )
				continue;

			SFontPairKerning pairkerning;
			pairkerning.key		= (uint64_t(it2->second) << 32) | it1->second;
			pairkerning.kerning = (kerning * fontscale) / numoversampling;
			pairkernings.push_back( pairkerning );

			//printf("pk %c %c   %f 0x%016llx (%d  %d),  (%d  %d)\n", it1->second, it2->second, kerning * fontscale, pairkerning.key, it1->second, it2->second, glyph1, glyph2);
		}

		WriteFontInfo(&f, outputfile, imagewidth, imageheight, fontsize, outglyphs, pairkernings);
		printf("Wrote %s\n", outputfile);
	}

	return 0;
}
