#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

layout (binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout (location = 0) in VS_IN {
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vIn[];

layout (location = 0) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

const float MAGNITUDE = 0.15;

void FinVertex(vec3 pos, vec3 norm, vec3 tangent, vec2 texCoord) {
    vOut.wPos     = pos;
    vOut.wNorm    = norm;
    vOut.wTangent = tangent;
    vOut.texCoord = texCoord;
    EmitVertex();
}

mat3 rotation(float time) {
    return mat3(
    0.f,        1.f, 0.f,
    cos(time),  0.f, sin(time),
    -sin(time), 0.f, cos(time)
    );
}


void main(void)
{
    gl_Position = params.mProjView * vec4(vIn[0].wPos, 1.0);
    FinVertex(vIn[0].wPos, vIn[0].wNorm, vIn[0].wTangent, vIn[0].texCoord);

    gl_Position = params.mProjView * vec4(vIn[1].wPos, 1.0);
    FinVertex(vIn[1].wPos, vIn[1].wNorm, vIn[1].wTangent, vIn[1].texCoord);

    vec3 rot = 0.5 * vIn[1].wNorm  +  0.2 * vIn[1].wTangent * rotation(4.0 * Params.time);
    gl_Position = params.mProjView * (vec4(vIn[1].wPos, 1.0) + vec4(rot * MAGNITUDE, 0.0));
    EmitVertex();

    gl_Position = params.mProjView * vec4(vIn[1].wPos, 1.0);
    FinVertex(vIn[1].wPos, vIn[1].wNorm, vIn[1].wTangent, vIn[1].texCoord);

    gl_Position = params.mProjView * vec4(vIn[2].wPos, 1.0);
    FinVertex(vIn[2].wPos, vIn[2].wNorm, vIn[2].wTangent, vIn[2].texCoord);

    gl_Position = params.mProjView * vec4(vIn[0].wPos, 1.0);
    FinVertex(vIn[0].wPos, vIn[0].wNorm, vIn[0].wTangent, vIn[0].texCoord);

    EndPrimitive();
}