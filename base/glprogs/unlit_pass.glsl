struct Vertex2Fragment {
    vec2 UV;
    vec4 Color;
};

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
//layout(location = 2) in vec3 inNormal;
//layout(location = 3) in vec3 inTangent;
//layout(location = 4) in vec3 inBinormal;
layout(location = 5) in vec4 inColor;

uniform mat4 gMVP;

uniform vec4 gColorMod;
uniform vec4 gColorAdd;

out Vertex2Fragment v2f;

void main() {
    v2f.UV = inUV;
    v2f.Color = inColor * gColorMod + gColorAdd;

    gl_Position = gMVP * vec4(inPos, 1.0f);
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER

uniform sampler2D gTexDiffuse;

in Vertex2Fragment v2f;

out vec4 oRT0;

void main() {
    oRT0 = texture(gTexDiffuse, v2f.UV) * v2f.Color;
}

#endif // FRAGMENT_SHADER
