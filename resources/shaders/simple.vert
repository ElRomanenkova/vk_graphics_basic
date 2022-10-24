#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "unpack_attributes.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

mat4 rotationL(float time) {
    return mat4(
    cos(time), 0.f, sin(time),  0.f,
    0.f,       1.f, 0.f,        0.f,
    -sin(time), 0.f, cos(time), 0.f,
    0.f,       0.f, 0.f,        1.f
    );
}

mat4 rotationR(float time) {
    return mat4(
    cos(time), 0.f, -sin(time), 0.f,
    0.f,       1.f, 0.f,        0.f,
    sin(time), 0.f, cos(time),  0.f,
    0.f,       0.f, 0.f,        1.f
    );
}

mat4 fluctuationY(float time) {
    float pos = 0.3f * abs(sin(2.5f * time));

    return mat4(
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, pos, 0.f, 1.f
    );
}

mat4 fluctuationX(float time) {
    float pos = 0.1f * abs(sin(2.5f * time));

    return mat4(
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    pos, 0.f, 0.f, 1.f
    );
}

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint type;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

out gl_PerVertex { vec4 gl_Position; };


void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    mat4 mModel = params.mModel;

    switch (params.type)
    {
//        case 0: //scene
//        mModel *= rotationL(Params.time);
//        break;

        case 1: //teapot
        mModel *= fluctuationY(Params.time);
        break;

        case 2: //box
        mModel *= rotationR(Params.time);
        break;

        case 3: //cylinder
        mModel *= rotationR(Params.time);
        break;

        case 4: //corner
        mModel *= fluctuationX(Params.time);
        break;

        case 5: //disco ball
        mModel *= fluctuationY(Params.time);
        break;
    }

    vOut.wPos     = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(mModel))) * wTang.xyz);

    vOut.texCoord = vTexCoordAndTang.xy;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
