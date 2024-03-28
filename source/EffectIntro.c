/**
* "invites u 2 nordlicht 2023"
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "Tools.h"
#include "SVatGSegmented20.h"

// Global data: Matrices
static int uLocProjection;
static int uLocModelview; 
static C3D_Mtx projection;
 
// Global data: Lighting
static C3D_LightEnv lightEnv;
static C3D_Light light;
static C3D_LightLut lutPhong;
static C3D_LightLut lutShittyFresnel;

// Boooones
static int uLocBone[21];

static C3D_Tex texIntro;
static C3D_Tex texEmpty;
static C3D_Tex texSky;
static C3D_TexCube texSkyCube;
static C3D_Tex texFg;
static fbxBasedObject models[3];
static fbxBasedObject camProxy;

// rot + zoom + skybox
static const struct sync_track* syncZoom;
static const struct sync_track* syncPanX;
static const struct sync_track* syncPanY;
static const struct sync_track* syncSky;
static const struct sync_track* syncScroll;
static const struct sync_track* syncDepthSep;

// Scrolltext
static Pixel* scrollPixels;
static Bitmap scroller;
static C3D_Tex scrollTex;

#define SCROLL_TEXT "              hi everybody, halcy on the keys! sorry, it's another scroller from @, once again on the 3DS! this time, we are showing off this nice model of the OwOhaus & by Violet, set to music once again from Saga Musix, and the code is as usual by halcy!" \
                    "                                                                                     >                                                                                       }" \
                    "                                                                                     @                                                                                       %                                                                                       " \
                    "              The scrolltext message. Das muss so! -- Saga Musix                                                  apologies if a raccoon ate your ticket, we cannot offer a refund but please accept compensation in the form of a hug -- Violet" \
                    "                                                                                     we would like to greet at this time: 5711 [ alcatraz [ aldroid [ Arsenic [ bacter [ DDK [ Desire [ dojoe [ EOS [ epoqe [ Furry Trash Group [ Gasman [ jeenio [ ICUP [ K2 [ mercury [ Molive [ netpoet [ oxyron [ Poo-Brain [ rabenauge [ RBBS [ Rift [ Slipstream [ Suricrasia Online [ TiTAN [ Tobach [ truck [ TUHB [ T$ [ Wursthupe [ xq [ and everyone else at deadline! % % %                   Wishing you all a great party, halcy out~" \
                    "                                                                                       "
int textLen = 0;

void effectIntroInit() {
    // Text info
    textLen = WidthOfSimpleString((Font*)&SVatGSegmented, SCROLL_TEXT);
    
    // Prep general info: Shader (precompiled in main for important ceremonial reasons)
    C3D_BindProgram(&shaderProgramBones);

    // Prep general info: Uniform locs 
    uLocProjection = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, "projection");
    uLocModelview = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, "modelView");
    
    // Prep general info: Bones 
    char boneName[255];
    for(int i = 0; i < 21; i++) {
        sprintf(boneName, "bone%02d", i);
        uLocBone[i] = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, boneName);
    }

    // Prep scroller
    C3D_TexInit(&scrollTex, 128, 128, GPU_RGBA8);
    scrollPixels = (Pixel*)linearAlloc(128 * 128 * sizeof(Pixel));
    InitialiseBitmap(&scroller, 128, 128, BytesPerRowForWidth(128), scrollPixels);
    C3D_TexSetFilter(&scrollTex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&scrollTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // Load a model
    loadTexture(&texIntro, NULL, "romfs:/tex_intro.bin");
    loadTexture(&texEmpty, NULL, "romfs:/tex_empty.bin");
    C3D_TexSetFilter(&texEmpty, GPU_NEAREST, GPU_NEAREST);

    loadTexture(&texSky, &texSkyCube, "romfs:/sky_cube.bin");
    loadTexture(&texFg, NULL, "romfs:/tex_fg1.bin");
    models[0] = loadFBXObject("romfs:/obj_scroller_plug.vbo", &texIntro, "intro:plug");

    camProxy = loadFBXObject("romfs:/obj_scroller_cam_cube.vbo", &texIntro, "intro:cam");

    // More sync
    syncZoom = sync_get_track(rocket, "intro:cam:zoom");
    syncPanX = sync_get_track(rocket, "intro:cam:panx");
    syncPanY = sync_get_track(rocket, "intro:cam:pany");
    syncSky = sync_get_track(rocket, "intro:skyrot");
    syncScroll = sync_get_track(rocket, "intro:text:scroll");
    syncDepthSep = sync_get_track(rocket, "global:depthsep");

     // Begin frame and bind shader
    C3D_BindProgram(&shaderProgramBones);

    // Set up lighting
    C3D_LightEnvInit(&lightEnv);
    C3D_LightEnvBind(&lightEnv);
    
    LightLut_Phong(&lutPhong, 1000.0);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN, false, &lutPhong);
    
    // Add funny edge lighting that makes 3D pop
    float lightStrengthFresnel = 0.35;
    LightLut_FromFunc(&lutShittyFresnel, badFresnel, lightStrengthFresnel, false);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_FR, GPU_LUTINPUT_NV, false, &lutShittyFresnel);
    C3D_LightEnvFresnel(&lightEnv, GPU_PRI_SEC_ALPHA_FRESNEL);
    
    // Basic shading with diffuse + specular
    C3D_FVec lightVec = FVec4_New(0.0, 0.0, 0.0, 1.0);
    C3D_LightInit(&light, &lightEnv);
    C3D_LightColor(&light, 1.0, 1.0, 1.0);
    C3D_LightPosition(&light, &lightVec);

    C3D_Material lightMaterial = {
        { 0.2, 0.2, 0.2 }, //ambient
        { 0.1,  0.1,  0.2 }, //diffuse
        { 0.1f, 0.1f, 0.2f }, //specular0
        { 0.0f, 0.0f, 0.0f }, //specular1
        { 0.0f, 0.0f, 0.0f }, //emission
    };
    C3D_LightEnvMaterial(&lightEnv, &lightMaterial);
}

// TODO: Split out shade setup
static void drawModel(fbxBasedObject* model, float row) {
    if(model == &models[0]) {
        // Set up attribute info
        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0 = position (float3)
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 1); // v1 = bone indices (float2)
        AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 1); // v2 = bone weights (float2)
        AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 3); // v3 = normal (float3)
        AttrInfo_AddLoader(attrInfo, 4, GPU_FLOAT, 2); // v4 = texcoords (float2)

        // Begin frame and bind shader
        C3D_BindProgram(&shaderProgramBones);

        // Set up lighting
        C3D_LightEnvBind(&lightEnv);
    }

     // Add VBO to draw buffer
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, (void*)model->vbo, sizeof(vertex_rigged), 5, 0x43210);

    // Bind texture
    C3D_TexBind(0, model->tex);    

    // Set up texture combiners
    if(model != &models[1]) {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_FRAGMENT_PRIMARY_COLOR, 0);
        C3D_TexEnvOpRgb(env, 0, 0, 0);
        C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
        
        env = C3D_GetTexEnv(1); 
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_RGB, GPU_FRAGMENT_SECONDARY_COLOR, GPU_PREVIOUS, 0);
        C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_ALPHA, 0, 0);
        C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);

        env = C3D_GetTexEnv(2);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Alpha, GPU_CONSTANT, GPU_PREVIOUS, 0);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
    }
    else {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PREVIOUS, 0);
        C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
        
        env = C3D_GetTexEnv(1);
        C3D_TexEnvInit(env);

        env = C3D_GetTexEnv(2);
        C3D_TexEnvInit(env);
    }

    // GPU state for normal drawing with transparency 
    if(model != &models[1]) {
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    }
    else {
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_COLOR);
    }
    //C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
 
    // Actual drawcall
    C3D_DrawArrays(GPU_TRIANGLES, 0, model->vertCount);
}

void effectIntroRender(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod) {
    // Update bone mats
    setBonesFromSync(&models[0], uLocBone, row, 1, 2);
    //setBonesFromSync(&models[1], uLocBone, row, 0, 8);

    // Update scroll texture
    float sshift = sync_get_val(syncScroll, row);
    FillBitmap(&scroller, RGBAf(0.0, 0.0, 0.0, 0.0));
    for(int i = 0; i < 5; i++) {
        DrawSimpleString(&scroller, (Font*)&SVatGSegmented, -i * 128 - sshift, 22 * i, 0, SCROLL_TEXT);
    }

    GSPGPU_FlushDataCache(scrollPixels, 128 * 128 * sizeof(Pixel));
    GX_DisplayTransfer((u32*)scrollPixels, GX_BUFFER_DIM(128, 128), (u32*)scrollTex.data, GX_BUFFER_DIM(128, 128), TEXTURE_TRANSFER_FLAGS);
    //gspWaitForPPF();



    // Frame starts (TODO pull out?)   
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    
    // Get some sync vals
    float syncSkyVal = sync_get_val(syncSky, row);
    float syncZoomVal = sync_get_val(syncZoom, row);
    float syncPanXVal = sync_get_val(syncPanX, row);
    float syncPanYVal = sync_get_val(syncPanY, row);
    float syncDepthSepVal = sync_get_val(syncDepthSep, row) + 1.0;
    iod *= syncDepthSepVal;

    // Send modelview 
    C3D_Mtx baseview;
    Mtx_Identity(&baseview);
    Mtx_RotateZ(&baseview, M_PI, true);
    Mtx_RotateX(&baseview, -M_PI / 2, true);
    Mtx_RotateY(&baseview, M_PI, true);
    
    C3D_Mtx camMat;
    getBoneMat(&camProxy, row, &camMat, 0);
    Mtx_Translate(&camMat, syncPanXVal, syncZoomVal, syncPanYVal, true);
    Mtx_Inverse(&camMat);

    C3D_Mtx modelview;
    Mtx_Multiply(&modelview, &baseview, &camMat);
    C3D_Mtx modelviewHaus = modelview;

    C3D_Mtx skyview;
    Mtx_Multiply(&skyview, &baseview, &camMat);
    Mtx_RotateZ(&skyview, syncSkyVal * M_PI, true);

     // Left eye 
    C3D_FrameDrawOn(targetLeft);  
    C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0xffF000FF, 0);
    
    // Uniform setup
    Mtx_PerspStereoTilt(&projection, 30.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, -iod,  7.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);

    // Dispatch drawcalls
    //skyboxCubeImmediate(&texSky, 2.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection); 
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &skyview);
    drawModel(&models[0], row);
    //drawModel(&models[2], row);
    //C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);
    //drawModel(&models[1], row);

    // Do fading and fg
    fullscreenQuad(texFg, 0.0, 1.0);
    fade();

    // Right eye?
    if(iod > 0.0) {
        C3D_FrameDrawOn(targetRight); 
        C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, 0x00ff00FF, 0); 
        
        // Uniform setup
        Mtx_PerspStereoTilt(&projection, 30.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, iod, 7.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);

        // Dispatch drawcalls
        skyboxCubeImmediate(&texSky, 1000.0f, vec3(0.0f, 0.0f, 0.0f), &skyview, &projection); 
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelviewHaus);
        drawModel(&models[0], row);
        //drawModel(&models[2], row);
        //C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &modelview);
        //drawModel(&models[1], row);

        // Do fading and fg
        fullscreenQuad(texFg, 0.0, 1.0);
        fade();
    } 

    // Swap
    C3D_FrameEnd(0);

}

void effectIntroExit() {
    for(int i = 0; i < 1; i++) {
        freeFBXObject(&models[i]);
    }
    freeFBXObject(&camProxy);
    C3D_TexDelete(&texIntro);
    C3D_TexDelete(&texSky);
    C3D_TexDelete(&texFg);
}
