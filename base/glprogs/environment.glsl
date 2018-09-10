struct Vertex2Fragment {
    vec3 Normal;
    vec3 ToEye;
    vec4 Color;
};

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 inPos;
layout(location = 2) in vec3 inNormal;
layout(location = 5) in vec4 inColor;

uniform mat4 gMVP;

uniform vec4 gViewOrigin;

uniform vec4 gColorMod;
uniform vec4 gColorAdd;

out Vertex2Fragment v2f;

void main() {
    v2f.Normal = inNormal;
    v2f.ToEye = gViewOrigin.xyz - inPos;
    v2f.Color = inColor * gColorMod + gColorAdd;

    gl_Position = gMVP * vec4(inPos, 1.0f);
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER

uniform samplerCube gTexCubeMap;

in Vertex2Fragment v2f;

out vec4 oRT0;

void main() {
    vec3 normal = normalize(v2f.Normal);
    vec3 toEye = normalize(v2f.ToEye);
    vec3 reflection = reflect(-toEye, normal);

    oRT0 = texture(gTexCubeMap, reflection) * v2f.Color;
}

#endif // FRAGMENT_SHADER
