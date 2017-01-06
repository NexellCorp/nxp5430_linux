#include "vr_common_inc.h"

const char deinterace_vertex_shader[] = {
"	#version 100													\n\
																	\n\
	precision highp float;											\n\
																	\n\
	attribute vec4 a_v4Position;									\n\
	attribute vec2 a_v2TexCoord;									\n\
	uniform float u_fTexHeight;										\n\
																	\n\
	varying vec2 v_tex0;											\n\
	varying vec2 v_tex1;											\n\
	varying vec2 v_tex2;											\n\
	varying vec2 v_tex3;											\n\
	varying vec2 v_tex4;											\n\
	varying vec2 v_offset_y;										\n\
																	\n\
	void main()														\n\
	{																\n\
		float size_of_texel = 1.0/u_fTexHeight;						\n\
																	\n\
		v_offset_y = a_v2TexCoord * vec2(1.0,(u_fTexHeight/2.0));	\n\
																	\n\
		v_tex0 = a_v2TexCoord + vec2(0.0,(size_of_texel*-2.0));		\n\
		v_tex1 = a_v2TexCoord + vec2(0.0,(size_of_texel*-1.0));		\n\
		v_tex2 = a_v2TexCoord;										\n\
		v_tex3 = a_v2TexCoord + vec2(0.0,(size_of_texel* 1.0));		\n\
		v_tex4 = a_v2TexCoord + vec2(0.0,(size_of_texel* 2.0));		\n\
																	\n\
		gl_Position = a_v4Position;									\n\
	}																\n\
"
};

const char deinterace_frag_shader[] = {
" 	#version 100																	\n\
																					\n\
	//precision mediump float;														\n\
	precision highp float;															\n\
																					\n\
	uniform float u_fTexHeight;														\n\
	uniform sampler2D diffuse;														\n\
	uniform sampler2D ref_tex;														\n\
																					\n\
	varying vec2 v_tex0;															\n\
	varying vec2 v_tex1;															\n\
	varying vec2 v_tex2;															\n\
	varying vec2 v_tex3;															\n\
	varying vec2 v_tex4;															\n\
	varying vec2 v_offset_y;														\n\
																					\n\
	void main()																		\n\
	{																				\n\
		vec4 tval0 = vec4(0.0, 0.0, 0.0, 0.0), tval1 = vec4(0.0, 0.0, 0.0, 0.0);	\n\
																					\n\
		//	 deinterface without scaling											\n\
		vec4 y0_frac = texture2D(ref_tex, v_offset_y);								\n\
		if( y0_frac.x == 1.0 )														\n\
		{																			\n\
			tval0 += texture2D(diffuse, v_tex0) * (-1.0/8.0);						\n\
			tval0 += texture2D(diffuse, v_tex1) * ( 4.0/8.0);						\n\
			tval0 += texture2D(diffuse, v_tex2) * ( 2.0/8.0);						\n\
			tval0 += texture2D(diffuse, v_tex3) * ( 4.0/8.0);						\n\
			tval0 += texture2D(diffuse, v_tex4) * (-1.0/8.0);						\n\
		}																			\n\
		else //even																	\n\
		{																			\n\
			tval0 = texture2D(diffuse, v_tex2);										\n\
		}																			\n\
																					\n\
		gl_FragColor = tval0;														\n\
																					\n\
		//for debugging																\n\
		//gl_FragColor = texture2D(diffuse, v_v2TexCoord.xy);						\n\
	}																				\n\
"
};

const char scaler_vertex_shader[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec2 a_v2TexCoord;	\n\
	varying vec2 v_tex;				\n\
									\n\
	void main()						\n\
	{								\n\
		v_tex = a_v2TexCoord;		\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};

const char scaler_frag_shader[] = {
"													\n\
	#version 100									\n\
													\n\
	precision highp float;							\n\
	uniform sampler2D diffuse;						\n\
	varying vec2 v_tex;								\n\
													\n\
	void main()										\n\
	{												\n\
		gl_FragColor = texture2D(diffuse, v_tex);	\n\
		//vec4 tval0 = vec4(1.0, 1.0, 1.0, 1.0); \n\
		//gl_FragColor = tval0; \n\
	}												\n\
"
};

const char cvt2y_vertex_shader[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec2 a_v2TexCoord;	\n\
	varying vec2 v_tex;				\n\
									\n\
	void main()						\n\
	{								\n\
		v_tex = a_v2TexCoord;		\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};

//compile error =>precision highp float;
const char cvt2y_frag_shader[] = {
"													\n\
	#version 100									\n\
													\n\
	precision mediump float;						\n\
	uniform sampler2D diffuse;						\n\
	varying vec2 v_tex;								\n\
													\n\
	void main()										\n\
	{												\n\
		vec4 tval;									\n\
		float color;									\n\
		tval = texture2D(diffuse, v_tex);	\n\
		color = (0.257*tval.x) + (0.504*tval.y) + (0.098*tval.z) + 0.0625; \n\
		gl_FragColor = vec4(color, color, color, color); \n\
	}												\n\
"
};

const char cvt2uv_vertex_shader[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec2 a_v2TexCoord;	\n\
	varying vec2 v_tex;				\n\
									\n\
	void main()						\n\
	{								\n\
		v_tex = a_v2TexCoord;		\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};

#ifdef VR_FEATURE_YCRCB_NV21_USE
//compile error =>precision highp float;
//NV12 : Y,U/V
//color.b = Cb(U)
//color.g = Cr(V)
const char cvt2uv_frag_shader[] = {
"													\n\
	#version 100									\n\
													\n\
	precision mediump float;						\n\
	uniform sampler2D diffuse;						\n\
	varying vec2 v_tex;								\n\
													\n\
	void main()										\n\
	{												\n\
		vec4 tval;	\n\
		vec4 color = vec4(0.0, 0.0, 0.0, 0.0);	\n\
		tval = texture2D(diffuse, v_tex);	\n\
		color.b = -(0.148*tval.x) - (0.291*tval.y) + (0.439*tval.z) + 0.5; \n\
		color.g = (0.439*tval.x) - (0.368*tval.y) - (0.071*tval.z) + 0.5; \n\
		gl_FragColor = color; \n\
	}												\n\
"
};
#else
//NV21 : Y,V/U
//color.g = Cr(V)
//color.b = Cb(U)
const char cvt2uv_frag_shader[] = {
"													\n\
	#version 100									\n\
													\n\
	precision mediump float;						\n\
	uniform sampler2D diffuse;						\n\
	varying vec2 v_tex;								\n\
													\n\
	void main()										\n\
	{												\n\
		vec4 tval;	\n\
		vec4 color = vec4(0.0, 0.0, 0.0, 0.0);	\n\
		tval = texture2D(diffuse, v_tex);	\n\
		color.g = -(0.148*tval.x) - (0.291*tval.y) + (0.439*tval.z) + 0.5; \n\
		color.b = (0.439*tval.x) - (0.368*tval.y) - (0.071*tval.z) + 0.5; \n\
		gl_FragColor = color; \n\
	}												\n\
"
};
#endif

const char cvt2rgba_vertex_shader[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec2 a_v2TexCoordY;	\n\
	attribute vec2 a_v2TexCoordU;	\n\
	attribute vec2 a_v2TexCoordV;	\n\
	varying vec2 v_texY;			\n\
	varying vec2 v_texU; 			\n\
	varying vec2 v_texV; 			\n\
									\n\
	void main()						\n\
	{								\n\
		v_texY = a_v2TexCoordY;		\n\
		v_texU = a_v2TexCoordU;		\n\
		v_texV = a_v2TexCoordV;		\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};

//compile error =>precision highp float;
const char cvt2rgba_frag_shader[] = {
"													\n\
	#version 100									\n\
													\n\
	precision mediump float;						\n\
	uniform sampler2D diffuseY;						\n\
	uniform sampler2D diffuseU;						\n\
	uniform sampler2D diffuseV;						\n\
	varying vec2 v_texY;							\n\
	varying vec2 v_texU;							\n\
	varying vec2 v_texV;							\n\
													\n\
	void main()										\n\
	{												\n\
		float tvalY, tvalU, tvalV;	\n\
		vec4 color = vec4(0.0, 0.0, 0.0, 0.0);	\n\
		tvalY = texture2D(diffuseY, v_texY).x;	\n\
		tvalU = texture2D(diffuseU, v_texU).x;	\n\
		tvalV = texture2D(diffuseV, v_texV).x;	\n\
		color.r = 1.164 * (tvalY - 0.0625) + 1.596 * (tvalV - 0.5); \n\
		color.g = 1.164 * (tvalY - 0.0625) - 0.813 * (tvalV - 0.5) - 0.391 * (tvalU - 0.5); \n\
		color.b = 1.164 * (tvalY - 0.0625) + 2.018 * (tvalU - 0.5); \n\
		color.a = 0.0; \n\
		gl_FragColor = color; \n\
	}												\n\
"
};

const char userfilter_vertex_shader[] = {
"	#version 100													\n\
																	\n\
	precision highp float;											\n\
																	\n\
	attribute vec4 a_v4Position;									\n\
	attribute vec2 a_v2TexCoord;									\n\
	uniform float u_fTexWidth;										\n\
	uniform float u_fTexHeight; 									\n\
																	\n\
	varying vec2 v_tex_00;											\n\
	varying vec2 v_tex_01;											\n\
	varying vec2 v_tex_02;											\n\
	varying vec2 v_tex_10;											\n\
	varying vec2 v_tex_11;											\n\
	varying vec2 v_tex_12;											\n\
	varying vec2 v_tex_20;											\n\
	varying vec2 v_tex_21;											\n\
	varying vec2 v_tex_22;											\n\
																	\n\
	void main()														\n\
	{																\n\
		float size_of_texel_x = 1.0/u_fTexWidth;						\n\
		float size_of_texel_y = 1.0/u_fTexHeight;						\n\
																	\n\
		v_tex_00 = a_v2TexCoord + vec2((size_of_texel_x*-1.0),(size_of_texel_y*+1.0));		\n\
		v_tex_01 = a_v2TexCoord + vec2(0,					  (size_of_texel_y*+1.0));		\n\
		v_tex_02 = a_v2TexCoord + vec2((size_of_texel_x*+1.0),(size_of_texel_y*+1.0));		\n\
		v_tex_10 = a_v2TexCoord + vec2((size_of_texel_x*-1.0),0);		\n\
		v_tex_11 = a_v2TexCoord + vec2(0,					  0);		\n\
		v_tex_12 = a_v2TexCoord + vec2((size_of_texel_x*+1.0),0);		\n\
		v_tex_20 = a_v2TexCoord + vec2((size_of_texel_x*-1.0),(size_of_texel_y*-1.0));		\n\
		v_tex_21 = a_v2TexCoord + vec2(0,					  (size_of_texel_y*-1.0));		\n\
		v_tex_22 = a_v2TexCoord + vec2((size_of_texel_x*+1.0),(size_of_texel_y*-1.0));		\n\
																	\n\
		gl_Position = a_v4Position;									\n\
	}																\n\
"
};

#if 1 
const char userfilter_frag_shader[] = {
" 	#version 100																	\n\
																					\n\
	precision mediump float;														\n\
	//precision highp float;															\n\
																					\n\
	uniform sampler2D diffuse;														\n\
																					\n\
	varying vec2 v_tex_00; // 1/16											\n\
	varying vec2 v_tex_01; // 1/8											\n\
	varying vec2 v_tex_02; // 1/16											\n\
	varying vec2 v_tex_10; // 1/8											\n\
	varying vec2 v_tex_11; // 1/4											\n\
	varying vec2 v_tex_12; // 1/8											\n\
	varying vec2 v_tex_20; // 1/16											\n\
	varying vec2 v_tex_21; // 1/8											\n\
	varying vec2 v_tex_22; // 1/16											\n\
																					\n\
	void main()																		\n\
	{																				\n\
		vec4 tval0 = vec4(0.0, 0.0, 0.0, 0.0);										\n\
		if(v_tex_11.x < 0.286)																			\n\
		{																		\n\
			tval0 += texture2D(diffuse, v_tex_00) * (1.0/16.0);						\n\
			tval0 += texture2D(diffuse, v_tex_01) * (1.0/8.0);						\n\
			tval0 += texture2D(diffuse, v_tex_02) * (1.0/16.0);						\n\
			tval0 += texture2D(diffuse, v_tex_10) * (1.0/8.0);						\n\
			tval0 += texture2D(diffuse, v_tex_11) * (1.0/4.0); 					\n\
			tval0 += texture2D(diffuse, v_tex_12) * (1.0/8.0);						\n\
			tval0 += texture2D(diffuse, v_tex_20) * (1.0/16.0);						\n\
			tval0 += texture2D(diffuse, v_tex_21) * (1.0/8.0); 					\n\
			tval0 += texture2D(diffuse, v_tex_22) * (1.0/16.0);						\n\
																						\n\
			//tval0 = vec4(0.0, 1.0, 0.0, 1.0); \n\										\n\
			gl_FragColor = tval0;														\n\
			//gl_FragColor = texture2D(diffuse, v_tex_11) * (-1.0/4.0);					\n\
		}																			\n\
		else																			\n\
		{																			\n\
			gl_FragColor = texture2D(diffuse, v_tex_11);																		\n\
		}																			\n\
		//for debugging																\n\
		//gl_FragColor = texture2D(diffuse, v_v2TexCoord.xy);						\n\
	}																				\n\
"
};
#else /*org*/
const char userfilter_frag_shader[] = {
" 	#version 100																	\n\
																					\n\
	//precision mediump float;														\n\
	precision highp float;															\n\
																					\n\
	uniform sampler2D diffuse;														\n\
																					\n\
	varying vec2 v_tex_00; // 1/16											\n\
	varying vec2 v_tex_01; // 1/8											\n\
	varying vec2 v_tex_02; // 1/16											\n\
	varying vec2 v_tex_10; // 1/8											\n\
	varying vec2 v_tex_11; // 1/4											\n\
	varying vec2 v_tex_12; // 1/8											\n\
	varying vec2 v_tex_20; // 1/16											\n\
	varying vec2 v_tex_21; // 1/8											\n\
	varying vec2 v_tex_22; // 1/16											\n\
																					\n\
	void main()																		\n\
	{																				\n\
		vec4 tval0 = vec4(0.0, 0.0, 0.0, 0.0);										\n\
																					\n\
		tval0 += texture2D(diffuse, v_tex_00) * (1.0/16.0);						\n\
		tval0 += texture2D(diffuse, v_tex_01) * (1.0/8.0);						\n\
		tval0 += texture2D(diffuse, v_tex_02) * (1.0/16.0);						\n\
		tval0 += texture2D(diffuse, v_tex_10) * (1.0/8.0);						\n\
		tval0 += texture2D(diffuse, v_tex_11) * (1.0/4.0); 					\n\
		tval0 += texture2D(diffuse, v_tex_12) * (1.0/8.0);						\n\
		tval0 += texture2D(diffuse, v_tex_20) * (1.0/16.0);						\n\
		tval0 += texture2D(diffuse, v_tex_21) * (1.0/8.0); 					\n\
		tval0 += texture2D(diffuse, v_tex_22) * (1.0/16.0);						\n\
																					\n\
		//tval0 = vec4(0.0, 1.0, 0.0, 1.0); \n\										\n\
		gl_FragColor = tval0;														\n\
		//gl_FragColor = texture2D(diffuse, v_tex_11) * (-1.0/4.0);					\n\
																					\n\
		//for debugging																\n\
		//gl_FragColor = texture2D(diffuse, v_v2TexCoord.xy);						\n\
	}																				\n\
"
};

#endif

const char display_vertex_shader[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec4 a_v4Color; 		\n\
	attribute vec2 a_v2TexCoord;	\n\
	varying vec2 v_v2Tex;			\n\
	varying vec4 v_v4FillColor; 	\n\
									\n\
	void main()						\n\
	{								\n\
		v_v4FillColor = a_v4Color;	\n\	
		v_v2Tex = a_v2TexCoord;		\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};

const char display_frag_shader[] = {
"													\n\
	#version 100									\n\
													\n\
	precision highp float;							\n\
	uniform sampler2D diffuse;						\n\
	varying vec2 v_v2Tex;							\n\
	varying vec4 v_v4FillColor; 					\n\
													\n\
	void main()										\n\
	{												\n\
		//gl_FragColor = texture2D(diffuse, v_v2Tex);	\n\
		vec4 tval = vec4(0.0, 0.0, 0.0, 1.0); 		 \n\
		tval.xyz = texture2D(diffuse, v_v2Tex).xyz; \n\
		gl_FragColor = v_v4FillColor * tval; 		\n\
	}												 \n\
"
};

const char scale_rgba_vertex_shader[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec2 a_v2TexCoord;	\n\
	varying vec2 v_v2Tex;			\n\
									\n\
	void main()						\n\
	{								\n\
		v_v2Tex = a_v2TexCoord;		\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};

const char scale_rgba_frag_shader[] = {
"													\n\
	#version 100									\n\
													\n\
	precision mediump float;						\n\
	uniform sampler2D diffuse;						\n\
	varying vec2 v_v2Tex;							\n\
													\n\
	void main()										\n\
	{												\n\
		gl_FragColor = texture2D(diffuse, v_v2Tex);	\n\
	}												 \n\
"
};

const char yuvfilter_vertex_shader_uv[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec2 a_v2TexCoord;	\n\
	varying vec2 v_tex;				\n\
									\n\
	void main()						\n\
	{								\n\
		v_tex = a_v2TexCoord;		\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};
const char yuvfilter_vertex_shader_y[] = {
"									\n\
	#version 100					\n\
									\n\
	precision highp float;			\n\
									\n\
	uniform vec2 Tinvsize; //vec2(1/width, 1/height)			\n\
	attribute vec4 a_v4Position;	\n\
	attribute vec2 a_v2TexCoord;	\n\
	varying vec2 v_tex0;			\n\
	varying vec2 v_tex1;			\n\
	varying vec2 v_tex2;			\n\
	varying vec2 v_tex3;			\n\
	varying vec2 v_tex4;			\n\
									\n\
	void main()						\n\
	{								\n\
		vec2 offset;					\n\
		{							\n\
			offset.x = 0.5, offset.y = 0.5;								\n\
			v_tex0 = a_v2TexCoord + offset * Tinvsize;		\n\
		}							\n\
		{							\n\
			offset.x = 0.5, offset.y = 3.5;								\n\
			v_tex1 = a_v2TexCoord + offset * Tinvsize;		\n\
		}							\n\
		{							\n\
			offset.x = 2.0, offset.y = 2.0; 							\n\
			v_tex2 = a_v2TexCoord + offset * Tinvsize;		\n\
		}							\n\
		{							\n\
			offset.x = 3.5, offset.y = 0.5;								\n\
			v_tex3 = a_v2TexCoord + offset * Tinvsize;		\n\
		}							\n\
		{							\n\
			offset.x = 3.5, offset.y = 3.5;								\n\
			v_tex4 = a_v2TexCoord + offset * Tinvsize;		\n\
		}							\n\
		gl_Position = a_v4Position;	\n\
	}								\n\
"
};

const char yuvfilter_frag_shader_uv[] = {
"													\n\
	#version 100									\n\
													\n\
	precision mediump float;									\n\
	uniform sampler2D diffuse;						\n\
	varying vec2 v_tex;								\n\
													\n\
	#define toVec(x) x.b							\n\
	void main()										\n\
	{												\n\
		toVec(gl_FragColor) = (texture2D(diffuse, v_tex)).b;	\n\
	}												\n\
"
};

#if 1
const char yuvfilter_frag_shader_y[] = {
" 	#version 100											\n\
															\n\
precision mediump float;									\n\
															\n\
uniform sampler2D diffuse;									\n\
uniform float Tgain;										\n\
varying vec2 v_tex0; 										\n\
varying vec2 v_tex1;										\n\
varying vec2 v_tex2;										\n\
varying vec2 v_tex3;										\n\
varying vec2 v_tex4;										\n\
															\n\
// Input texture uniform sampler2D src; 					\n\
// Change these 2 defines to change precision				\n\
#define vec float											\n\
#define toVec(x) x.b										\n\
const float median_ts = 30.0 / 255.0;						\n\
void main() 												\n\
{															\n\
	vec v_sum = 0.0, diff, read_val_center, read_val; 		\n\
	read_val = toVec(texture2D(diffuse, v_tex0.xy )); \n\
	v_sum += read_val;	\n\
	read_val = toVec(texture2D(diffuse, v_tex1.xy)); \n\
	v_sum += read_val;	\n\
	read_val_center = toVec(texture2D(diffuse, v_tex2.xy)); \n\
	//v_sum += read_val_center;	\n\
	read_val = toVec(texture2D(diffuse, v_tex3.xy)); \n\
	v_sum += read_val;	\n\
	read_val = toVec(texture2D(diffuse, v_tex4.xy)); \n\
	v_sum += read_val;	\n\
						\n\
	vec result, med;					\n\
	med = v_sum * (1.0 / /*5.0*/4.0);	\n\
	diff = abs(med - read_val_center);	\n\
	diff = min(diff * Tgain, 1.0);	\n\
	result = mix(med, read_val_center, diff) ;			\n\
	toVec(gl_FragColor) = result;						\n\
}														\n\
"
};
#elif 0 //
const char yuvfilter_frag_shader_y[] = {
" 	#version 100											\n\
															\n\
//precision highp float;									\n\
precision mediump float;									\n\
															\n\
uniform sampler2D diffuse;									\n\
uniform vec2 Tinvsize; //vec2(1/width, 1/height)			\n\
varying vec2 v_tex; 										\n\
															\n\
// Input texture uniform sampler2D src; 					\n\
// Change these 2 defines to change precision				\n\
#define vec float											\n\
#define toVec(x) x.b										\n\
const float median_ts = 30.0 / 255.0;						\n\
void main() 												\n\
{															\n\
	vec  v_sum = 0.0, diff, read_val_center; 								\n\
	vec2 v_min = vec2( 1.0, 1.0), v_max = vec2(0.0, 0.0);	\n\
	float dX, dY;										\n\
	{													\n\
		dX = 0.5, dY = 0.5;									\n\
		vec2 offset = vec2(dX, dY);		\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 0.5, dY = 3.5;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 3.5, dY = 0.5;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 3.5, dY = 3.5;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 2.0, dY = 2.0;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		read_val_center = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		v_sum += read_val_center;	\n\
	}						\n\
								\n\
	vec result, med;					\n\
	med = v_sum * (1.0 / 5.0);					\n\
	diff = abs(med - read_val_center);	\n\
	diff = min(diff * 7.0, 1.0);	\n\
	/*if(diff >= median_ts)		\n\
		result = read_val_center; 	\n\
	else												\n\
		result = med*/;					\n\
	//result = (diff * read_val_center) + (1.0 - diff) * med;												\n\
	result = mix(med, read_val_center, diff) ;												\n\
	toVec(gl_FragColor) = result;						\n\
}														\n\
"
};
#elif 0
const char yuvfilter_frag_shader_y[] = {
" 	#version 100											\n\
															\n\
//precision highp float;									\n\
precision mediump float;									\n\
															\n\
uniform sampler2D diffuse;									\n\
uniform vec2 Tinvsize; //vec2(1/width, 1/height)			\n\
varying vec2 v_tex; 										\n\
															\n\
// Input texture uniform sampler2D src; 					\n\
// Change these 2 defines to change precision				\n\
#define vec float											\n\
#define toVec(x) x.b										\n\
const float median_ts = 30.0 / 255.0;						\n\
void main() 												\n\
{															\n\
	vec  v_sum = 0.0, diff; 								\n\
	vec2 v_min = vec2( 1.0, 1.0), v_max = vec2(0.0, 0.0);	\n\
	float dX, dY;										\n\
	{													\n\
		dX = 0.5, dY = 0.5;									\n\
		vec2 offset = vec2(dX, dY);		\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		if		(read_val < v_min.x) { v_min.y = v_min.x; v_min.x = read_val; }	\n\
		else if (read_val < v_min.y) { v_min.y = read_val; }					\n\
		if		(read_val > v_max.x) { v_max.y = v_max.x; v_max.x = read_val; }	\n\
		else if (read_val > v_max.y) { v_max.y = read_val; }					\n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 0.5, dY = 3.5;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		if		(read_val < v_min.x) { v_min.y = v_min.x; v_min.x = read_val; } \n\
		else if (read_val < v_min.y) { v_min.y = read_val; }					\n\
		if		(read_val > v_max.x) { v_max.y = v_max.x; v_max.x = read_val; } \n\
		else if (read_val > v_max.y) { v_max.y = read_val; }					\n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 3.5, dY = 0.5;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		if		(read_val < v_min.x) { v_min.y = v_min.x; v_min.x = read_val; } \n\
		else if (read_val < v_min.y) { v_min.y = read_val; }					\n\
		if		(read_val > v_max.x) { v_max.y = v_max.x; v_max.x = read_val; } \n\
		else if (read_val > v_max.y) { v_max.y = read_val; }					\n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 3.5, dY = 3.5;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		if		(read_val < v_min.x) { v_min.y = v_min.x; v_min.x = read_val; } \n\
		else if (read_val < v_min.y) { v_min.y = read_val; }					\n\
		if		(read_val > v_max.x) { v_max.y = v_max.x; v_max.x = read_val; } \n\
		else if (read_val > v_max.y) { v_max.y = read_val; }					\n\
		v_sum += read_val;	\n\
	}						\n\
	{													\n\
		dX = 2.0, dY = 2.0;									\n\
		vec2 offset = vec2(dX, dY); 	\n\
		vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
		if		(read_val < v_min.x) { v_min.y = v_min.x; v_min.x = read_val; } \n\
		else if (read_val < v_min.y) { v_min.y = read_val; }					\n\
		if		(read_val > v_max.x) { v_max.y = v_max.x; v_max.x = read_val; } \n\
		else if (read_val > v_max.y) { v_max.y = read_val; }					\n\
		v_sum += read_val;	\n\
	}						\n\
								\n\
	vec result;					\n\
	diff = (v_max.y - v_min.y);	\n\
	if(diff >= median_ts)		\n\
		result = toVec(texture2D(diffuse, v_tex.xy)); 	\n\
	else												\n\
		result = v_sum * (1.0 / 5.0);					\n\
														\n\
	toVec(gl_FragColor) = result;						\n\
}														\n\
"
};
#elif 0
const char yuvfilter_frag_shader_y[] = {
" 	#version 100											\n\
															\n\
//precision highp float;									\n\
precision mediump float;									\n\
															\n\
uniform sampler2D diffuse;									\n\
uniform vec2 Tinvsize; //vec2(1/width, 1/height)			\n\
varying vec2 v_tex; 										\n\
															\n\
// Input texture uniform sampler2D src; 					\n\
// Change these 2 defines to change precision				\n\
#define vec float											\n\
#define toVec(x) x.b										\n\
const float median_ts = 30.0 / 255.0;						\n\
void main() 												\n\
{															\n\
	vec  v_sum = 0.0, diff; 										\n\
	vec2 v_min = vec2( 1.0, 1.0), v_max = vec2(0.0, 0.0);	\n\
	for(int dX = -2; dX <= 2; ++dX) 						\n\
	{														\n\
		for(int dY = -2; dY <= 2; ++dY) 					\n\
		{													\n\
			vec2 offset = vec2(float(dX), float(dY));		\n\
			vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
																					\n\
			if		(read_val < v_min.x) { v_min.y = v_min.x; v_min.x = read_val; }	\n\
			else if (read_val < v_min.y) { v_min.y = read_val; }					\n\
			if		(read_val > v_max.x) { v_max.y = v_max.x; v_max.x = read_val; }	\n\
			else if (read_val > v_max.y) { v_max.y = read_val; }					\n\
			v_sum += read_val;	\n\
		}						\n\
	}							\n\
								\n\
	vec result;					\n\
	diff = (v_max.y - v_min.y);	\n\
	if(diff >= median_ts)		\n\
		result = toVec(texture2D(diffuse, v_tex.xy)); 	\n\
	else												\n\
		result = v_sum * (1.0 / 25.0);					\n\
														\n\
	toVec(gl_FragColor) = result;						\n\
}														\n\
"
};
#elif 0
const char yuvfilter_frag_shader_y[] = {
" 	#version 100											\n\
															\n\
//precision highp float;									\n\
precision mediump float;									\n\
															\n\
uniform sampler2D diffuse;									\n\
uniform vec2 Tinvsize; //vec2(1/width, 1/height)			\n\
varying vec2 v_tex; 										\n\
															\n\
// Input texture uniform sampler2D src; 					\n\
// Change these 2 defines to change precision				\n\
#define vec float											\n\
#define toVec(x) x.b										\n\
const float median_ts = 30.0 / 255.0;						\n\
void main() 												\n\
{															\n\
	vec  v_sum = 0.0, diff; 										\n\
	vec2 v_min = vec2( 1.0, 1.0), v_max = vec2(0.0, 0.0);	\n\
	for(int dX = -1; dX <= 1; ++dX) 						\n\
	{														\n\
		for(int dY = -1; dY <= 1; ++dY) 					\n\
		{													\n\
			vec2 offset = vec2(float(dX), float(dY));		\n\
			vec read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); \n\
																					\n\
			if		(read_val < v_min.x) { v_min.y = v_min.x; v_min.x = read_val; }	\n\
			else if (read_val < v_min.y) { v_min.y = read_val; }					\n\
			if		(read_val > v_max.x) { v_max.y = v_max.x; v_max.x = read_val; }	\n\
			else if (read_val > v_max.y) { v_max.y = read_val; }					\n\
			v_sum += read_val;	\n\
		}						\n\
	}							\n\
								\n\
	vec result;					\n\
	diff = (v_max.y - v_min.y);	\n\
	if(diff >= median_ts)		\n\
		result = toVec(texture2D(diffuse, v_tex.xy)); 	\n\
	else												\n\
		result = v_sum * (1.0 / 9.0);					\n\
														\n\
	toVec(gl_FragColor) = result;						\n\
}														\n\
"
};
#elif 0 /* 5x5 normal filter but 5sec */
const char yuvfilter_frag_shader_y[] = {
" 	#version 100																	\n\
																					\n\
	//precision highp float;															\n\
	precision mediump float;															\n\
																					\n\
	uniform sampler2D diffuse;														\n\
	uniform vec2 Tinvsize; //vec2(1/width, 1/height)								\n\
	varying vec2 v_tex;																\n\
																					\n\
	// Input texture uniform sampler2D src; 										\n\
	// Change these 2 defines to change precision 									\n\
	#define vec float 																\n\
	#define toVec(x) x.b 															\n\
	#define s2(a, b) temp = a; a = min(a, b); b = max(temp, b); 					\n\
	#define t2(a, b) s2(v[a], v[b]); 												\n\
	#define t24(a, b, c, d, e, f, g, h) t2(a, b); t2(c, d); t2(e, f); t2(g, h); 	\n\
	#define t25(a, b, c, d, e, f, g, h, i, j) t24(a, b, c, d, e, f, g, h); t2(i, j); \n\
	const float median_ts = 30.0 / 255.0;												\n\
	void main() 																		\n\
	{ 																					\n\
		vec v[25]; // Add the pixels which make up our window to the pixel array. 		\n\
		vec2 v_min = vec2(1.0, 1.0), v_max = vec2(0.0, 0.0);	\n\
		for(int dX = -2; dX <= 2; ++dX) 												\n\
		{ 																				\n\
			for(int dY = -2; dY <= 2; ++dY) 											\n\
			{ 																			\n\
				vec2 offset = vec2(float(dX), float(dY)); 								\n\
				vec read_val; 								\n\
				// If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the \n\
				// pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the  \n\
				// bottom right pixel of the window at pixel[N-1]. 															\n\
				read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize));  \n\
				v[(dX + 2) * 5 + (dY + 2)] = read_val; 	\n\
			} 												\n\
		} 													\n\
		vec temp, med; \n\
		t25(0, 1, 3, 4, 2, 4, 2, 3, 6, 7); 					\n\
		t25(5, 7, 5, 6, 9, 7, 1, 7, 1, 4); 					\n\
		t25(12, 13, 11, 13, 11, 12, 15, 16, 14, 16); 		\n\
		t25(14, 15, 18, 19, 17, 19, 17, 18, 21, 22); 		\n\
		t25(20, 22, 20, 21, 23, 24, 2, 5, 3, 6); 			\n\
		t25(0, 6, 0, 3, 4, 7, 1, 7, 1, 4);					\n\
		t25(11, 14, 8, 14, 8, 11, 12, 15, 9, 15); 			\n\
		t25(9, 12, 13, 16, 10, 16, 10, 13, 20, 23); 		\n\
		t25(17, 23, 17, 20, 21, 24, 18, 24, 18, 21); 		\n\
		t25(19, 22, 8, 17, 9, 18, 0, 18, 0, 9); 			\n\
		t25(10, 19, 1, 19, 1, 10, 11, 20, 2, 20); 			\n\
		t25(2, 11, 12, 21, 3, 21, 3, 12, 13, 22); 			\n\
		t25(4, 22, 4, 13, 14, 23, 5, 23, 5, 14); 			\n\
		t25(15, 24, 6, 24, 6, 15, 7, 16, 7, 19); 			\n\
		t25(3, 11, 5, 17, 11, 17, 9, 17, 4, 10); 			\n\
		t25(6, 12, 7, 14, 4, 6, 4, 7, 12, 14); 				\n\
		t25(10, 14, 6, 7, 10, 12, 6, 10, 6, 17); 			\n\
		t25(12, 17, 7, 17, 7, 10, 12, 18, 7, 12); 			\n\
		t24(10, 18, 12, 20, 10, 20, 10, 12); 				\n\
															\n\
		med = v[12];						\n\
		toVec(gl_FragColor) = med; 		\n\
	}										\n\
"
};
#elif 0 /* NR filter but 5sec */
const char yuvfilter_frag_shader_y[] = {
" 	#version 100																	\n\
																					\n\
	//precision highp float;															\n\
	precision mediump float;															\n\
																					\n\
	uniform sampler2D diffuse;														\n\
	uniform vec2 Tinvsize; //vec2(1/width, 1/height)								\n\
	varying vec2 v_tex;																\n\
																					\n\
	// Input texture uniform sampler2D src; 										\n\
	// Change these 2 defines to change precision 									\n\
	#define vec float 																\n\
	#define toVec(x) x.b 															\n\
	#define s2(a, b) temp = a; a = min(a, b); b = max(temp, b); 					\n\
	#define t2(a, b) s2(v[a], v[b]); 												\n\
	#define t24(a, b, c, d, e, f, g, h) t2(a, b); t2(c, d); t2(e, f); t2(g, h); 	\n\
	#define t25(a, b, c, d, e, f, g, h, i, j) t24(a, b, c, d, e, f, g, h); t2(i, j); \n\
	const float median_ts = 30.0 / 255.0;												\n\
	void main() 																		\n\
	{ 																					\n\
		vec v[25]; // Add the pixels which make up our window to the pixel array. 		\n\
		vec2 v_min = vec2(1.0, 1.0), v_max = vec2(0.0, 0.0);	\n\
		for(int dX = -2; dX <= 2; ++dX) 												\n\
		{ 																				\n\
			for(int dY = -2; dY <= 2; ++dY) 											\n\
			{ 																			\n\
				vec2 offset = vec2(float(dX), float(dY)); 								\n\
				vec read_val; 								\n\
				bool min_proc_pos_direction = true, max_proc_pos_direction = true; 	\n\
				// If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the \n\
				// pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the  \n\
				// bottom right pixel of the window at pixel[N-1]. 															\n\
				read_val = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize));  \n\
															  \n\
				if (min_proc_pos_direction)					  \n\
				{											  \n\
					if (read_val < v_min.x)		  \n\
						v_min.x = read_val;		  \n\
					else if (read_val < v_min.y)	  \n\
						v_min.y = read_val;		  \n\
				}											  \n\
				else										  \n\
				{											  \n\
					if (read_val < v_min.y)		  \n\
						v_min.y = read_val;		  \n\
					else if (read_val < v_min.x)	  \n\
						v_min.x = read_val;		  \n\
				}											  \n\
				if (v_min.x > v_min.y)					  \n\
					min_proc_pos_direction = true;				  \n\
				else										  \n\
					min_proc_pos_direction = false;				  \n\
															  \n\
				if (max_proc_pos_direction)					  \n\
				{											  \n\
					if (read_val > v_max.x)		  \n\
						v_max.x = read_val;		  \n\
					else if (read_val > v_max.y)	  \n\
						v_max.y = read_val;		  \n\
				}											  \n\
				else										  \n\
				{											  \n\
					if (read_val > v_max.y)		  \n\
						v_max.y = read_val;		  \n\
					else if (read_val > v_max.x)	  \n\
						v_max.x = read_val;		  \n\
				}											  \n\
				if (v_max.x < v_max.y)					  \n\
					max_proc_pos_direction = true;				  \n\
				else										  \n\
					max_proc_pos_direction = false;				  \n\
															  \n\
				v[(dX + 2) * 5 + (dY + 2)] = read_val; 	\n\
			} 												\n\
		} 													\n\
		vec temp, temp_min, temp_max, result, med, diff, center; \n\
		center = v[12];										\n\
		t25(0, 1, 3, 4, 2, 4, 2, 3, 6, 7); 					\n\
		t25(5, 7, 5, 6, 9, 7, 1, 7, 1, 4); 					\n\
		t25(12, 13, 11, 13, 11, 12, 15, 16, 14, 16); 		\n\
		t25(14, 15, 18, 19, 17, 19, 17, 18, 21, 22); 		\n\
		t25(20, 22, 20, 21, 23, 24, 2, 5, 3, 6); 			\n\
		t25(0, 6, 0, 3, 4, 7, 1, 7, 1, 4);					\n\
		t25(11, 14, 8, 14, 8, 11, 12, 15, 9, 15); 			\n\
		t25(9, 12, 13, 16, 10, 16, 10, 13, 20, 23); 		\n\
		t25(17, 23, 17, 20, 21, 24, 18, 24, 18, 21); 		\n\
		t25(19, 22, 8, 17, 9, 18, 0, 18, 0, 9); 			\n\
		t25(10, 19, 1, 19, 1, 10, 11, 20, 2, 20); 			\n\
		t25(2, 11, 12, 21, 3, 21, 3, 12, 13, 22); 			\n\
		t25(4, 22, 4, 13, 14, 23, 5, 23, 5, 14); 			\n\
		t25(15, 24, 6, 24, 6, 15, 7, 16, 7, 19); 			\n\
		t25(3, 11, 5, 17, 11, 17, 9, 17, 4, 10); 			\n\
		t25(6, 12, 7, 14, 4, 6, 4, 7, 12, 14); 				\n\
		t25(10, 14, 6, 7, 10, 12, 6, 10, 6, 17); 			\n\
		t25(12, 17, 7, 17, 7, 10, 12, 18, 7, 12); 			\n\
		t24(10, 18, 12, 20, 10, 20, 10, 12); 				\n\
															\n\
		temp_max = min(v_max.x, v_max.y);	\n\
		temp_min = max(v_min.x, v_min.y);	\n\
		med = v[12];						\n\
		diff = abs(temp_max - temp_min);		\n\
		if(diff >= median_ts) 				\n\
			result = center;			\n\
		else								\n\
			result = med;					\n\
											\n\
		toVec(gl_FragColor) = med;//result; 		\n\
	}										\n\
"
};
#elif 0 /* 3x3 filter */
const char yuvfilter_frag_shader_y[] = {
"													\n\
 	#version 100																	\n\
																					\n\
	//precision highp float;															\n\
	precision mediump float;															\n\
																					\n\
	uniform sampler2D diffuse;														\n\
	uniform vec2 Tinvsize; //vec2(1/width, 1/height)								\n\
	varying vec2 v_tex;																\n\
																				\n\
	// Change these 2 defines to change precision, 									\n\
	#define vec float																\n\
	#define toVec(x) x.b															\n\
	#define s2(a, b) temp = a; a = min(a, b); b = max(temp, b); \n\
	#define mn3(a, b, c) s2(a, b); s2(a, c); 					\n\
	#define mx3(a, b, c) s2(b, c); s2(a, c); 					\n\
	#define mnmx3(a, b, c) mx3(a, b, c); s2(a, b); // 3 exchanges \n\
	#define mnmx4(a, b, c, d) s2(a, b); s2(c, d); s2(a, c); s2(b, d); // 4 exchanges \n\
	#define mnmx5(a, b, c, d, e) s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e); // 6 exchanges \n\
	#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges \n\
	void main() 	\n\
	{ 				\n\
		vec v[9]; 	\n\
		// Add the pixels which make up our window to the pixel array. \n\
		for(int dX = -1; dX <= 1; ++dX) \n\
		{ 								\n\
			for(int dY = -1; dY <= 1; ++dY) \n\ 
			{ 								\n\
				vec2 offset = vec2(float(dX), float(dY)); \n\
				// If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the  \n\
				// pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the  	\n\
				// bottom right pixel of the window at pixel[N-1]. 															\n\
				v[(dX + 1) * 3 + (dY + 1)] = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize)); 					\n\
			} 																												\n\
		} 																													\n\
		vec temp; // Starting with a subset of size 6, remove the min and max each time 									\n\
		mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]); \n\
		mnmx5(v[1], v[2], v[3], v[4], v[6]); 		\n\
		mnmx4(v[2], v[3], v[4], v[7]); 			\n\
		mnmx3(v[3], v[4], v[8]); 				\n\
		toVec(gl_FragColor) = v[4];  			\n\
	}										\n\
"
};
#elif 0 /* simple */
const char yuvfilter_frag_shader_y[] = {
"													\n\
	#version 100									\n\
													\n\
	precision highp float;							\n\
	uniform sampler2D diffuse;						\n\
	uniform vec2 Tinvsize; 			 				\n\
	varying vec2 v_tex;								\n\
													\n\
	#define toVec(x) x.b							\n\
	void main()										\n\
	{												\n\
		toVec(gl_FragColor) = (texture2D(diffuse, v_tex)).b;	\n\
	}												\n\
"
};
#elif 1 /* simple */
const char yuvfilter_frag_shader_y[] = {
"													\n\
	#version 100									\n\
													\n\
	precision highp float;							\n\
	uniform sampler2D diffuse;						\n\
	uniform vec2 Tinvsize;							\n\ 
	varying vec2 v_tex; 							\n\
													\n\
	#define toVec(x) x.b							\n\
	void main() 									\n\
	{												\n\
		vec4 color = vec4(0.0, 0.0, (texture2D(diffuse, v_tex)).b, 0.2);	\n\
		gl_FragColor.ba = color.ba;					\n\
	}												\n\
"
};
#elif 0 /* not working */
	const char yuvfilter_frag_shader_y[] = {
	"	#version 100																	\n\
																						\n\
		//precision highp float;															\n\
		precision mediump float;														\n\
																						\n\
		uniform sampler2D diffuse;														\n\
		uniform vec2 Tinvsize; //vec2(1/width, 1/height)								\n\
		varying vec2 v_tex; 															\n\
																						\n\
		// Input texture uniform sampler2D src; 										\n\
		// Change these 2 defines to change precision									\n\
	#define vec float 																\n\
	#define toVec(x) x.b 															\n\
	#define s2(a, b) temp = a; a = min(a, b); b = max(temp, b); 					\n\
	#define t2(a, b) s2(v[a], v[b]); 												\n\
	#define t24(a, b, c, d, e, f, g, h) t2(a, b); t2(c, d); t2(e, f); t2(g, h); 	\n\
	#define t25(a, b, c, d, e, f, g, h, i, j) t24(a, b, c, d, e, f, g, h); t2(i, j); \n\
		const float median_ts = 30.0;												\n\
		const int LEN = 25; 														\n\
		vec sorted_v[25];												\n\
		vec src_v[25]; // Add the pixels which make up our window to the pixel array.		\n\ 
																		\n\
																		\n\
		void mergesort (int num)										\n\
		{																\n\
			int rght, wid, rend;										\n\
			int i,j,m,t;												\n\
																		\n\
			for (int k=1; k < num; k *= 2 ) {							\n\
				for (int left=0; left+k < num; left += k*2 ) {			\n\
					rght = left + k;									\n\
					rend = rght + k;									\n\
					if (rend > num) rend = num; 						\n\
					m = left; i = left; j = rght;						\n\
					while (i < rght && j < rend) {						\n\
						if (src_v[i] <= src_v[j]) { 					\n\
							sorted_v[m] = src_v[i]; i++;				\n\
						} else {										\n\
							sorted_v[m] = src_v[j]; j++;				\n\
						}												\n\
						m++;											\n\
					}													\n\
					while (i < rght) {									\n\
						sorted_v[m]=src_v[i];							\n\
						i++; m++;										\n\
					}													\n\
					while (j < rend) {									\n\
						sorted_v[m]=src_v[j];							\n\
						j++; m++;										\n\
					}													\n\
					for (m=left; m < rend; m++) {						\n\
						src_v[m] = sorted_v[m]; 						\n\
					}													\n\
				}														\n\
			}															\n\
		}																\n\
																		\n\
		void main() 																		\n\
		{																					\n\
			for(int dX = -2; dX <= 2; ++dX) 												\n\
			{																				\n\
				for(int dY = -2; dY <= 2; ++dY) 											\n\
				{																			\n\
					vec2 offset = vec2(float(dX), float(dY));								\n\
					// If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the \n\
					// pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the  \n\
					// bottom right pixel of the window at pixel[N-1].															\n\
					src_v[(dX + 2) * 5 + (dY + 2)] = toVec(texture2D(diffuse, v_tex.xy + offset * Tinvsize));  \n\
				}												\n\
			}													\n\
			vec temp, temp_min, temp_max, result, med, diff, center;				\n\
			center = src_v[12]; 									\n\
																\n\
			mergesort(25);										\n\
																\n\
			med = sorted_v[12]; 								\n\
			diff = (sorted_v[23] - sorted_v[1]);				\n\
			if(diff >= median_ts)								\n\
				result = 1.0; //center; 						\n\
			else												\n\
				result = med;									\n\
																\n\
			toVec(gl_FragColor) = result;						\n\
		}														\n\
	"
	};

#endif

#if 0
{
	/* 3x3 Median GLSL 1.0 Morgan McGuire and Kyle Whitson, 2006 Williams College http://graphics.cs.williams.edu Copyright (c) Morgan McGuire and Williams College, 2006 All rights reserved. Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */ 
	// Input texture uniform sampler2D src; 
	// Change these 2 defines to change precision 
	#define vec vec3 
	#define toVec(x) x.rgb 
	#define s2(a, b) temp = a; a = min(a, b); b = max(temp, b); 
	#define t2(a, b) s2(v[a], v[b]); 
	#define t24(a, b, c, d, e, f, g, h) t2(a, b); t2(c, d); t2(e, f); t2(g, h); 
	#define t25(a, b, c, d, e, f, g, h, i, j) t24(a, b, c, d, e, f, g, h); t2(i, j); 
	void main() 
	{ 
		vec v[25]; // Add the pixels which make up our window to the pixel array. 
		for(int dX = -2; dX <= 2; ++dX) 
		{ 
			for(int dY = -2; dY <= 2; ++dY) 
			{ 
				vec2 offset = vec2(float(dX), float(dY)); 
				// If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the 
				// pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the 
				// bottom right pixel of the window at pixel[N-1]. 
				v[(dX + 2) * 5 + (dY + 2)] = toVec(texture2D(src, gl_TexCoord[0].xy + offset * g3d_sampler2DInvSize(src))); 
			} 
		} 
		vec temp; 
		t25(0, 1, 3, 4, 2, 4, 2, 3, 6, 7); 
		t25(5, 7, 5, 6, 9, 7, 1, 7, 1, 4); 
		t25(12, 13, 11, 13, 11, 12, 15, 16, 14, 16); 
		t25(14, 15, 18, 19, 17, 19, 17, 18, 21, 22); 
		t25(20, 22, 20, 21, 23, 24, 2, 5, 3, 6); 
		t25(0, 6, 0, 3, 4, 7, 1, 7, 1, 4);
		t25(11, 14, 8, 14, 8, 11, 12, 15, 9, 15); 
		t25(9, 12, 13, 16, 10, 16, 10, 13, 20, 23); 
		t25(17, 23, 17, 20, 21, 24, 18, 24, 18, 21); 
		t25(19, 22, 8, 17, 9, 18, 0, 18, 0, 9); 
		t25(10, 19, 1, 19, 1, 10, 11, 20, 2, 20); 
		t25(2, 11, 12, 21, 3, 21, 3, 12, 13, 22); 
		t25(4, 22, 4, 13, 14, 23, 5, 23, 5, 14); 
		t25(15, 24, 6, 24, 6, 15, 7, 16, 7, 19); 
		t25(3, 11, 5, 17, 11, 17, 9, 17, 4, 10); 
		t25(6, 12, 7, 14, 4, 6, 4, 7, 12, 14); 
		t25(10, 14, 6, 7, 10, 12, 6, 10, 6, 17); 
		t25(12, 17, 7, 17, 7, 10, 12, 18, 7, 12); 
		t24(10, 18, 12, 20, 10, 20, 10, 12); 
		toVec(gl_FragColor) = v[12]; 
	} 
#endif

#if 0
/* 3x3 Median Morgan McGuire and Kyle Whitson http://graphics.cs.williams.edu Copyright (c) Morgan McGuire and Williams College, 2006 All rights reserved. Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */ 
// Input texture 
uniform sampler2D T; 
#ifndef G3D 
// vec2(1/width, 1/height) of the texture 
uniform vec2 Tinvsize; 
#else 
# define Tinvsize g3d_sampler2DInvSize(T)) 
#endif 
// Change these 2 defines to change precision, 
#define vec vec3 
#define toVec(x) x.rgb 
//#define vec vec4 
//#define toVec(x) x.rgba 
#define s2(a, b) temp = a; a = min(a, b); b = max(temp, b); 
#define mn3(a, b, c) s2(a, b); s2(a, c); 
#define mx3(a, b, c) s2(b, c); s2(a, c); 
#define mnmx3(a, b, c) mx3(a, b, c); s2(a, b); // 3 exchanges 
#define mnmx4(a, b, c, d) s2(a, b); s2(c, d); s2(a, c); s2(b, d); // 4 exchanges 
#define mnmx5(a, b, c, d, e) s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e); // 6 exchanges 
#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges 
void main() 
{ 
	vec v[9]; 
	// Add the pixels which make up our window to the pixel array. 
	for(int dX = -1; dX <= 1; ++dX) 
	{ 
		for(int dY = -1; dY <= 1; ++dY) 
		{ 
			vec2 offset = vec2(float(dX), float(dY)); 
			// If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the 
			// pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the 
			// bottom right pixel of the window at pixel[N-1]. 
			v[(dX + 1) * 3 + (dY + 1)] = toVec(texture2D(T, gl_TexCoord[0].xy + offset * Tinvsize)); 
		} 
	} 
	vec temp; // Starting with a subset of size 6, remove the min and max each time 
	mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]); 
	mnmx5(v[1], v[2], v[3], v[4], v[6]); 
	mnmx4(v[2], v[3], v[4], v[7]); 
	mnmx3(v[3], v[4], v[8]); 
	toVec(gl_FragColor) = v[4]; 
} 
#endif
