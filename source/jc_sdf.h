#pragma once

/** A collection of distance transforms
 */

typedef unsigned char 	u8;
typedef unsigned int  	u32;
typedef float  	  		_jc_sdf_float;
#define JC_SDF_SQRTFN	sqrtf
#define JC_SDF_SQRT2 	1.4142135623730951f

#define _JC_BIG_VAL 1000000.0f

static inline _jc_sdf_float _jc_sdf_clamp01(_jc_sdf_float a)
{
	return a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
}

struct _jc_point_s
{
	int16_t x;
	int16_t y;
};

struct _jc_point
{
	int x;
	int y;
};

struct _jc_point_f
{
	_jc_sdf_float x;
	_jc_sdf_float y;
};


static inline _jc_sdf_float _jc_sdf_distsqr(_jc_sdf_float x1, _jc_sdf_float y1, _jc_sdf_float x2, _jc_sdf_float y2)
{
	_jc_sdf_float xdiff = x2 - x1;
	_jc_sdf_float ydiff = y2 - y1;
	return xdiff*xdiff + ydiff*ydiff;
}


static _jc_sdf_float _jc_sdf_calc_edge_df(_jc_sdf_float gx, _jc_sdf_float gy, _jc_sdf_float a)
{
	_jc_sdf_float df, a1;

	if ((gx == 0) || (gy == 0))
	{ // Either A) gu or gv are zero, or B) both
		df = 0.5 - a;  // Linear approximation is A) correct or B) a fair guess
	}
	else
	{
		_jc_sdf_float glength = sqrt(gx * gx + gy * gy);
		if (glength > 0)
		{
			gx = gx / glength;
			gy = gy / glength;
		}
		/* Everything is symmetric wrt sign and transposition,
		 * so move to first octant (gx>=0, gy>=0, gx>=gy) to
		 * avoid handling all possible edge directions.
		 */
		gx = fabs(gx);
		gy = fabs(gy);
		if (gx < gy)
		{
			_jc_sdf_float temp = gx;
			gx = gy;
			gy = temp;
		}
		a1 = 0.5f * gy / gx;
		if (a < a1)
		{ // 0 <= a < a1
			df = 0.5f * (gx + gy) - sqrt(2.0f * gx * gy * a);
		}
		else if (a < (1.0 - a1))
		{ // a1 <= a <= 1-a1
			df = (0.5f - a) * gx;
		}
		else
		{ // 1-a1 < a <= 1
			df = -0.5f * (gx + gy) + sqrt(2.0f * gx * gy * (1.0f - a));
		}
	}
	return df;
}

inline _jc_sdf_float _jc_sdf_calc_dist(const u8* image, const _jc_point_f* gradients, int width, int c, int xc, int yc, int xi, int yi)
{
	int closest = c - xc - yc * width;
	_jc_sdf_float a = image[closest]/255.0f;

	a = _jc_sdf_clamp01(a);
	if( a == 0.0f )
		return _JC_BIG_VAL;

	_jc_sdf_float disq = (_jc_sdf_float)(xi * xi + yi * yi);
	if( disq == 0 )
	{
		// If we're very close to the edge
		return _jc_sdf_calc_edge_df(gradients[closest].x, gradients[closest].y, a);
	}
	else
	{
		return JC_SDF_SQRTFN( disq ) + _jc_sdf_calc_edge_df(xi, yi, a);
	}
}

void jc_sdf_dr_eedtaa3(const u8* image, u32 width, u32 height, u8* out, u32 outwidth, u32 radius)
{
	u32 size = width * height;
	_jc_point_f* pts = (_jc_point_f*)malloc(size * sizeof(_jc_point_f));
	_jc_sdf_float* dist = (_jc_sdf_float*)malloc(size * sizeof(_jc_sdf_float));
	_jc_point_f* gradients = (_jc_point_f*)malloc(size * sizeof(_jc_point_f));

	for( u32 y = 0, i = 0; y < height; ++y )
	{
		for( u32 x = 0; x < width; ++x, ++i )
		{
			pts[i].x = 0;
			pts[i].y = 0;
			dist[i] = _JC_BIG_VAL;

			if( image[i] == 255 )
				continue;

			if( image[i] == 0 )
			{
				int v = (x >= 1 ? image[i-1] == 255 : 0) || (x <= width-2 ? image[i+1] == 255 : 0) ||
						(y >= 1 ? image[i-width] == 255 : 0) || (y <= height-2 ? image[i+width] == 255 : 0);
				if( !v )
				{
					continue;
				}
			}

			//
			gradients[i].x = (-image[i-width-1] - JC_SDF_SQRT2 * image[i-1] - image[i+width-1] + image[i-width+1] + JC_SDF_SQRT2 * image[i+1] + image[i+width+1]) / (_jc_sdf_float)(255 * 6);
			gradients[i].y = (-image[i-width-1] - JC_SDF_SQRT2 * image[i-width] - image[i-width+1] + image[i+width-1] + JC_SDF_SQRT2 * image[i+width] + image[i+width+1]) / (_jc_sdf_float)(255 * 6);
			_jc_sdf_float lensq = gradients[i].x*gradients[i].x + gradients[i].y*gradients[i].y;
			if( lensq )
			{
				_jc_sdf_float len_inv = (_jc_sdf_float)1 / JC_SDF_SQRTFN(lensq);
				gradients[i].x *= len_inv;
				gradients[i].y *= len_inv;
			}

			_jc_sdf_float a = image[i]/255.0f;
			_jc_sdf_float df = _jc_sdf_calc_edge_df(gradients[i].x, gradients[i].y, a);
			pts[i].x = x + gradients[i].x * df;
			pts[i].y = y + gradients[i].y * df;
			dist[i]	 = _jc_sdf_distsqr( x, y, pts[i].x, pts[i].y );
		}
	}

#define CALC_DIST( OFFSET ) 												\
	{																		\
		int c = i + (OFFSET);												\
		if(dist[c] < dist[i])	{											\
			_jc_sdf_float d = _jc_sdf_distsqr(x, y, pts[c].x, pts[c].y);	\
			if( d < dist[i]) {												\
				pts[i] = pts[c];											\
				dist[i] = d;												\
				changed = 1;												\
			}																\
		}																	\
	}

	// 0 1 2
	// 3 c 4
	// 5 6 7
	int indexoffsets[8] = { -width-1, -width, -width+1, -1,  +1, width-1, width, width+1 };

	int changed = 1;
	int loopcount = 0;
	while( changed && loopcount < 1 )
	{
		changed = 0;

		for( int y = 1; y < height - 1; ++y )
		{
			for( int x = 1; x < width - 1; ++x )
			{
				int i = y * width + x;

				// Left and top
				CALC_DIST( indexoffsets[0] );
				CALC_DIST( indexoffsets[1] );
				CALC_DIST( indexoffsets[2] );
				CALC_DIST( indexoffsets[3] );
			}
		}

		for( int y = height-2; y >= 0; --y )
		{
			for( int x = width-2; x >= 1; --x )
			{
				int i = y * width + x;

				// Right and bottom
				CALC_DIST( indexoffsets[4] );
				CALC_DIST( indexoffsets[5] );
				CALC_DIST( indexoffsets[6] );
				CALC_DIST( indexoffsets[7] );
			}
		}

		++loopcount;
	}

	_jc_sdf_float scale = 1.0f / radius;
	for(int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			int i = y * width + x;
			_jc_sdf_float d = JC_SDF_SQRTFN(dist[i]) * scale * (image[i] > 127 ? -1 : 1);
			out[i] = (u8)(_jc_sdf_clamp01(0.5f - d * 0.5f) * 255.0f);
		}
	}

#undef CALC_DIST

	free(dist);
	free(pts);
	free(gradients);
}
