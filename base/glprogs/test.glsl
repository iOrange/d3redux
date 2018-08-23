struct Vertex2Fragment {
    vec3 ToLight;
    vec3 ToViewer;
    vec2 TC1;
    vec2 TC2;
    vec3 TC3;
    vec2 TC4;
    vec2 TC5;
    vec4 Color;
};

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBinormal;
layout(location = 5) in vec4 inColor;

uniform mat4 gMVP;
uniform mat4 gModel;

uniform vec4 gLightOrigin;
uniform vec4 gViewOrigin;
uniform vec4 gLightProject_S;
uniform vec4 gLightProject_T;
uniform vec4 gLightProject_Q;
uniform vec4 gLightFalloff_S;
uniform vec4 gBumpMatrix_S;
uniform vec4 gBumpMatrix_T;
uniform vec4 gDiffuseMatrix_S;
uniform vec4 gDiffuseMatrix_T;
uniform vec4 gSpecularMatrix_S;
uniform vec4 gSpecularMatrix_T;
uniform vec4 gColorMod;
uniform vec4 gColorAdd;

out Vertex2Fragment v2f;

void main() {
    vec4 vPos = vec4(inPos, 1.0);

    vec3 toLight = gLightOrigin.xyz - inPos;
    vec3 toViewer = gViewOrigin.xyz - inPos;

    mat3 TBN = mat3(inTangent, inBinormal, inNormal);

    v2f.ToLight = TBN * toLight;
    v2f.ToViewer = TBN * toViewer;

    vec4 tcV4 = vec4(inUV, 1.0, 1.0);

    v2f.TC1.x = dot(tcV4, gBumpMatrix_S);
    v2f.TC1.y = dot(tcV4, gBumpMatrix_T);

    v2f.TC2.x = dot(vPos, gLightFalloff_S);
    v2f.TC2.y = 0.5;

    v2f.TC3.x = dot(vPos, gLightProject_S);
    v2f.TC3.y = dot(vPos, gLightProject_T);
    v2f.TC3.z = dot(vPos, gLightProject_Q);

    v2f.TC4.x = dot(tcV4, gDiffuseMatrix_S);
    v2f.TC4.y = dot(tcV4, gDiffuseMatrix_T);

    v2f.TC5.x = dot(tcV4, gSpecularMatrix_S);
    v2f.TC5.y = dot(tcV4, gSpecularMatrix_T);

    v2f.Color = inColor * gColorMod + gColorAdd;

    gl_Position = gMVP * vPos;
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER

uniform samplerCube gTexCubeMap;
uniform sampler2D   gTexBumpMap;
uniform sampler2D   gTexLightFalloff;
uniform sampler2D   gTexLight;
uniform sampler2D   gTexDiffuse;
uniform sampler2D   gTexSpecular;
uniform sampler2D   gTexSpecularLUT;

uniform vec4        gDiffuseModifier;
uniform vec4        gSpecularModifier;
uniform vec4        gLocalLightOrigin;
uniform vec4        gLocalViewOrigin;

in Vertex2Fragment v2f;

out vec4 oRT0;

void main() {
    // the amount of light contacting the fragment is the
    // product of the two light projections and the surface
    // bump mapping

    // normalize the direction to the light
    vec3 toLight = normalize(v2f.ToLight);
    // normalize the direction to the viewer
    vec3 toViewer = normalize(v2f.ToViewer);

    // load the filtered normal map, then normalize to full scale,
    // leaving the divergence in .w unmodified
    vec4 localNormal = texture(gTexBumpMap, v2f.TC1);
    localNormal.xyz = normalize(localNormal.xyz * 2.0 - 1.0);

    // diffuse dot product
    vec4 light = vec4(vec3(dot(toLight, localNormal.xyz)), 1.0);

    // modulate by the light projection
    light *= textureProj(gTexLight, v2f.TC3);

    // modulate by the light falloff
    light *= texture(gTexLightFalloff, v2f.TC2);

    // modulate the diffuse map and constant diffuse factor
    vec4 color = texture(gTexDiffuse, v2f.TC4);
    color *= gDiffuseModifier;

    // calculate the specular reflection vector from light and localNormal
    vec3 reflection = reflect(toLight, localNormal.xyz);
    float lookupX = dot(toViewer, reflection);

    // perform a dependent table read for the specular falloff
    // s = specular dot product, t = local normal divergence
    //vec2 lookupUV = vec2(lookupX, localNormal.w);
    vec2 lookupUV = vec2(lookupX, 0.2); //#NOTE_SK: why the fuck ?
    vec4 specular = texture(gTexSpecularLUT, lookupUV);

    // modulate by the constant specular factor
    specular *= gSpecularModifier;

    // modulate by the specular map * 2
    specular *= texture(gTexSpecular, v2f.TC5) * 2.0;

    color = (specular + color) * light;

    oRT0 = color;
}

#endif // FRAGMENT_SHADER
