struct Vertex2Fragment {
    vec3 ToLight;
    vec3 HalfVector;
    vec2 uvBumpmap;
    vec2 uvLightFalloff;
    vec3 uvLightCookie;
    vec2 uvDiffuse;
    vec2 uvSpecular;
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
    vec4 vPos = vec4(inPos, 1.0f);

    vec3 toLight = normalize(gLightOrigin.xyz - inPos);
    vec3 toViewer = normalize(gViewOrigin.xyz - inPos);

    // put toViewer vec to tangent space
    mat3 TBN = mat3(inTangent, inBinormal, inNormal);
    v2f.ToLight = toLight * TBN;

    vec4 tcV4 = vec4(inUV, 0.0f, 1.0f);

    v2f.uvBumpmap.x = dot(tcV4, gBumpMatrix_S);
    v2f.uvBumpmap.y = dot(tcV4, gBumpMatrix_T);

    v2f.uvLightFalloff.x = dot(vPos, gLightFalloff_S);
    v2f.uvLightFalloff.y = 0.5f;

    v2f.uvLightCookie.x = dot(vPos, gLightProject_S);
    v2f.uvLightCookie.y = dot(vPos, gLightProject_T);
    v2f.uvLightCookie.z = dot(vPos, gLightProject_Q);

    v2f.uvDiffuse.x = dot(tcV4, gDiffuseMatrix_S);
    v2f.uvDiffuse.y = dot(tcV4, gDiffuseMatrix_T);

    v2f.uvSpecular.x = dot(tcV4, gSpecularMatrix_S);
    v2f.uvSpecular.y = dot(tcV4, gSpecularMatrix_T);

    // add together to become the half angle vector in object space (non-normalized)
    v2f.HalfVector = (toLight + toViewer) * TBN;

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

    // normalize the half vector
    vec3 halfVector = normalize(v2f.HalfVector);

    //
    // perform the diffuse bump mapping
    //

    // load the filtered normal map, then normalize to full scale,
    vec3 localNormal = texture(gTexBumpMap, v2f.uvBumpmap).wyz;  // normalmal is RXGB compressed (R and A swapped)
    localNormal = normalize(localNormal * 2.0f - 1.0f);

    // diffuse dot product
    vec4 light = vec4(vec3(dot(toLight, localNormal)), 1.0f);

    // modulate by the light projection
    light *= textureProj(gTexLight, v2f.uvLightCookie);

    // modulate by the light falloff
    light *= texture(gTexLightFalloff, v2f.uvLightFalloff);

    //
    // the light will be modulated by the diffuse and
    // specular surface characteristics
    //

    // modulate the diffuse map and constant diffuse factor
    vec4 color = texture(gTexDiffuse, v2f.uvDiffuse);
    color *= gDiffuseModifier;

    // perform the specular bump mapping
    float lookupX = dot(halfVector, localNormal);

    // perform a dependent table read for the specular falloff
    vec2 lookupUV = vec2(lookupX, 0.5f);
    vec4 specular = texture(gTexSpecularLUT, lookupUV);

    // modulate by the constant specular factor
    specular *= gSpecularModifier;

    // modulate by the specular map * 2
    specular *= texture(gTexSpecular, v2f.uvSpecular) * 2.0f;

    color = (specular + color) * light;

    oRT0 = color * v2f.Color;
}

#endif // FRAGMENT_SHADER
