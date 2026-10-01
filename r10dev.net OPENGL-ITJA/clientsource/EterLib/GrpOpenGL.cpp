#include "StdAfx.h"
#include "GrpOpenGL.h"

#ifdef ENABLE_OPENGL

#ifdef ANDROID
#include <android/log.h>
#include <EGL/egl.h>
#define LOG_TAG "Metin2Mobile"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif

void* c_dfDIKeyboard = NULL;

namespace {
    const char* vShaderSource = 
        "#version 300 es\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec2 aTexCoord;\n"
        "layout(location = 2) in vec4 aColor;\n"
        "uniform mat4 uProjection;\n"
        "uniform mat4 uModelView;\n"
        "out vec2 vTexCoord;\n"
        "out vec4 vColor;\n"
        "void main() {\n"
        "  gl_Position = uProjection * uModelView * vec4(aPos, 1.0);\n"
        "  vTexCoord = aTexCoord;\n"
        "  vColor = aColor;\n"
        "}\n";

    const char* fShaderSource = 
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec2 vTexCoord;\n"
        "in vec4 vColor;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "uniform bool uUseTexture;\n"
        "void main() {\n"
        "  vec4 texColor = uUseTexture ? texture(uTexture, vTexCoord) : vec4(1.0);\n"
        "  FragColor = texColor * vColor;\n"
        "}\n";

    GLuint CompileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, NULL);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            LOGE("Shader Compilation Failed: %s", infoLog);
        }
        return shader;
    }

    GLuint program = 0;
    GLint uProjectionLoc, uModelViewLoc, uTextureLoc, uUseTextureLoc;
}

IDirect3D8* Direct3DCreate8(UINT SDKVersion) {
    return new IDirect3D8();
}

HRESULT IDirect3D8::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) {
    *ppReturnedDeviceInterface = new IDirect3DDevice8();
    
    // Initialize Shaders
    GLuint vShader = CompileShader(GL_VERTEX_SHADER, vShaderSource);
    GLuint fShader = CompileShader(GL_FRAGMENT_SHADER, fShaderSource);
    program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glLinkProgram(program);
    
    uProjectionLoc = glGetUniformLocation(program, "uProjection");
    uModelViewLoc = glGetUniformLocation(program, "uModelView");
    uTextureLoc = glGetUniformLocation(program, "uTexture");
    uUseTextureLoc = glGetUniformLocation(program, "uUseTexture");
    
    glUseProgram(program);
    return S_OK;
}

HRESULT IDirect3DDevice8::Clear(DWORD Count, const void* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    GLbitfield mask = 0;
    if (Flags & D3DCLEAR_TARGET) {
        glClearColor(Color.r, Color.g, Color.b, Color.a);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (Flags & D3DCLEAR_ZBUFFER) {
        glClearDepthf(Z);
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (mask) glClear(mask);
    return S_OK;
}

HRESULT IDirect3DDevice8::DrawIndexedPrimitive(D3DPRIMITIVETYPE Type, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount) {
    glUseProgram(program);
    
    // Update active matrices
    glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, &m_matProj.m[0][0]);
    glUniformMatrix4fv(uModelViewLoc, 1, GL_FALSE, &m_matWorld.m[0][0]); // Simplified: only world mat for now
    
    GLenum mode = GL_TRIANGLES;
    GLsizei count = 0;
    switch(Type) {
        case D3DPT_TRIANGLELIST: mode = GL_TRIANGLES; count = PrimitiveCount * 3; break;
        case D3DPT_TRIANGLESTRIP: mode = GL_TRIANGLE_STRIP; count = PrimitiveCount + 2; break;
        case D3DPT_TRIANGLEFAN: mode = GL_TRIANGLE_FAN; count = PrimitiveCount + 2; break;
    }

    if (m_pStreamSource) {
        glBindBuffer(GL_ARRAY_BUFFER, m_pStreamSource->glVbo);
        // Assuming position at offset 0, tex at offset 24, color at 12 (approximate FVF mapping)
        glEnableVertexAttribArray(0); // Pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, m_StreamStride, (void*)0);
        
        glEnableVertexAttribArray(1); // Tex
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, m_StreamStride, (void*)24); // Dummy offset
        
        glEnableVertexAttribArray(2); // Color
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, m_StreamStride, (void*)12);
    }

    if (m_pIndexBuffer) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pIndexBuffer->glIbo);
        glDrawElements(mode, count, GL_UNSIGNED_SHORT, (void*)(size_t)(StartIndex * 2));
    }
    
    return S_OK;
}

// Stub implementation for common device methods
HRESULT IDirect3DDevice8::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) { return S_OK; }
HRESULT IDirect3DDevice8::BeginScene() { return S_OK; }
HRESULT IDirect3DDevice8::EndScene() { return S_OK; }
HRESULT IDirect3DDevice8::Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const void* pDirtyRegion) {
    eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW));
    return S_OK;
}
HRESULT IDirect3DDevice8::SetTexture(DWORD Stage, LPDIRECT3DBaseTexture8 pTexture) { 
    if (pTexture) {
        IDirect3DTexture8* tex = (IDirect3DTexture8*)pTexture;
        glActiveTexture(GL_TEXTURE0 + Stage);
        glBindTexture(GL_TEXTURE_2D, tex->glId);
        glUniform1i(uUseTextureLoc, 1);
        glUniform1i(uTextureLoc, Stage);
    } else {
        glUniform1i(uUseTextureLoc, 0);
    }
    return S_OK; 
}
HRESULT IDirect3DDevice8::SetStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER8 pStreamData, UINT Stride) { 
    m_pStreamSource = pStreamData;
    m_StreamStride = Stride;
    return S_OK; 
}
HRESULT IDirect3DDevice8::SetIndices(LPDIRECT3DINDEXBUFFER8 pIndexBuffer, UINT BaseVertexIndex) { 
    m_pIndexBuffer = pIndexBuffer;
    return S_OK; 
}

#endif
