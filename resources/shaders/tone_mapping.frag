#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout (binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D hdrImage;

const float whiteLevel = 2.0;
const float greyLevel = 0.5;


//// http://steps3d.narod.ru/tutorials/hdr-tutorial.html + code from lection

vec4 reinhard_tone_mapping(vec4 base)
{
    float lum = max(dot(base.rgb, vec3(0.299, 0.587, 0.114)), 0.0001);
    float scale = lum / (1.0 + lum);

    base = base * scale / lum;
    return vec4(base.rgb, 1.0);
}

vec4 reinhard_mod_tone_mapping(vec4 base)
{
    float avgLum = exp(dot(base.rgb, vec3(0.299, 0.587, 0.114)));

    base = base * (greyLevel / avgLum);
    return vec4(base.rgb, 1.0);
}

vec4 squared_tone_mapping(vec4 base)
{
    float avgLum = sqrt(0.0001 + dot(base.rgb, vec3(0.299, 0.587, 0.114)));

    base = base * (greyLevel / avgLum);
    base = base / (vec4(1.0) + base);

    return vec4(base.rgb, 1.0);
}

vec4 exponential_tone_mapping(vec4 base)
{
    float lum = max(dot(base.rgb, vec3(0.299, 0.587, 0.114)), 0.0001);

    float mappedLum = 1 - exp(-lum / whiteLevel);
    base = base * (mappedLum / lum);

    return vec4(base.rgb, 1.0);
}


void main()
{
    const vec4 hdrColor = texture(hdrImage, vOut.texCoord);

    switch(Params.toneMode)
    {
        case 0: // None
            out_fragColor = clamp(hdrColor, vec4(0.0f), vec4(1.0f));
            break;

        case 1: // Reinhard
            out_fragColor = reinhard_tone_mapping(hdrColor);
            break;

        case 2: // Reinhard
            out_fragColor = reinhard_mod_tone_mapping(hdrColor);
            break;

        case 3: // Squared
            out_fragColor = squared_tone_mapping(hdrColor);
            break;

        case 4: // Exp
            out_fragColor = exponential_tone_mapping(hdrColor);
            break;
    }
}