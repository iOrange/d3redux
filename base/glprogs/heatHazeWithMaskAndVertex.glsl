struct Vertex2Fragment {
    vec2 uv0;   // model surface texture coords unmodified for the mask
    vec2 uv1;   // model surface texture coords with a scroll
    vec2 uv2;   // copied deform magnitude
};

#define saturate(v) clamp((v), 0.0f, 1.0f)

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

uniform mat4 gMVP;
uniform mat4 gModelView;
uniform mat4 gProjection;

// 0 -  scroll
// 1 -  deform magnitude (1.0 is reasonable, 2.0 is twice as wavy, 0.5 is half as wavy, etc)
uniform vec4 gVParams[2];

// 0 - is the
// 1 - is the
out Vertex2Fragment v2f;

#define dot4_row(v, m, r) dot((v), vec4((m)[0][(r)], (m)[1][(r)], (m)[2][(r)], (m)[3][(r)]))

void main() {
    vec4 vPos = vec4(inPos, 1.0f);

    // uv0 takes the texture coordinates unmodified
    v2f.uv0 = inUV;

    // uv1 takes the texture coordinates and adds a scroll
    v2f.uv1 = inUV + gVParams[0].xy;

    // uv2 takes the deform magnitude and scales it by the projection distance
    vec4 R0 = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    R0.z = dot4_row(vPos, gModelView, 2);

    float R1 = dot4_row(R0, gProjection, 0);
    float R2 = dot4_row(R0, gProjection, 3);

    // don't let the recip get near zero for polygons that cross the view plane
    R2 = max(R2, 1.0f);
    R1 /= R2;

    // clamp the distance so the the deformations don't get too wacky near the view
    R1 = min(R1, 0.02f);

    v2f.uv2 = gVParams[1].xy * R1;

    gl_Position = gMVP * vPos;
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER

// texture 0 is _currentRender
// texture 1 is a normal map that we will use to deform texture 0
// texture 2 is a mask texture
uniform sampler2D   gTextures[3];

// 0 - is the 1.0 to _currentRender conversion
// 1 - is the fragment.position to 0.0 - 1.0 conversion
uniform vec4        gFParams[2];

in Vertex2Fragment v2f;

out vec4 oRT0;

void main() {
    // load the distortion map
    vec4 mask = texture(gTextures[2], v2f.uv0);

    // kill the pixel if the distortion wound up being very small
    mask.xy -= 0.01f;
    if (any(lessThan(mask.xy, vec2(0.0f)))) {
        discard;
    }

    // load the filtered normal map and convert to -1 to 1 range
    vec3 localNormal = texture(gTextures[1], v2f.uv1).wyz;  // normalmap is RXGB compressed (R and A swapped)
    localNormal = normalize(localNormal * 2.0f - 1.0f);

    localNormal *= mask.xyz;

    // calculate the screen texcoord in the 0.0 to 1.0 range
    vec4 R0 = gl_FragCoord * gFParams[1];

    // offset by the scaled localNormal and clamp it to 0.0 - 1.0
    vec2 rtUV = saturate(localNormal.xy * v2f.uv2 + R0.xy);

    // scale by the screen non-power-of-two-adjust
    rtUV *= gFParams[0].xy;

    // load the screen render
    vec4 color = texture(gTextures[0], rtUV);
    oRT0 = color;
}

#endif // FRAGMENT_SHADER
