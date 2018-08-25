#ifdef VERTEX_SHADER

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

uniform mat4 gMVP;

out vec2 v2fUV;

void main() {
    v2fUV = inUV;
    gl_Position = gMVP * vec4(inPos, 1.0f);
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER

uniform sampler2D   gTexDiffuse;

uniform vec4        gDiffuseModifier;

in vec2 v2fUV;

out vec4 oRT0;

void main() {
    vec4 color = texture(gTexDiffuse, v2fUV);
    oRT0 = color * gDiffuseModifier;
}

#endif // FRAGMENT_SHADER
