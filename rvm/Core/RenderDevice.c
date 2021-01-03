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
void *gfxTexturePtr[NUM_TEXTURES];
GLuint framebufferId;
GLuint fbTextureId;
struct Vector3 *screenVerts;
struct Vector2 *fbTexVerts;
struct Color *pureLight;

// VITA: we're gonna have to use "native" replacements for drawing functions

static uint16_t *gfxSeqIdx;

static inline void GL_VertexPointer(GLuint size, GLenum type, GLuint stride, const void *ptr)
{
    vglVertexPointerMapped(ptr);
}

static inline void GL_ColorPointer(GLuint size, GLenum type, GLuint stride, const void *ptr)
{
    vglColorPointerMapped(type, ptr);
}

static inline void GL_TexCoordPointer(GLuint size, GLenum type, GLuint stride, const void *ptr)
{
    vglTexCoordPointerMapped(ptr);
}

static inline void GL_DrawElements(GLenum mode, GLuint count, const void *idx)
{
    vglIndexPointerMapped(idx);
    vglDrawObjects(mode, count, GL_TRUE);
}

static inline void GL_DrawArrays(GLenum mode, GLuint start, GLuint count)
{
    // this is only ever called with (GL_TRIANGLES, 0, 6)
    GL_DrawElements(mode, count, gfxSeqIdx);
}

static inline void GL_BindFramebuffer(GLenum target, GLuint fb)
{
    // need to finish rendering the current framebuffer before switching over
    vglStopRenderingInit();
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
    
    // VITA: allocate draw buffers and index buffers
    
    float *vertBuf = vglAlloc((3 + 2 + 1) * (VERTEX_LIMIT + VERTEX3D_LIMIT) * sizeof(float), VGL_MEM_RAM);
    gfxPolyList.position = (struct Vector3 *)vertBuf; vertBuf += VERTEX_LIMIT * 3;
    gfxPolyList.texCoord = (struct Vector2 *)vertBuf; vertBuf += VERTEX_LIMIT * 2;
    gfxPolyList.color    = (struct   Color *)vertBuf; vertBuf += VERTEX_LIMIT;
    polyList3D.position  = (struct Vector3 *)vertBuf; vertBuf += VERTEX3D_LIMIT * 3;
    polyList3D.texCoord  = (struct Vector2 *)vertBuf; vertBuf += VERTEX3D_LIMIT * 2;
    polyList3D.color     = (struct   Color *)vertBuf; vertBuf += VERTEX3D_LIMIT;
    
    // indices never change, so they can go into vram
    gfxPolyListIndex = vglAlloc(INDEX_LIMIT * sizeof(uint16_t), VGL_MEM_VRAM);
    
    // allocate sequential indices for DrawArrays
    gfxSeqIdx = vglAlloc(16 * sizeof(uint16_t), VGL_MEM_VRAM);
    for (uint32_t i = 0; i < 16; ++i)
        gfxSeqIdx[i] = i;
    
    // allocate and init draw buffers for the screen quad
    vertBuf = vglAlloc((3 + 2 + 1) * 6 * sizeof(float), VGL_MEM_VRAM);
    screenVerts = (struct Vector3 *)vertBuf; vertBuf += 6 * 3;
    fbTexVerts  = (struct Vector2 *)vertBuf; vertBuf += 6 * 2;
    pureLight   = (struct   Color *)vertBuf; vertBuf += 6;
    
    screenVerts[0] = (struct Vector3){    0,    0, 0 };
    screenVerts[1] = (struct Vector3){ 6400,    0, 0 };
    screenVerts[2] = (struct Vector3){    0, 3844, 0 };
    screenVerts[3] = (struct Vector3){ 6400,    0, 0 };
    screenVerts[4] = (struct Vector3){    0, 3844, 0 };
    screenVerts[5] = (struct Vector3){ 6400, 3844, 0 };
    
    fbTexVerts[0] = (struct Vector2){ -1024, -1024 + VOFS };
    fbTexVerts[1] = (struct Vector2){     0, -1024 + VOFS };
    fbTexVerts[2] = (struct Vector2){ -1024,         VOFS };
    fbTexVerts[3] = (struct Vector2){     0, -1024 + VOFS };
    fbTexVerts[4] = (struct Vector2){ -1024,         VOFS };
    fbTexVerts[5] = (struct Vector2){     0,         VOFS };
    
    memset(pureLight, 0xFF, sizeof(struct Color) * 6);
    
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
        gfxTexturePtr[i] = vglGetTexDataPointer(GL_TEXTURE_2D);
    }
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(0.0009765625f, 0.0009765625f, 1.0f); //1.0 / 1024.0. Allows for texture locations in pixels instead of from 0.0 to 1.0
    glMatrixMode(GL_PROJECTION);
    
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glClear(GL_COLOR_BUFFER_BIT);
    
    framebufferId = 0;
    fbTextureId = 0;
}
void RenderDevice_UpdateHardwareTextures()
{
    // VITA: write updates directly to textures instead of copying them later
    texBuffer = gfxTexturePtr[0];
    
    GraphicsSystem_SetActivePalette(0, 0, 240);
    GraphicsSystem_UpdateTextureBufferWithTiles();
    GraphicsSystem_UpdateTextureBufferWithSortedSprites();
    
    for (uint8_t b = 1; b < NUM_TEXTURES; b += 1)
    {
        texBuffer = gfxTexturePtr[b];
        GraphicsSystem_SetActivePalette(b, 0, 240);
        GraphicsSystem_UpdateTextureBufferWithTiles();
        GraphicsSystem_UpdateTextureBufferWithSprites();
    }
    
    GraphicsSystem_SetActivePalette(0, 0, 240);
    texBuffer = localTexBuffer;
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
    
    screenVerts[1].X = newWidth;
    screenVerts[3].X = newWidth;
    screenVerts[5].X = newWidth;
    screenVerts[2].Y = newHeight;
    screenVerts[4].Y = newHeight;
    screenVerts[5].Y = newHeight;
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
    
    if(render3DEnabled){
        GL_VertexPointer(3, GL_FLOAT, 0, &gfxPolyList.position[0]);
        GL_TexCoordPointer(2, GL_FLOAT, 0, &gfxPolyList.texCoord[0]);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, &gfxPolyList.color[0]);
        GL_DrawElements(GL_TRIANGLES, gfxIndexSizeOpaque, gfxPolyListIndex);
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
        GL_VertexPointer(3, GL_FLOAT, 0, &polyList3D.position[0]);
        GL_TexCoordPointer(2, GL_FLOAT, 0, &polyList3D.texCoord[0]);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, &polyList3D.color[0]);
        GL_DrawElements(GL_TRIANGLES, indexSize3D, gfxPolyListIndex);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        
        glViewport(0, 0, bufferWidth, bufferHeight);
        glPopMatrix();
        HandleGlError();
        
        int numBlendedGfx = (int)(gfxIndexSize - gfxIndexSizeOpaque);
        GL_VertexPointer(3, GL_FLOAT, 0, &gfxPolyList.position[0]);
        GL_TexCoordPointer(2, GL_FLOAT, 0, &gfxPolyList.texCoord[0]);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, &gfxPolyList.color[0]);
        GL_DrawElements(GL_TRIANGLES, numBlendedGfx, &gfxPolyListIndex[gfxIndexSizeOpaque]);
        glDisable(GL_BLEND);
        HandleGlError();
    }
    else{
        GL_VertexPointer(3, GL_FLOAT, 0, &gfxPolyList.position[0]);
        GL_TexCoordPointer(2, GL_FLOAT, 0, &gfxPolyList.texCoord[0]);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, &gfxPolyList.color[0]);
        GL_DrawElements(GL_TRIANGLES, gfxIndexSizeOpaque, gfxPolyListIndex);
        HandleGlError();
        
        int numBlendedGfx = (int)(gfxIndexSize - gfxIndexSizeOpaque);
        
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        GL_VertexPointer(3, GL_FLOAT, 0, &gfxPolyList.position[0]);
        GL_TexCoordPointer(2, GL_FLOAT, 0, &gfxPolyList.texCoord[0]);
        GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, &gfxPolyList.color[0]);
        GL_DrawElements(GL_TRIANGLES, numBlendedGfx, &gfxPolyListIndex[gfxIndexSizeOpaque]);
        glDisable(GL_BLEND);
        HandleGlError();
    }
    
    //Render the framebuffer now
    GL_BindFramebuffer(GL_FRAMEBUFFER, 0);
    
    glViewport(virtualX, virtualY, virtualWidth, virtualHeight);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fbTextureId);
    GL_VertexPointer(3, GL_FLOAT, 0, screenVerts);
    GL_TexCoordPointer(2, GL_FLOAT, 0, fbTexVerts);
    GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, pureLight);
    GL_DrawArrays(GL_TRIANGLES, 0, 6);

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

    GL_VertexPointer(3, GL_FLOAT, 0, &gfxPolyList.position[0]);
    GL_TexCoordPointer(2, GL_FLOAT, 0, &gfxPolyList.texCoord[0]);
    GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, &gfxPolyList.color[0]);
    GL_DrawElements(GL_TRIANGLES, gfxIndexSizeOpaque, gfxPolyListIndex);

    HandleGlError();

    glEnable(GL_BLEND);
    int numBlendedGfx = (int)((gfxIndexSize) - (gfxIndexSizeOpaque));
    GL_VertexPointer(3, GL_FLOAT, 0, &gfxPolyList.position[0]);
    GL_TexCoordPointer(2, GL_FLOAT, 0, &gfxPolyList.texCoord[0]);
    GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, &gfxPolyList.color[0]);
    GL_DrawElements(GL_TRIANGLES, numBlendedGfx, &gfxPolyListIndex[gfxIndexSizeOpaque]);
    glDisable(GL_BLEND);

    HandleGlError();

    //Render the framebuffer now
    GL_BindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(virtualX, virtualY, virtualWidth, virtualHeight);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fbTextureId);
    GL_VertexPointer(3, GL_FLOAT, 0, screenVerts);
    GL_TexCoordPointer(2, GL_FLOAT, 0, fbTexVerts);
    GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, pureLight);
    GL_DrawArrays(GL_TRIANGLES, 0, 6);

    glViewport(0, 0, bufferWidth, bufferHeight);

    glFinish();
}
