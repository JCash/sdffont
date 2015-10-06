#pragma once

struct SFontGlyph
{
	uint32_t	codepoint;
	uint16_t	box[4];		// pixels
	float		offset[2];	// pixels
	float		advance;	// pixels
	float		bearing_x;	// pixels
};

struct SFontPairKerning
{
	uint64_t	key;		// (codepoint1 << 32) | codepoint2
	float		kerning;	// texel coords
};

struct SFontHeader
{
	char		magic[4];
	uint16_t	texturesize_width;
	uint16_t	texturesize_height;
	uint16_t	fontsize;		// In pixels
	float		line_ascend;	// pixels
	float		line_descend; 	// pixels
	float		line_gap; 		// pixels
	uint16_t	num_glyphs;
	uint16_t	num_pairkernings;
	uint8_t		_pad[8];
	// 24 bytes
	// Offsets into the file where to find data (0 based, i.e from beginning of file)
	uint64_t	codepoints;		// num_glyphs long list of sorted code points. Used to determine glyph index for a code point
	uint64_t	pairkeys;		// num_pairkernings long list of codepoint pairs
	uint64_t	pairvalues;		// num_pairkernings long list of pair kernings (Each value is an unscaled coordinate (int16_t) )
	uint64_t	glyphs;			// num_glyphs long list of SFontGlyphs
};

bool operator< (const SFontGlyph& lhs, const SFontGlyph& rhs)
{
	return lhs.codepoint < rhs.codepoint;
}
