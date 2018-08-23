#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

enum E_VERTEX_UNIFORMS {
    EVU_MVP = 0,
    EVU_Model,

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
    "gModel",

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
    { GLPROG_DEPTH_PASS,    0, {-1}, {-1}, "depth_pass.glsl" },

// additional programs can be dynamically specified in materials
};


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
    qglActiveTextureARB(GL_TEXTURE0_ARB + unit);
    RB_LogComment("glActiveTextureARB( %i )\n", unit);
}


static GLuint R_CompileGLSLShader(const char* src, int len, const bool isFragment) {
    GLuint shader = qglCreateShaderObjectARB(isFragment ? GL_FRAGMENT_SHADER_ARB : GL_VERTEX_SHADER_ARB);
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

        qglShaderSourceARB(shader, 3, sources, lengthes);
        qglCompileShaderARB(shader);
        qglGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);

        // we grab the log anyway
        GLint infoLen = 0;
        qglGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infoLen);
        if (infoLen > 1) {
            char* log = (char *)_alloca(infoLen + 1);
            qglGetInfoLogARB(shader, infoLen, NULL, log);
            common->Printf("GLSL shader compilation failed:\n%s\n", log);
        }

        if (!status) {
            qglDeleteObjectARB(shader);
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
        return;
    }

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
        GLuint shader = qglCreateProgramObjectARB();
        if (shader) {
            qglAttachObjectARB(shader, vertexShader);
            qglAttachObjectARB(shader, fragmentShader);
            qglLinkProgramARB(shader);

            // we don't need our shaders anymore
            qglDeleteObjectARB(vertexShader);
            qglDeleteObjectARB(fragmentShader);

            // we grab the log anyway
            GLint infoLen = 0;
            qglGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infoLen);
            if (infoLen > 1) {
                char* log = (char *)_alloca(infoLen + 1);
                qglGetInfoLogARB(shader, infoLen, NULL, log);
                common->Printf("GLSL program link failed:\n%s\n", log);
            }

            GLint status = 0;
            qglGetObjectParameterivARB(shader, GL_OBJECT_LINK_STATUS_ARB, &status);
            if (!status) {
                qglDeleteObjectARB(shader);
            } else {
                glslProgs[progIndex].handle = shader;

                // collect uniforms
                for (int i = 0; i < EVU_Last; ++i) {
                    glslProgs[progIndex].vulocs[i] = qglGetUniformLocationARB(shader, sVertexUniforms[i]);
                }
                for (int i = 0; i < EFU_Last; ++i) {
                    glslProgs[progIndex].fulocs[i] = qglGetUniformLocationARB(shader, sFragmentUniforms[i]);

                    //// we pre-assign sampler uniforms
                    //if (i <= EFU_TexSpecularLUT && glslProgs[progIndex].fulocs[i] >= 0) {
                    //    qglUniform1iARB(glslProgs[progIndex].fulocs[i], i);
                    //}
                }
            }
        }
    }

    common->Printf("\n");
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
        qglUniform4fvARB(loc, 1, v);
    }
}

static void SetUniformMat4(const int loc, const float* v) {
    if (loc >= 0) {
        qglUniformMatrix4fvARB(loc, 1, GL_FALSE, v);
    }
}

int R_GL33_UseProgram(const int ident) {
    int result = -1;

    for (int i = 0; glslProgs[i].name[0]; ++i) {
        if (glslProgs[i].ident == ident) {
            result = i;
            qglUseProgramObjectARB(glslProgs[i].handle);
            break;
        }
    }

    return result;
}

/*
==================
RB_GL33_DrawInteraction
==================
*/
void RB_GL33_DrawInteraction(const drawInteraction_t* din) {
    glslProg_t& prog = glslProgs[0];

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
    if (!surf) {
        return;
    }

    // perform setup here that will be constant for all interactions
    GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc);

    // bind the vertex program
    const int progIdx = R_GL33_UseProgram(VPROG_TEST);
    if (progIdx < 0) {
        return;
    }

    // we pre-assign sampler uniforms
    for (int i = 0; i < EFU_Last; ++i) {
        if (i <= EFU_TexSpecularLUT && glslProgs[0].fulocs[i] >= 0) {
            qglUniform1iARB(glslProgs[progIdx].fulocs[i], i);
        }
    }

    // enable the vertex arrays
    qglEnableVertexAttribArrayARB(EVA_Pos);
    qglEnableVertexAttribArrayARB(EVA_UV);
    qglEnableVertexAttribArrayARB(EVA_Normal);
    qglEnableVertexAttribArrayARB(EVA_Tangent);
    qglEnableVertexAttribArrayARB(EVA_Binormal);
    qglEnableVertexAttribArrayARB(EVA_Color);

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
        globalImages->specular2DTableImage->Bind();	// variable specularity in alpha channel
    //} else {
    //    globalImages->specularTableImage->Bind();
    //}


    for (; surf; surf = surf->nextOnLight) {
        // perform setup here that will not change over multiple interaction passes

        // set the vertex pointers
        idDrawVert* ac = (idDrawVert *)vertexCache.Position(surf->geo->ambientCache);
        qglVertexAttribPointerARB(EVA_Pos,      3, GL_FLOAT,         false, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
        qglVertexAttribPointerARB(EVA_UV,       2, GL_FLOAT,         false, sizeof(idDrawVert), ac->st.ToFloatPtr());
        qglVertexAttribPointerARB(EVA_Normal,   3, GL_FLOAT,         false, sizeof(idDrawVert), ac->normal.ToFloatPtr());
        qglVertexAttribPointerARB(EVA_Tangent,  3, GL_FLOAT,         false, sizeof(idDrawVert), ac->tangents[0].ToFloatPtr());
        qglVertexAttribPointerARB(EVA_Binormal, 3, GL_FLOAT,         false, sizeof(idDrawVert), ac->tangents[1].ToFloatPtr());
        qglVertexAttribPointerARB(EVA_Color,    4, GL_UNSIGNED_BYTE, true,  sizeof(idDrawVert), ac->color);

        // this may cause RB_GL33_DrawInteraction to be exacuted multiple
        // times with different colors and images if the surface or light have multiple layers
        RB_CreateSingleDrawInteractions(surf, RB_GL33_DrawInteraction);
    }

    qglDisableVertexAttribArrayARB(EVA_Pos);
    qglDisableVertexAttribArrayARB(EVA_UV);
    qglDisableVertexAttribArrayARB(EVA_Normal);
    qglDisableVertexAttribArrayARB(EVA_Tangent);
    qglDisableVertexAttribArrayARB(EVA_Binormal);
    qglDisableVertexAttribArrayARB(EVA_Color);

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

    qglUseProgramObjectARB(0);
}


/*
==================
RB_ARB2_DrawInteractions
==================
*/
void RB_GL33_DrawInteractions() {
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
                qglScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
                    backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
                    backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
                    backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
            }
            qglClear(GL_STENCIL_BUFFER_BIT);
        } else {
            // no shadows, so no need to read or write the stencil buffer
            // we might in theory want to use GL_ALWAYS instead of disabling
            // completely, to satisfy the invarience rules
            qglStencilFunc(GL_ALWAYS, 128, 255);
        }

       /* if (r_useShadowVertexProgram.GetBool()) {
            qglEnable(GL_VERTEX_PROGRAM_ARB);
            qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, VPROG_STENCIL_SHADOW);
            RB_StencilShadowPass(vLight->globalShadows);
            RB_ARB2_CreateDrawInteractions(vLight->localInteractions);
            qglEnable(GL_VERTEX_PROGRAM_ARB);
            qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, VPROG_STENCIL_SHADOW);
            RB_StencilShadowPass(vLight->localShadows);
            RB_ARB2_CreateDrawInteractions(vLight->globalInteractions);
            qglDisable(GL_VERTEX_PROGRAM_ARB);	// if there weren't any globalInteractions, it would have stayed on
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

        qglStencilFunc(GL_ALWAYS, 128, 255);

        backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
        RB_GL33_CreateDrawInteractions(vLight->translucentInteractions);

        backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
    }

    // disable stencil shadow test
    qglStencilFunc(GL_ALWAYS, 128, 255);

    GL_SelectTexture(0);
    qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
}


/*
==================
RB_GL33_FillDepthBuffer
==================
*/
void RB_GL33_FillDepthBuffer(const drawSurf_t *surf) {
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
        qglTexGenfv(GL_S, GL_OBJECT_PLANE, plane.ToFloatPtr());
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

    qglUniform1iARB(prog.fulocs[EFU_TexDiffuse], 0);

    // set polygon offset if necessary
    if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
        qglEnable(GL_POLYGON_OFFSET_FILL);
        qglPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset());
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

    qglEnableVertexAttribArrayARB(EVA_Pos);
    qglEnableVertexAttribArrayARB(EVA_UV);

    idDrawVert *ac = (idDrawVert *)vertexCache.Position(tri->ambientCache);
    qglVertexAttribPointerARB(EVA_Pos, 3, GL_FLOAT, GL_FALSE, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
    qglVertexAttribPointerARB(EVA_UV,  2, GL_FLOAT, GL_FALSE, sizeof(idDrawVert), ac->st.ToFloatPtr());

    bool drawSolid = false;

    if (shader->Coverage() == MC_OPAQUE) {
        drawSolid = true;
    }

    // we may have multiple alpha tested stages
    if (shader->Coverage() == MC_PERFORATED) {
        // if the only alpha tested stages are condition register omitted,
        // draw a normal opaque surface
        bool	didDraw = false;

        qglEnable(GL_ALPHA_TEST);
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
            qglColor4fv(color);

            SetUniformVec4(prog.fulocs[EFU_DiffuseModifier], color);

            qglAlphaFunc(GL_GREATER, regs[pStage->alphaTestRegister]);

            // bind the texture
            pStage->texture.image->Bind();

            // set texture matrix and texGens
            //RB_PrepareStageTexturing(pStage, surf, ac);

            // draw it
            RB_DrawElementsWithCounters(tri);

            //RB_FinishStageTexturing(pStage, surf, ac);
        }
        qglDisable(GL_ALPHA_TEST);
        if (!didDraw) {
            drawSolid = true;
        }
    }

    // draw the entire surface solid
    if (drawSolid) {
        qglColor4fv(color);
        SetUniformVec4(prog.fulocs[EFU_DiffuseModifier], color);

        globalImages->whiteImage->Bind();

        // draw it
        RB_DrawElementsWithCounters(tri);
    }

    qglUseProgramObjectARB(0);

    qglDisableVertexAttribArrayARB(EVA_Pos);
    qglDisableVertexAttribArrayARB(EVA_UV);

    // reset polygon offset
    if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
        qglDisable(GL_POLYGON_OFFSET_FILL);
    }

    // reset blending
    if (shader->GetSort() == SS_SUBVIEW) {
        GL_State(GLS_DEPTHFUNC_LESS);
    }
}
