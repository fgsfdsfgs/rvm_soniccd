//
//  RenderDevice.c
//  rvm
//

#include "RenderDevice.h"
#include "GLIncludes.h"

#include <vitasdk.h>

#define NUM_TEXTURES 6
#define VOFS 120
const int TEXTURE_SIZE = 1024*1024*2;
int orthWidth;
int viewWidth;
int viewHeight;
float viewAspect;
int bufferWidth;
int bufferHeight;
int highResMode;
int virtualX;
int virtualY;
int virtualWidth;
int virtualHeight;
GLuint gfxTextureID[NUM_TEXTURES];
GLuint framebufferId;
GLuint fbTextureId;
float screenVerts[] = {
    0, 0, 0,
    6400, 0, 0,
    0, 3844, 0,
    6400, 0, 0,
    0, 3844, 0,
    6400, 3844, 0,
};
float fbTexVerts[] = {
    -1024, -1024 + VOFS,
    0, -1024 + VOFS,
    -1024, VOFS,
    0, -1024 + VOFS,
    -1024, VOFS,
    0, VOFS,
};
float pureLight[] = {
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
};

// VITA: we're gonna have to use "native" replacements for drawing functions

static struct AttrBuf {
    GLuint size;
    GLenum type;
    GLuint stride;
    const void *ptr;
    GLboolean dirty;
} vglAttr[3];

static inline void GL_VertexPointer(GLuint size, GLenum type, GLuint stride, const void *ptr)
{
    vglAttr[0].size = size;
    vglAttr[0].type = GL_FLOAT; // can't do shorts with vgl*Pointer
    vglAttr[0].stride = stride;
    vglAttr[0].ptr = ptr;
    vglAttr[0].dirty = GL_TRUE;
}

static inline void GL_ColorPointer(GLuint size, GLenum type, GLuint stride, const void *ptr)
{
    vglAttr[1].size = size;
    vglAttr[1].type = type;
    vglAttr[1].stride = stride;
    vglAttr[1].ptr = ptr;
    vglAttr[1].dirty = GL_TRUE;
}

static inline void GL_TexCoordPointer(GLuint size, GLenum type, GLuint stride, const void *ptr)
{
    vglAttr[2].size = size;
    vglAttr[2].type = GL_FLOAT; // can't do shorts with vgl*Pointer
    vglAttr[2].stride = stride;
    vglAttr[2].ptr = ptr;
    vglAttr[2].dirty = GL_TRUE;
}

static inline void FlushAttribs(const GLuint count)
{
    if (vglAttr[0].dirty) {
        vglVertexPointer(vglAttr[0].size, vglAttr[0].type, vglAttr[0].stride, count, vglAttr[0].ptr);
        vglAttr[0].dirty = GL_FALSE;
    }
    if (vglAttr[1].dirty) {
        vglColorPointer(vglAttr[1].size, vglAttr[1].type, vglAttr[1].stride, count, vglAttr[1].ptr);
        vglAttr[1].dirty = GL_FALSE;
    }
    if (vglAttr[2].dirty) {
        vglTexCoordPointer(vglAttr[2].size, vglAttr[2].type, vglAttr[2].stride, count, vglAttr[2].ptr);
        vglAttr[2].dirty = GL_FALSE;
    }
}

static inline void GL_DrawElements(GLenum mode, GLuint count, GLenum type, const void *idx, GLuint total)
{
    FlushAttribs(total);
    vglIndexPointer(GL_SHORT, 0, count, idx);
    vglDrawObjects(mode, count, GL_TRUE);
}

static inline void GL_DrawArrays(GLenum mode, GLuint start, GLuint count)
{
    // this is only ever called with (GL_TRIANGLES, 0, 6)
    static const uint16_t seqidx[] = { 0, 1, 2, 3, 4, 5 };
    GL_DrawElements(mode, count, GL_SHORT, seqidx, count);
}

static inline void GL_BindFramebuffer(GLenum target, GLuint fb)
{
    // need to finish rendering the current framebuffer before switching over
    // HACK: vitaGL doesn't expose anything to end/restart scenes, so we make do
    extern SceGxmContext *gxm_context;
    sceGxmEndScene(gxm_context, NULL, NULL);
    glBindFramebuffer(target, fb);
    // restart rendering
    vglStartRendering();
}

void HandleGlError(){
    GLenum boo = glGetError();
    if(boo != GL_NO_ERROR){
        if (boo == GL_INVALID_OPERATION){
            printf("Invalid operation\n");
        }
#if !defined(WINDOWS) && !defined(LINUX) && !defined(__VITA__)
        else if(boo == GL_INVALID_FRAMEBUFFER_OPERATION){
            boo = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if(boo == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT){
                printf("Framebuffer attachment missing!");
            }
            else{
                printf("Invalid framebuffer operation\n");
            }
        }
#endif
        else{
            printf("An error! %08x\n", boo);
        }
    }
}


void InitRenderDevice()
{
    Init_GraphicsSystem();
    highResMode = 0;
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    
    glLoadIdentity();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GraphicsSystem_SetupPolygonLists();
    
    for (int i = 0; i < NUM_TEXTURES; i++)
    {
        glGenTextures(1, &gfxTextureID[i]);
        glBindTexture(GL_TEXTURE_2D, gfxTextureID[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, texBuffer);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(0.0009765625f, 0.0009765625f, 1.0f); //1.0 / 1024.0. Allows for texture locations in pixels instead of from 0.0 to 1.0
    glMatrixMode(GL_PROJECTION);
    
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glClear(GL_COLOR_BUFFER_BIT);
    
    framebufferId = 0;
    fbTextureId = 0;
}
void RenderDevice_UpdateHardwareTextures()
{
    GraphicsSystem_SetActivePalette(0, 0, 240);
    GraphicsSystem_UpdateTextureBufferWithTiles();
    GraphicsSystem_UpdateTextureBufferWithSortedSprites();
    
    glBindTexture(GL_TEXTURE_2D, gfxTextureID[0]);
    HandleGlError();
    
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, texBuffer);
    HandleGlError();
    
    for (uint8_t b = 1; b < NUM_TEXTURES; b += 1)
    {
        GraphicsSystem_SetActivePalette(b, 0, 240);
        GraphicsSystem_UpdateTextureBufferWithTiles();
        GraphicsSystem_UpdateTextureBufferWithSprites();

        glBindTexture(GL_TEXTURE_2D, gfxTextureID[b]);
        HandleGlError();
        
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, texBuffer);
        HandleGlError();
    }
    GraphicsSystem_SetActivePalette(0, 0, 240);
}
void RenderDevice_SetScreenDimensions(int width, int height)
{
    touchWidth = width;
    touchHeight = height;
    viewWidth = touchWidth;
    viewHeight = touchHeight;
    float num = (float)viewWidth / (float)viewHeight;
    num *= 240.0f;
    bufferWidth = (int)num;
    bufferWidth += 8;
    bufferWidth = bufferWidth >> 4 << 4;
    /*if (bufferWidth > 400)
    {
        bufferWidth = 400;
    }*/
    viewAspect = 0.75f;
    if (viewHeight >= 480.0)
    {
        HQ3DFloorEnabled = true;
    }
    else
    {
        HQ3DFloorEnabled = false;
    }
    GraphicsSystem_SetScreenRenderSize(bufferWidth, bufferWidth);
    if (viewHeight >= 480)
    {
        bufferWidth *= 2;
        bufferHeight = 480;
    }
    else
    {
        bufferHeight = 240;
    }
    orthWidth = SCREEN_XSIZE * 16;
    
    //You should never change screen dimensions, so we should not need to do this, but I'll do it anyway.
    if(framebufferId > 0){
        glDeleteFramebuffers(1, &framebufferId);
    }
    if(fbTextureId > 0){
        glDeleteTextures(1, &fbTextureId);
    }
    //Setup framebuffer texture
    glGenFramebuffers(1, &framebufferId);
    GL_BindFramebuffer(GL_FRAMEBUFFER, framebufferId);
    glGenTextures(1, &fbTextureId);
    glBindTexture(GL_TEXTURE_2D, fbTextureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewWidth, viewHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fbTextureId, 0);
    GL_BindFramebuffer(GL_FRAMEBUFFER, 0);
    int newWidth = width * 8;
    int newHeight = (height * 8)+4;
    
    screenVerts[1 * 3 + 0] = newWidth;
    screenVerts[3 * 3 + 0] = newWidth;
    screenVerts[5 * 3 + 0] = newWidth;
    screenVerts[2 * 3 + 1] = newHeight;
    screenVerts[4 * 3 + 1] = newHeight;
    screenVerts[5 * 3 + 1] = newHeight;
    RenderDevice_ScaleViewport(width, height);
}

void RenderDevice_ScaleViewport(int width, int height){
    virtualWidth = width;
    virtualHeight = height;
    virtualX = 0;
    virtualY = 0;
    
    float virtualAspect = (float)width / height;
    float realAspect = (float)viewWidth / viewHeight;
    if(virtualAspect < realAspect){
        virtualHeight = viewHeight * ((float)width/viewWidth);
        virtualY = (height - virtualHeight) >> 1;
    }
    else{
        virtualWidth = viewWidth * ((float)height/viewHeight);
        virtualX = (width - virtualWidth) >> 1;
    }
}

void CalcPerspective(float fov, float aspectRatio, float nearPlane, float farPlane){
    GLfloat matrix[16];
    float w = 1.0 / tanf(fov * 0.5f);
    float h = 1.0 / (w*aspectRatio);
    float q = (nearPlane+farPlane)/(farPlane - nearPlane);
    
    matrix[0] = w;
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;
    
    matrix[4] = 0;
    matrix[5] = h/2;
    matrix[6] = 0;
    matrix[7] = 0;
    
    matrix[8] = 0;
    matrix[9] = 0;
    matrix[10] = q;
    matrix[11] = 1.0;
    
    matrix[12] = 0;
    matrix[13] = 0;
    matrix[14] = (((farPlane*-2.0f)*nearPlane)/(farPlane-nearPlane));
    matrix[15] = 0;
    
    glMultMatrixf(matrix);
}

void RenderDevice_FlipScreen()
{
    GL_BindFramebuffer(GL_FRAMEBUFFER, framebufferId);
    
    glLoadIdentity();
    HandleGlError();
    
    glOrtho(0, orthWidth, 3844.0f, 0.0, 0.0f, 100.0f);
    if(texPaletteNum >= NUM_TEXTURES){
        //This is a stage that requires the software renderer for correct water palettes.
        //Only happens if using the PC / Console Data.rsdk file
        glBindTexture(GL_TEXTURE_2D, gfxTextureID[texPaletteNum % NUM_TEXTURES]);
    }
    else{
        glBindTexture(GL_TEXTURE_2D, gfxTextureID[texPaletteNum]);
    }
    glEnableClientState(GL_COLOR_ARRAY);
    HandleGlError();
    if(render3DEnabled){
        GL_VertexPointer(VTX2D_COUNT, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].position);
        GL_TexCoordPointer(2, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].texCoord);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, VTX2D_STRIDE, &gfxPolyList[0].color);
        GL_DrawElements(GL_TRIANGLES, gfxIndexSizeOpaque, GL_UNSIGNED_SHORT, gfxPolyListIndex, gfxIndexSizeOpaque);
        glEnable(GL_BLEND);
        HandleGlError();
        
        glViewport(0, 0, viewWidth, viewHeight);
        glPushMatrix();
        glLoadIdentity();
        CalcPerspective(1.8326f, viewAspect, 0.1f, 1000.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
       
        glScalef(1.0f, -1.0f, -1.0f);
        glRotatef(180.0f + floor3DAngle, 0, 1.0f, 0);
        glTranslatef(floor3DPos.X, floor3DPos.Y, floor3DPos.Z);
        GL_VertexPointer(VTX3D_COUNT, GL_FLOAT, VTX3D_STRIDE, &polyList3D[0].position);
        GL_TexCoordPointer(2, GL_FLOAT, VTX3D_STRIDE, &polyList3D[0].texCoord);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, VTX3D_STRIDE, &polyList3D[0].color);
        GL_DrawElements(GL_TRIANGLES, indexSize3D, GL_UNSIGNED_SHORT, gfxPolyListIndex, indexSize3D);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        
        glViewport(0, 0, bufferWidth, bufferHeight);
        glPopMatrix();
        HandleGlError();
        
        int numBlendedGfx = (int)(gfxIndexSize - gfxIndexSizeOpaque);
        GL_VertexPointer(VTX2D_COUNT, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].position);
        GL_TexCoordPointer(2, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].texCoord);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, VTX2D_STRIDE, &gfxPolyList[0].color);
        GL_DrawElements(GL_TRIANGLES, numBlendedGfx, GL_UNSIGNED_SHORT, &gfxPolyListIndex[gfxIndexSizeOpaque], numBlendedGfx + gfxIndexSizeOpaque);
        HandleGlError();
    }
    else{
        GL_VertexPointer(VTX2D_COUNT, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].position);
        GL_TexCoordPointer(2, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].texCoord);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, VTX2D_STRIDE, &gfxPolyList[0].color);
        GL_DrawElements(GL_TRIANGLES, gfxIndexSizeOpaque, GL_UNSIGNED_SHORT, gfxPolyListIndex, gfxIndexSizeOpaque);
        HandleGlError();
        
        int numBlendedGfx = (int)(gfxIndexSize - gfxIndexSizeOpaque);
        
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        GL_VertexPointer(VTX2D_COUNT, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].position);
        GL_TexCoordPointer(2, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].texCoord);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, VTX2D_STRIDE, &gfxPolyList[0].color);
        GL_DrawElements(GL_TRIANGLES, numBlendedGfx, GL_UNSIGNED_SHORT, &gfxPolyListIndex[gfxIndexSizeOpaque], numBlendedGfx + gfxIndexSizeOpaque);
        HandleGlError();
    }
    
    glDisable(GL_BLEND);
    
    //Render the framebuffer now
    GL_BindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glViewport(virtualX, virtualY, virtualWidth, virtualHeight);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fbTextureId);
    GL_VertexPointer(3, GL_FLOAT, 0, screenVerts);
    GL_TexCoordPointer(2, GL_FLOAT, 0, fbTexVerts);
    GL_ColorPointer(4, GL_FLOAT, 0, pureLight);
    GL_DrawArrays(GL_TRIANGLES, 0, 6);

    glDisableClientState(GL_COLOR_ARRAY);

    glViewport(0, 0, bufferWidth, bufferHeight);

    glFinish();
}

void RenderDevice_FlipScreenHRes()
{
    GL_BindFramebuffer(GL_FRAMEBUFFER, framebufferId);
    
    glLoadIdentity();
    
    glOrtho(0, orthWidth, 3844.0f, 0.0, 0.0f, 100.0f);
    glViewport(0, 0, bufferWidth, bufferHeight);
    glBindTexture(GL_TEXTURE_2D, gfxTextureID[texPaletteNum]);
    glDisable(GL_BLEND);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glEnableClientState(GL_COLOR_ARRAY);
    
    GL_VertexPointer(VTX2D_COUNT, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].position);
    GL_TexCoordPointer(2, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].texCoord);
    GL_ColorPointer(4, GL_UNSIGNED_BYTE, VTX2D_STRIDE, &gfxPolyList[0].color);
    GL_DrawElements(GL_TRIANGLES, gfxIndexSizeOpaque, GL_UNSIGNED_SHORT, gfxPolyListIndex, gfxIndexSizeOpaque);
    
    HandleGlError();
    
    glEnable(GL_BLEND);
    int numBlendedGfx = (int)((gfxIndexSize) - (gfxIndexSizeOpaque));
    GL_VertexPointer(VTX2D_COUNT, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].position);
    GL_TexCoordPointer(2, GL_FLOAT, VTX2D_STRIDE, &gfxPolyList[0].texCoord);
    GL_ColorPointer(4, GL_UNSIGNED_BYTE, VTX2D_STRIDE, &gfxPolyList[0].color);
    GL_DrawElements(GL_TRIANGLES, numBlendedGfx, GL_UNSIGNED_SHORT, &gfxPolyListIndex[gfxIndexSizeOpaque], numBlendedGfx + gfxIndexSizeOpaque);
    
    HandleGlError();
    
    glDisable(GL_BLEND);
    
    //Render the framebuffer now
    GL_BindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glViewport(virtualX, virtualY, virtualWidth, virtualHeight);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fbTextureId);
    GL_VertexPointer(3, GL_FLOAT, 0, screenVerts);
    GL_TexCoordPointer(2, GL_FLOAT, 0, fbTexVerts);
    GL_ColorPointer(4, GL_FLOAT, 0, pureLight);
    GL_DrawArrays(GL_TRIANGLES, 0, 6);

    glDisableClientState(GL_COLOR_ARRAY);

    glViewport(0, 0, bufferWidth, bufferHeight);

    glFinish();
}
