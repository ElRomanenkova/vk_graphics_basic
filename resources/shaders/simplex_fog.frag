#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint quadResolution;
    float minHeight;
    float maxHeight;
} params;

layout(location = 0) out vec4 color;

layout (location = 0 ) in VS_OUT
{
    vec3 wPos;
    vec3 oPos;
} surface;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1, set = 0) uniform NoiseData
{
  NoiseParams Noise;
};

//	Simplex 3D Noise
//	by Ian McEwan, Ashima Arts
//
//  Was taken from https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
//
vec4 permute(vec4 x)
{
    return mod(((x * 34.0)+1.0) * x, 289.0);
}
vec4 taylorInvSqrt(vec4 r) {return 1.79284291400159 - 0.85373472095314 * r;}

float snoise(vec3 v) {
  const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
  const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

// First corner
  vec3 i  = floor(v + dot(v, C.yyy) );
  vec3 x0 =   v - i + dot(i, C.xxx) ;

// Other corners
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min( g.xyz, l.zxy );
  vec3 i2 = max( g.xyz, l.zxy );

  //  x0 = x0 - 0. + 0.0 * C
  vec3 x1 = x0 - i1 + 1.0 * C.xxx;
  vec3 x2 = x0 - i2 + 2.0 * C.xxx;
  vec3 x3 = x0 - 1. + 3.0 * C.xxx;

// Permutations
  i = mod(i, 289.0 );
  vec4 p = permute( permute( permute(
             i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
           + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

// Gradients
// ( N*N points uniformly over a square, mapped onto an octahedron.)
  float n_ = 1.0/7.0; // N=7
  vec3  ns = n_ * D.wyz - D.xzx;

  vec4 j = p - 49.0 * floor(p * ns.z *ns.z);  //  mod(p,N*N)

  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);

  vec4 b0 = vec4( x.xy, y.xy );
  vec4 b1 = vec4( x.zw, y.zw );

  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));

  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

  vec3 p0 = vec3(a0.xy,h.x);
  vec3 p1 = vec3(a0.zw,h.y);
  vec3 p2 = vec3(a1.xy,h.z);
  vec3 p3 = vec3(a1.zw,h.w);

//Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;

// Mix final noise value
  vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m * m;
  return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1),
                                dot(p2,x2), dot(p3,x3) ) );
}

vec2 boxIntersection(vec3 ro, vec3 rd, vec3 boxSize)
{
    vec3 m = 1.0 / rd;
    vec3 n = m * ro;
    vec3 k = abs(m) * boxSize;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    float tN = max(max(t1.x, t1.y), t1.z);
    float tF = min(min(t2.x, t2.y), t2.z);

    if(tN > tF || tF < 0.0)
        return vec2(-1.0, -1.0);

    return vec2(tN, tF);
}

float dSphere(vec3 query_position, vec3 position, float radius)
{
    return length(query_position - position) - radius;
}

float minUnion(float d1, float d2)
{
    return min(d1, d2);
}

float sdfBalls(vec3 q)
{
    vec3 qSL = q + vec3( 4* abs(cos(Params.time)), 2.6*abs(sin(2. * Params.time)), 0.0);
    vec3 qSR = q + vec3(-4* abs(cos(Params.time)), 2.6*abs(sin(2. * Params.time)), 0.0);

    float sphL = dSphere(qSL, vec3 ( 2, 0, 13), abs(cos(Params.time)));
    float sphR = dSphere(qSR, vec3 (-2, 0, 13), abs(cos(Params.time)));

    return minUnion(sphL, sphR);
}

float sdfTorus(vec3 p, vec2 t, float waveScale)
{
	vec2	q = vec2(length(p.xy) - t.x, p.z);

	return length(q) - t.y + waveScale * sin(20.0 * length(p.xy) + Params.time);
}

float fogSDF(vec3 queryPos)
{
    float balls = sdfBalls(queryPos);
    float torus = sdfTorus(queryPos + vec3(0, 2, -10), vec2(1.5, 0.5), 0.4);

    return minUnion(balls, torus);
}

float getDensity(vec3 position)
{
  if (fogSDF(position) < 0)
    return snoise(vec3(Noise.noiseScale.x * position.x + 5. * sin(0.1 * Params.time),
                       Noise.noiseScale.y * position.y + 0.3 * sin(0.5 * Params.time),
                       Noise.noiseScale.z * position.z + 5. * cos(0.1 * Params.time)));
  else
    return 0;
}

void main()
{
    vec3 rayDirection = normalize((inverse(params.mModel) * vec4(normalize(Params.wCameraPos - surface.wPos), 0.0)).xyz);
    vec3 rayOrigin = surface.oPos - rayDirection;

    vec3 boxSize = vec3(Noise.boxSize);

    vec2 hits = boxIntersection(rayOrigin, rayDirection, boxSize);

    vec3 exit = rayOrigin  + hits.x * rayDirection;
    vec3 entry = rayOrigin  + hits.y * rayDirection;

    const int STEPS = 100 * int((Noise.boxSize.x + Noise.boxSize.y + Noise.boxSize.z) / 3.f);
    float stepSize = length(exit - entry) / STEPS;

    float transmittance = 1.0;
    vec3 point = entry;

    for (int i = 0; i < STEPS; ++i)
    {
        point -= rayDirection  * stepSize;
        float density = getDensity(point);

        if (density > 0)
        {
            transmittance *= exp(-density * stepSize * Noise.extinctionCoef);

            if (transmittance < 0.01)
              break;
        }
    }

    float color_value = mix(1, 0, transmittance);
    color = vec4(color_value, color_value, color_value, 1 - transmittance);
}
