#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

//#define GL33_DISABLE_DRAW

enum E_VERTEX_UNIFORMS {
    EVU_MVP = 0,
    EVU_ModelView,
    EVU_Model,
    EVU_View,
    EVU_Projection,

    // Custom shaders
    EVU_VParams,

    EVU_LightOrigin,
    EVU_ViewOrigin,
    EVU_LightProject_S,
    EVU_LightProject_T,
    EVU_LightProject_Q,
    EVU_LightFalloff_S,
    EVU_BumpMatrix_S,
    EVU_BumpMatrix_T,
    EVU_DiffuseMatrix_S,
    EVU_DiffuseMatrix_T,
    EVU_SpecularMatrix_S,
    EVU_SpecularMatrix_T,
    EVU_ColorMod,
    EVU_ColorAdd,

    EVU_Last
};

static const char* sVertexUniforms[EVU_Last] = {
    "gMVP",
    "gModelView",
    "gModel",
    "gView",
    "gProjection",

    // Custom shaders
    "gVParams",

    "gLightOrigin",
    "gViewOrigin",
    "gLightProject_S",
    "gLightProject_T",
    "gLightProject_Q",
    "gLightFalloff_S",
    "gBumpMatrix_S",
    "gBumpMatrix_T",
    "gDiffuseMatrix_S",
    "gDiffuseMatrix_T",
    "gSpecularMatrix_S",
    "gSpecularMatrix_T",
    "gColorMod",
    "gColorAdd",
};

enum E_FRAGMENT_UNIFORMS {
    EFU_TexCubeMap = 0,
    EFU_TexBumpMap,
    EFU_TexLightFalloff,
    EFU_TexLight,
    EFU_TexDiffuse,
    EFU_TexSpecular,
    EFU_TexSpecularLUT,

    // Custom shaders
    EFU_Textures,
    EFU_FParams,

    EFU_DiffuseModifier,
    EFU_SpecularModifier,
    EFU_LocalLightOrigin,
    EFU_LocalViewOrigin,

    EFU_Last
};

static const char* sFragmentUniforms[EFU_Last] = {
    "gTexCubeMap",
    "gTexBumpMap",
    "gTexLightFalloff",
    "gTexLight",
    "gTexDiffuse",
    "gTexSpecular",
    "gTexSpecularLUT",

    // Custom shaders
    "gTextures",
    "gFParams",

    "gDiffuseModifier",
    "gSpecularModifier",
    "gLocalLightOrigin",
    "gLocalViewOrigin",
};


typedef struct {
    int     ident;
    GLuint  handle;
    GLint   vulocs[EVU_Last];   // vertex shader uniforms locations
    GLint   fulocs[EFU_Last];   // fragment shader uniforms locations
    char    name[64];
} glslProg_t;

static  const int   MAX_GLPROGS = 256;

// a single file can have both a vertex program and a fragment program
static glslProg_t glslProgs[MAX_GLPROGS] = {
    { VPROG_TEST,           0, {-1}, {-1}, "test.glsl" },
    { VPROG_INTERACTION,    0, {-1}, {-1}, "interaction.glsl" },
    { GLPROG_DEPTH_PASS,    0, {-1}, {-1}, "depth_pass.glsl" },
    { GLPROG_UNLIT_PASS,    0, {-1}, {-1}, "unlit_pass.glsl" },

// additional programs can be dynamically specified in materials
};

int gLastProgIdx = -1;


enum E_VERTEX_ATTRIBS {
    EVA_Pos = 0,
    EVA_UV,
    EVA_Normal,
    EVA_Tangent,
    EVA_Binormal,
    EVA_Color
};



static void GL_SelectTextureNoClient(const int unit) {
    backEnd.glState.currenttmu = unit;
    glActiveTexture(GL_TEXTURE0 + unit);
    RB_LogComment("glActiveTextureARB( %i )\n", unit);
}


static GLuint R_CompileGLSLShader(const char* src, int len, const bool isFragment) {
    GLuint shader = glCreateShader(isFragment ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
    if (shader) {
        GLint status = 0;

        const char* sources[3] = {
            "#version 330\n",
            (isFragment ? "#define FRAGMENT_SHADER\n" : "#define VERTEX_SHADER\n"),
            src
        };

        const int lengthes[3] = {
            static_cast<int>(strlen(sources[0])),
            static_cast<int>(strlen(sources[1])),
            len
        };

        glShaderSource(shader, 3, sources, lengthes);
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

        // we grab the log anyway
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            char* log = (char *)_alloca(infoLen + 1);
            glGetShaderInfoLog(shader, infoLen, NULL, log);
            common->Printf("GLSL shader compilation failed:\n%s\n", log);
        }

        if (!status) {
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}


/*
=================
R_LoadGLSLProgram
=================
*/
void R_LoadGLSLProgram(int progIndex) {
    int     fileLen;
    idStr   fullPath = "glprogs/";
    fullPath += glslProgs[progIndex].name;
    char*   fileBuffer;

    if (!glConfig.isInitialized || !glConfig.glslShadersAvailable) {
        return;
    }

    common->Printf("%s", fullPath.c_str());

    // load the program even if we don't support it, so
    // fs_copyfiles can generate cross-platform data dumps
    fileLen = fileSystem->ReadFile(fullPath.c_str(), (void **)&fileBuffer, NULL);
    if (!fileBuffer) {
        common->Printf(": File not found\n");
        glslProgs[progIndex].ident = -1;
        return;
    }

    GLuint oldShader = glslProgs[progIndex].handle;

    GLuint vertexShader = R_CompileGLSLShader(fileBuffer, fileLen, false);
    GLuint fragmentShader = R_CompileGLSLShader(fileBuffer, fileLen, true);

    fileSystem->FreeFile(fileBuffer);

    //
    // submit the program string at start to GL
    //
    if (glslProgs[progIndex].ident == 0) {
        // allocate a new identifier for this program
        glslProgs[progIndex].ident = PROG_USER + progIndex;
    }

    if (vertexShader && fragmentShader) {
        GLuint shader = glCreateProgram();
        if (shader) {
            glAttachShader(shader, vertexShader);
            glAttachShader(shader, fragmentShader);
            glLinkProgram(shader);

            // we don't need our shaders anymore
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            // we grab the log anyway
            GLint infoLen = 0;
            glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1) {
                char* log = (char *)_alloca(infoLen + 1);
                glGetProgramInfoLog(shader, infoLen, NULL, log);
                common->Printf("GLSL program link failed:\n%s\n", log);
            }

            GLint status = 0;
            glGetProgramiv(shader, GL_LINK_STATUS, &status);
            if (!status) {
                glDeleteProgram(shader);
            } else {
                if (oldShader) {
                    glDeleteProgram(oldShader);
                }

                glslProgs[progIndex].handle = shader;

                // collect uniforms
                for (int i = 0; i < EVU_Last; ++i) {
                    glslProgs[progIndex].vulocs[i] = glGetUniformLocation(shader, sVertexUniforms[i]);
                }
                for (int i = 0; i < EFU_Last; ++i) {
                    glslProgs[progIndex].fulocs[i] = glGetUniformLocation(shader, sFragmentUniforms[i]);
                }
            }
        }
    }

    common->Printf("\n");
}

/*
==================
R_FindGLSLProgram

Returns a GL identifier that can be bound to the given target, parsing
a text file if it hasn't already been loaded.
==================
*/
int R_FindGLSLProgram(const char* program) {
    int     i;
    idStr   stripped = program;

    stripped.StripFileExtension();

    // see if it is already loaded
    for (i = 0; glslProgs[i].name[0]; ++i) {
        idStr compare = glslProgs[i].name;
        compare.StripFileExtension();

        if (!idStr::Icmp(stripped.c_str(), compare.c_str())) {
            return glslProgs[i].ident;
        }
    }

    if (i == MAX_GLPROGS) {
        common->Error("R_FindGLSLProgram: MAX_GLPROGS");
        ASSERT(false);
    }

    //#NOTE_SK: here we replace ARB programs with GLSL shaders
    stripped += ".glsl";

    // add it to the list and load it
    glslProgs[i].ident = 0;	// will be gen'd by R_LoadARBProgram
    strncpy(glslProgs[i].name, stripped.c_str(), sizeof(glslProgs[i].name) - 1);

    R_LoadGLSLProgram(i);

    return glslProgs[i].ident;
}

/*
==================
R_ReloadGLSLPrograms_f
==================
*/
void R_ReloadGLSLPrograms_f(const idCmdArgs &args) {
    common->Printf("----- R_ReloadGLSLPrograms -----\n");
    for (int i = 0; glslProgs[i].name[0]; ++i) {
        R_LoadGLSLProgram(i);
    }
    common->Printf("-------------------------------\n");
}



/*
==================
R_GL33_Init
==================
*/
void R_GL33_Init() {
    glConfig.allowGL33Path = false;

    common->Printf("---------- R_GL33_Init ----------\n");

    if (!glConfig.glslShadersAvailable) {
        common->Printf("Not available.\n");
        return;
    }

    common->Printf("Available.\n");

    common->Printf("---------------------------------\n");

    glConfig.allowGL33Path = true;
}

static void SetUniformVec4(const int loc, const float* v) {
    if (loc >= 0) {
        glUniform4fv(loc, 1, v);
    }
}

static void SetUniformMat4(const int loc, const float* v) {
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, v);
    }
}

int R_GL33_UseProgram(const int ident) {
    gLastProgIdx = -1;

    if (ident >= 0) {
        for (int i = 0; glslProgs[i].name[0]; ++i) {
            if (glslProgs[i].ident == ident) {
                gLastProgIdx = i;
                glUseProgram(glslProgs[i].handle);
                break;
            }
        }
    }

    return gLastProgIdx;
}

/*
==================
RB_GL33_DrawInteraction
==================
*/
void RB_GL33_DrawInteraction(const drawInteraction_t* din) {
#ifdef GL33_DISABLE_DRAW
    return;
#endif

    glslProg_t& prog = glslProgs[gLastProgIdx];

    // load all the vertex program uniforms
    SetUniformMat4(prog.vulocs[EVU_MVP],              din->modelViewProj.ToFloatPtr());

    SetUniformVec4(prog.vulocs[EVU_LightOrigin],      din->localLightOrigin.ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_ViewOrigin],       din->localViewOrigin.ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_LightProject_S],   din->lightProjection[0].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_LightProject_T],   din->lightProjection[1].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_LightProject_Q],   din->lightProjection[2].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_LightFalloff_S],   din->lightProjection[3].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_BumpMatrix_S],     din->bumpMatrix[0].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_BumpMatrix_T],     din->bumpMatrix[1].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_DiffuseMatrix_S],  din->diffuseMatrix[0].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_DiffuseMatrix_T],  din->diffuseMatrix[1].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_SpecularMatrix_S], din->specularMatrix[0].ToFloatPtr());
    SetUniformVec4(prog.vulocs[EVU_SpecularMatrix_T], din->specularMatrix[1].ToFloatPtr());

    static const float zero[4] = { 0, 0, 0, 0 };
    static const float one[4] = { 1, 1, 1, 1 };
    static const float negOne[4] = { -1, -1, -1, -1 };

    switch (din->vertexColor) {
        case SVC_IGNORE:
            SetUniformVec4(prog.vulocs[EVU_ColorMod], zero);
            SetUniformVec4(prog.vulocs[EVU_ColorAdd], one);
            break;
        case SVC_MODULATE:
            SetUniformVec4(prog.vulocs[EVU_ColorMod], one);
            SetUniformVec4(prog.vulocs[EVU_ColorAdd], zero);
            break;
        case SVC_INVERSE_MODULATE:
            SetUniformVec4(prog.vulocs[EVU_ColorMod], negOne);
            SetUniformVec4(prog.vulocs[EVU_ColorAdd], one);
            break;
    }

    // load all fragment program uniforms
    SetUniformVec4(prog.fulocs[EFU_DiffuseModifier],  din->diffuseColor.ToFloatPtr());
    SetUniformVec4(prog.fulocs[EFU_SpecularModifier], din->specularColor.ToFloatPtr());

    SetUniformVec4(prog.fulocs[EFU_LocalLightOrigin], din->localLightOrigin.ToFloatPtr());
    SetUniformVec4(prog.fulocs[EFU_LocalViewOrigin],  din->localViewOrigin.ToFloatPtr());

    // set the textures

    // texture 1 will be the per-surface bump map
    GL_SelectTextureNoClient(EFU_TexBumpMap);
    din->bumpImage->Bind();

    // texture 2 will be the light falloff texture
    GL_SelectTextureNoClient(EFU_TexLightFalloff);
    din->lightFalloffImage->Bind();

    // texture 3 will be the light projection texture
    GL_SelectTextureNoClient(EFU_TexLight);
    din->lightImage->Bind();

    // texture 4 is the per-surface diffuse map
    GL_SelectTextureNoClient(EFU_TexDiffuse);
    din->diffuseImage->Bind();

    // texture 5 is the per-surface specular map
    GL_SelectTextureNoClient(EFU_TexSpecular);
    din->specularImage->Bind();

    // draw it
    RB_DrawElementsWithCounters(din->surf->geo);
}

/*
=============
RB_GL33_CreateDrawInteractions
=============
*/
void RB_GL33_CreateDrawInteractions(const drawSurf_t* surf) {
#ifdef GL33_DISABLE_DRAW
    return;
#endif

    if (!surf) {
        return;
    }

    // perform setup here that will be constant for all interactions
    GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc);

    // bind the vertex program
    const int progIdx = R_GL33_UseProgram(VPROG_INTERACTION);
    if (progIdx < 0) {
        return;
    }

    // we pre-assign sampler uniforms
    for (int i = 0; i < EFU_Last; ++i) {
        if (i <= EFU_TexSpecularLUT && glslProgs[progIdx].fulocs[i] >= 0) {
            glUniform1i(glslProgs[progIdx].fulocs[i], i);
        }
    }

    // enable the vertex arrays
    glEnableVertexAttribArray(EVA_Pos);
    glEnableVertexAttribArray(EVA_UV);
    glEnableVertexAttribArray(EVA_Normal);
    glEnableVertexAttribArray(EVA_Tangent);
    glEnableVertexAttribArray(EVA_Binormal);
    glEnableVertexAttribArray(EVA_Color);

    // texture 0 is the normalization cube map for the vector towards the light
    GL_SelectTextureNoClient(EFU_TexCubeMap);
    if (backEnd.vLight->lightShader->IsAmbientLight()) {
        globalImages->ambientNormalMap->Bind();
    } else {
        globalImages->normalCubeMapImage->Bind();
    }

    // texture 6 is the specular lookup table
    GL_SelectTextureNoClient(EFU_TexSpecularLUT);
    //if (r_testARBProgram.GetBool()) {
    //    globalImages->specular2DTableImage->Bind();	// variable specularity in alpha channel
    //} else {
        globalImages->specularTableImage->Bind();
    //}


    for (; surf; surf = surf->nextOnLight) {
        // perform setup here that will not change over multiple interaction passes

        // set the vertex pointers
        idDrawVert* ac = (idDrawVert *)vertexCache.Position(surf->geo->ambientCache);
        glVertexAttribPointer(EVA_Pos,      3, GL_FLOAT,         false, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
        glVertexAttribPointer(EVA_UV,       2, GL_FLOAT,         false, sizeof(idDrawVert), ac->st.ToFloatPtr());
        glVertexAttribPointer(EVA_Normal,   3, GL_FLOAT,         false, sizeof(idDrawVert), ac->normal.ToFloatPtr());
        glVertexAttribPointer(EVA_Tangent,  3, GL_FLOAT,         false, sizeof(idDrawVert), ac->tangents[0].ToFloatPtr());
        glVertexAttribPointer(EVA_Binormal, 3, GL_FLOAT,         false, sizeof(idDrawVert), ac->tangents[1].ToFloatPtr());
        glVertexAttribPointer(EVA_Color,    4, GL_UNSIGNED_BYTE, true,  sizeof(idDrawVert), ac->color);

        // this may cause RB_GL33_DrawInteraction to be exacuted multiple
        // times with different colors and images if the surface or light have multiple layers
        RB_CreateSingleDrawInteractions(surf, RB_GL33_DrawInteraction);
    }

    glDisableVertexAttribArray(EVA_Pos);
    glDisableVertexAttribArray(EVA_UV);
    glDisableVertexAttribArray(EVA_Normal);
    glDisableVertexAttribArray(EVA_Tangent);
    glDisableVertexAttribArray(EVA_Binormal);
    glDisableVertexAttribArray(EVA_Color);

    // disable features
    GL_SelectTextureNoClient(EFU_TexSpecularLUT);
    globalImages->BindNull();

    GL_SelectTextureNoClient(EFU_TexSpecular);
    globalImages->BindNull();

    GL_SelectTextureNoClient(EFU_TexDiffuse);
    globalImages->BindNull();

    GL_SelectTextureNoClient(EFU_TexLight);
    globalImages->BindNull();

    GL_SelectTextureNoClient(EFU_TexLightFalloff);
    globalImages->BindNull();

    GL_SelectTextureNoClient(EFU_TexBumpMap);
    globalImages->BindNull();

    backEnd.glState.currenttmu = -1;
    GL_SelectTexture(0);

    glUseProgram(0);
}


/*
==================
RB_ARB2_DrawInteractions
==================
*/
void RB_GL33_DrawInteractions() {
#ifdef GL33_DISABLE_DRAW
    return;
#endif

    viewLight_t*        vLight;
    const idMaterial*   lightShader;

    GL_SelectTexture(0);

    //
    // for each light, perform adding and shadowing
    //
    for (vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next) {
        backEnd.vLight = vLight;

        // do fogging later
        if (vLight->lightShader->IsFogLight()) {
            continue;
        }
        if (vLight->lightShader->IsBlendLight()) {
            continue;
        }

        if (!vLight->localInteractions && !vLight->globalInteractions
            && !vLight->translucentInteractions) {
            continue;
        }

        lightShader = vLight->lightShader;

        // clear the stencil buffer if needed
        if (vLight->globalShadows || vLight->localShadows) {
            backEnd.currentScissor = vLight->scissorRect;
            if (r_useScissor.GetBool()) {
                glScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
                    backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
                    backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
                    backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
            }
            glClear(GL_STENCIL_BUFFER_BIT);
        } else {
            // no shadows, so no need to read or write the stencil buffer
            // we might in theory want to use GL_ALWAYS instead of disabling
            // completely, to satisfy the invarience rules
            glStencilFunc(GL_ALWAYS, 128, 255);
        }

       /* if (r_useShadowVertexProgram.GetBool()) {
            glEnable(GL_VERTEX_PROGRAM_ARB);
            qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, VPROG_STENCIL_SHADOW);
            RB_StencilShadowPass(vLight->globalShadows);
            RB_ARB2_CreateDrawInteractions(vLight->localInteractions);
            glEnable(GL_VERTEX_PROGRAM_ARB);
            qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, VPROG_STENCIL_SHADOW);
            RB_StencilShadowPass(vLight->localShadows);
            RB_ARB2_CreateDrawInteractions(vLight->globalInteractions);
            glDisable(GL_VERTEX_PROGRAM_ARB);	// if there weren't any globalInteractions, it would have stayed on
        } else*/ {
            RB_StencilShadowPass(vLight->globalShadows);
            RB_GL33_CreateDrawInteractions(vLight->localInteractions);
            RB_StencilShadowPass(vLight->localShadows);
            RB_GL33_CreateDrawInteractions(vLight->globalInteractions);
        }

        // translucent surfaces never get stencil shadowed
        if (r_skipTranslucent.GetBool()) {
            continue;
        }

        glStencilFunc(GL_ALWAYS, 128, 255);

        backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
        RB_GL33_CreateDrawInteractions(vLight->translucentInteractions);

        backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
    }

    // disable stencil shadow test
    glStencilFunc(GL_ALWAYS, 128, 255);

    GL_SelectTexture(0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}


/*
==================
RB_GL33_FillDepthBuffer
==================
*/
void RB_GL33_FillDepthBuffer(const drawSurf_t *surf) {
#ifdef GL33_DISABLE_DRAW
    return;
#endif

    int			stage;
    const idMaterial	*shader;
    const shaderStage_t *pStage;
    const float	*regs;
    float		color[4];
    const srfTriangles_t	*tri;

    tri = surf->geo;
    shader = surf->material;

    // update the clip plane if needed
    if (backEnd.viewDef->numClipPlanes && surf->space != backEnd.currentSpace) {
        GL_SelectTexture(1);

        idPlane	plane;

        R_GlobalPlaneToLocal(surf->space->modelMatrix.ToFloatPtr(), backEnd.viewDef->clipPlanes[0], plane);
        plane[3] += 0.5;	// the notch is in the middle
        glTexGenfv(GL_S, GL_OBJECT_PLANE, plane.ToFloatPtr());
        GL_SelectTexture(0);
    }

    if (!shader->IsDrawn()) {
        return;
    }

    // some deforms may disable themselves by setting numIndexes = 0
    if (!tri->numIndexes) {
        return;
    }

    // translucent surfaces don't put anything in the depth buffer and don't
    // test against it, which makes them fail the mirror clip plane operation
    if (shader->Coverage() == MC_TRANSLUCENT) {
        return;
    }

    if (!tri->ambientCache) {
        common->Printf("RB_T_FillDepthBuffer: !tri->ambientCache\n");
        return;
    }

    // get the expressions for conditionals / color / texcoords
    regs = surf->shaderRegisters;

    // if all stages of a material have been conditioned off, don't do anything
    for (stage = 0; stage < shader->GetNumStages(); stage++) {
        pStage = shader->GetStage(stage);
        // check the stage enable condition
        if (regs[pStage->conditionRegister] != 0) {
            break;
        }
    }
    if (stage == shader->GetNumStages()) {
        return;
    }

    const int progIdx = R_GL33_UseProgram(GLPROG_DEPTH_PASS);
    if (progIdx < 0) {
        return;
    }

    const glslProg_t& prog = glslProgs[progIdx];

    idMat4 mvp = surf->space->modelViewMatrix * backEnd.viewDef->projectionMatrix;

    // load all the vertex program uniforms
    SetUniformMat4(prog.vulocs[EVU_MVP], mvp.ToFloatPtr());

    glUniform1i(prog.fulocs[EFU_TexDiffuse], 0);

    // set polygon offset if necessary
    if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset());
    }

    // subviews will just down-modulate the color buffer by overbright
    if (shader->GetSort() == SS_SUBVIEW) {
        GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS);
        color[0] =
        color[1] =
        color[2] = (1.0 / backEnd.overBright);
        color[3] = 1;
    } else {
        // others just draw black
        color[0] = 0;
        color[1] = 0;
        color[2] = 0;
        color[3] = 1;
    }

    glEnableVertexAttribArray(EVA_Pos);
    glEnableVertexAttribArray(EVA_UV);

    idDrawVert *ac = (idDrawVert *)vertexCache.Position(tri->ambientCache);
    glVertexAttribPointer(EVA_Pos, 3, GL_FLOAT, GL_FALSE, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
    glVertexAttribPointer(EVA_UV,  2, GL_FLOAT, GL_FALSE, sizeof(idDrawVert), ac->st.ToFloatPtr());

    bool drawSolid = false;

    if (shader->Coverage() == MC_OPAQUE) {
        drawSolid = true;
    }

    // we may have multiple alpha tested stages
    if (shader->Coverage() == MC_PERFORATED) {
        // if the only alpha tested stages are condition register omitted,
        // draw a normal opaque surface
        bool	didDraw = false;

        glEnable(GL_ALPHA_TEST);
        // perforated surfaces may have multiple alpha tested stages
        for (stage = 0; stage < shader->GetNumStages(); stage++) {
            pStage = shader->GetStage(stage);

            if (!pStage->hasAlphaTest) {
                continue;
            }

            // check the stage enable condition
            if (regs[pStage->conditionRegister] == 0) {
                continue;
            }

            // if we at least tried to draw an alpha tested stage,
            // we won't draw the opaque surface
            didDraw = true;

            // set the alpha modulate
            color[3] = regs[pStage->color.registers[3]];

            // skip the entire stage if alpha would be black
            if (color[3] <= 0) {
                continue;
            }
            glColor4fv(color);

            SetUniformVec4(prog.fulocs[EFU_DiffuseModifier], color);

            glAlphaFunc(GL_GREATER, regs[pStage->alphaTestRegister]);

            // bind the texture
            pStage->texture.image->Bind();

            // set texture matrix and texGens
            //RB_PrepareStageTexturing(pStage, surf, ac);

            // draw it
            RB_DrawElementsWithCounters(tri);

            //RB_FinishStageTexturing(pStage, surf, ac);
        }
        glDisable(GL_ALPHA_TEST);
        if (!didDraw) {
            drawSolid = true;
        }
    }

    // draw the entire surface solid
    if (drawSolid) {
        glColor4fv(color);
        SetUniformVec4(prog.fulocs[EFU_DiffuseModifier], color);

        globalImages->whiteImage->Bind();

        // draw it
        RB_DrawElementsWithCounters(tri);
    }

    glUseProgram(0);

    glDisableVertexAttribArray(EVA_Pos);
    glDisableVertexAttribArray(EVA_UV);

    // reset polygon offset
    if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // reset blending
    if (shader->GetSort() == SS_SUBVIEW) {
        GL_State(GLS_DEPTHFUNC_LESS);
    }
}

/*
==================
RB_GL33_RenderShaderPasses

This is also called for the generated 2D rendering
==================
*/
void RB_GL33_RenderShaderPasses(const drawSurf_t *surf) {
    int                     stage;
    const idMaterial*       material;
    const shaderStage_t*    pStage;
    const float*            regs;
    const srfTriangles_t*   tri;

    tri = surf->geo;
    material = surf->material;

    if (!material->HasAmbient()) {
        return;
    }

    if (material->IsPortalSky()) {
        return;
    }

    // change the matrix if needed
    if (surf->space != backEnd.currentSpace) {
        glLoadMatrixf(surf->space->modelViewMatrix.ToFloatPtr());
        backEnd.currentSpace = surf->space;
        //RB_SetProgramEnvironmentSpace();
    }

    // change the scissor if needed
    if (r_useScissor.GetBool() && !backEnd.currentScissor.Equals(surf->scissorRect)) {
        backEnd.currentScissor = surf->scissorRect;
        glScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
            backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
            backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
            backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
    }

    // some deforms may disable themselves by setting numIndexes = 0
    if (!tri->numIndexes) {
        return;
    }

    if (!tri->ambientCache) {
        common->Printf("RB_T_RenderShaderPasses: !tri->ambientCache\n");
        return;
    }

    // get the expressions for conditionals / color / texcoords
    regs = surf->shaderRegisters;

    // set face culling appropriately
    GL_Cull(material->GetCullType());

    // set polygon offset if necessary
    if (material->TestMaterialFlag(MF_POLYGONOFFSET)) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * material->GetPolygonOffset());
    }

    if (surf->space->weaponDepthHack) {
        RB_EnterWeaponDepthHack();
    }

    if (surf->space->modelDepthHack != 0.0f) {
        RB_EnterModelDepthHack(surf->space->modelDepthHack);
    }

    int progIdx = R_GL33_UseProgram(GLPROG_UNLIT_PASS);
    if (progIdx < 0) {
        return;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glEnableVertexAttribArray(EVA_Pos);
    glEnableVertexAttribArray(EVA_UV);
    glEnableVertexAttribArray(EVA_Normal);
    glEnableVertexAttribArray(EVA_Tangent);
    glEnableVertexAttribArray(EVA_Binormal);
    glEnableVertexAttribArray(EVA_Color);

    idDrawVert *ac = (idDrawVert *)vertexCache.Position(tri->ambientCache);

    ///////////
    //#NOTE_SK: this bullshit fixes glDrawElements crash on Nvidia :(
    glVertexPointer(3, GL_FLOAT, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
    glTexCoordPointer(2, GL_FLOAT, sizeof(idDrawVert), ac->st.ToFloatPtr());
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(idDrawVert), ac->color);
    ///////////

    glVertexAttribPointer(EVA_Pos,      3, GL_FLOAT,         GL_FALSE, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
    glVertexAttribPointer(EVA_UV,       2, GL_FLOAT,         GL_FALSE, sizeof(idDrawVert), ac->st.ToFloatPtr());
    glVertexAttribPointer(EVA_Normal,   3, GL_FLOAT,         GL_FALSE, sizeof(idDrawVert), ac->normal.ToFloatPtr());
    glVertexAttribPointer(EVA_Tangent,  3, GL_FLOAT,         GL_FALSE, sizeof(idDrawVert), ac->tangents[0].ToFloatPtr());
    glVertexAttribPointer(EVA_Binormal, 3, GL_FLOAT,         GL_FALSE, sizeof(idDrawVert), ac->tangents[1].ToFloatPtr());
    glVertexAttribPointer(EVA_Color,    4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(idDrawVert), ac->color);

    const glslProg_t* prog = &glslProgs[progIdx];

    idMat4 mvp = surf->space->modelViewMatrix * backEnd.viewDef->projectionMatrix;

    // load all the vertex program uniforms
    SetUniformMat4(prog->vulocs[EVU_MVP], mvp.ToFloatPtr());
    SetUniformMat4(prog->vulocs[EVU_ModelView], surf->space->modelViewMatrix.ToFloatPtr());
    SetUniformMat4(prog->vulocs[EVU_Projection], backEnd.viewDef->projectionMatrix.ToFloatPtr());

    glUniform1i(prog->fulocs[EFU_TexDiffuse], 0);

    for (stage = 0; stage < material->GetNumStages(); stage++) {
        pStage = material->GetStage(stage);

        // check the enable condition
        if (regs[pStage->conditionRegister] == 0) {
            continue;
        }

        // skip the stages involved in lighting
        if (pStage->lighting != SL_AMBIENT) {
            continue;
        }

        // skip if the stage is ( GL_ZERO, GL_ONE ), which is used for some alpha masks
        if ((pStage->drawStateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == (GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE)) {
            continue;
        }

        // see if we are a new-style stage
        newShaderStage_t *newStage = pStage->newStage;
        if (newStage) {
            //--------------------------
            //
            // new style stages
            //
            //--------------------------

            if (r_skipNewAmbient.GetBool()) {
                continue;
            }

#if 1
            GL_State(pStage->drawStateBits);

            progIdx = R_GL33_UseProgram(newStage->vertexProgram);
            if (progIdx < 0) {
                continue;
            }

            prog = &glslProgs[progIdx];

            // megaTextures bind a lot of images and set a lot of parameters
            //#TODO_SK: implement ???
            //if (newStage->megaTexture) {
            //    newStage->megaTexture->SetMappingForSurface(tri);
            //    idVec3	localViewer;
            //    R_GlobalPointToLocal(surf->space->modelMatrix.ToFloatPtr(), backEnd.viewDef->renderView.vieworg, localViewer);
            //    newStage->megaTexture->BindForViewOrigin(localViewer);
            //}

            // load all the vertex program uniforms
            SetUniformMat4(prog->vulocs[EVU_MVP], mvp.ToFloatPtr());
            SetUniformMat4(prog->vulocs[EVU_ModelView], surf->space->modelViewMatrix.ToFloatPtr());
            SetUniformMat4(prog->vulocs[EVU_Projection], backEnd.viewDef->projectionMatrix.ToFloatPtr());

            idVec4 vparm;
            for (int i = 0; i < newStage->numVertexParms; ++i) {
                vparm.x = regs[newStage->vertexParms[i][0]];
                vparm.y = regs[newStage->vertexParms[i][1]];
                vparm.z = regs[newStage->vertexParms[i][2]];
                vparm.w = regs[newStage->vertexParms[i][3]];

                SetUniformVec4(prog->vulocs[EVU_VParams] + i, vparm.ToFloatPtr());
            }

            for (int i = 0; i < newStage->numFragmentProgramImages; ++i) {
                if (newStage->fragmentProgramImages[i]) {
                    GL_SelectTexture(i);
                    newStage->fragmentProgramImages[i]->Bind();

                    glUniform1i(prog->fulocs[EFU_Textures] + i, i);
                }
            }

            idVec4 fparm;
            // screen power of two correction factor, assuming the copy to _currentRender
            // also copied an extra row and column for the bilerp
            int w = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
            int pot = globalImages->currentRenderImage->uploadWidth;
            fparm.x = (float)w / pot;

            int h = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
            pot = globalImages->currentRenderImage->uploadHeight;
            fparm.y = (float)h / pot;

            fparm.z = 0;
            fparm.w = 1;
            SetUniformVec4(prog->fulocs[EFU_FParams] + 0, fparm.ToFloatPtr());

            // window coord to 0.0 to 1.0 conversion
            fparm.x = 1.0 / w;
            fparm.y = 1.0 / h;
            SetUniformVec4(prog->fulocs[EFU_FParams] + 1, fparm.ToFloatPtr());

            // draw it
            RB_DrawElementsWithCounters(tri);

            for (int i = 1; i < newStage->numFragmentProgramImages; ++i) {
                if (newStage->fragmentProgramImages[i]) {
                    GL_SelectTexture(i);
                    globalImages->BindNull();
                }
            }
            if (newStage->megaTexture) {
                newStage->megaTexture->Unbind();
            }

            GL_SelectTexture(0);

            glUseProgram(0);
#endif
            continue;
        }

        //--------------------------
        //
        // old style stages
        //
        //--------------------------

        idVec4 colorMod, colorAdd;
        colorMod.Zero();

        // set the color
        colorAdd.x = regs[pStage->color.registers[0]];
        colorAdd.y = regs[pStage->color.registers[1]];
        colorAdd.z = regs[pStage->color.registers[2]];
        colorAdd.w = regs[pStage->color.registers[3]];

        // skip the entire stage if an add would be black
        if ((pStage->drawStateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE)
            && colorAdd.x <= 0 && colorAdd.y <= 0 && colorAdd.z <= 0) {
            continue;
        }

        // skip the entire stage if a blend would be completely transparent
        if ((pStage->drawStateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)
            && colorAdd.w <= 0) {
            continue;
        }

        // select the vertex color source
        if (pStage->vertexColor == SVC_IGNORE) {
            //glColor4fv(color);
        } else {
            //glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(idDrawVert), (void *)&ac->color);
            //glEnableClientState(GL_COLOR_ARRAY);

            //#TODO_SK: TODO !!!
#if 0
            if (pStage->vertexColor == SVC_INVERSE_MODULATE) {
                GL_TexEnv(GL_COMBINE_ARB);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);
            }
#endif

            // for vertex color and modulated color, we need to enable a second
            // texture stage
#if 0
            if (color[0] != 1 || color[1] != 1 || color[2] != 1 || color[3] != 1) {
                GL_SelectTexture(1);

                globalImages->whiteImage->Bind();
                GL_TexEnv(GL_COMBINE_ARB);

                qglTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);

                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_CONSTANT_ARB);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);

                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_CONSTANT_ARB);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);

                GL_SelectTexture(0);
            }
#endif
        }

        SetUniformVec4(prog->vulocs[EVU_ColorMod], colorMod.ToFloatPtr());
        SetUniformVec4(prog->vulocs[EVU_ColorAdd], colorAdd.ToFloatPtr());

        // bind the texture
        RB_BindVariableStageImage(&pStage->texture, regs);

        // set the state
        GL_State(pStage->drawStateBits);

        //RB_PrepareStageTexturing(pStage, surf, ac);

        // draw it
        RB_DrawElementsWithCounters(tri);

        //RB_FinishStageTexturing(pStage, surf, ac);

#if 0
        if (pStage->vertexColor != SVC_IGNORE) {
            glDisableClientState(GL_COLOR_ARRAY);

            GL_SelectTexture(1);
            GL_TexEnv(GL_MODULATE);
            globalImages->BindNull();
            GL_SelectTexture(0);
            GL_TexEnv(GL_MODULATE);
        }
#endif
    }

    // reset polygon offset
    if (material->TestMaterialFlag(MF_POLYGONOFFSET)) {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    if (surf->space->weaponDepthHack || surf->space->modelDepthHack != 0.0f) {
        RB_LeaveDepthHack();
    }

    glUseProgram(0);

    glDisableVertexAttribArray(EVA_Pos);
    glDisableVertexAttribArray(EVA_UV);
    glDisableVertexAttribArray(EVA_Normal);
    glDisableVertexAttribArray(EVA_Tangent);
    glDisableVertexAttribArray(EVA_Binormal);
    glDisableVertexAttribArray(EVA_Color);
}
