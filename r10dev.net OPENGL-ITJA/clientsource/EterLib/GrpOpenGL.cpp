#include "StdAfx.h"
#include "GrpOpenGL.h"

#ifdef ENABLE_OPENGL

#ifdef ANDROID
#include <android/log.h>
#include <EGL/egl.h>
#define LOG_TAG "Metin2Mobile"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#define LOGE(...)
#endif

void* c_dfDIKeyboard = NULL;
static unsigned g_uDbgDraws = 0;

namespace {
    // D3D8 fixed-function emulation: stage 0 texture combiner, alpha test, pre-transformed (RHW) vertices.
    // D3DCOLOR and A8R8G8B8 texels are stored little-endian as B,G,R,A, so both are swizzled with .bgra.
    const char* vShaderSource =
        "#version 300 es\n"
        "layout(location = 0) in vec4 aPos;\n"
        "layout(location = 1) in vec2 aTexCoord;\n"
        "layout(location = 2) in vec4 aColor;\n"
        "uniform mat4 uMVP;\n"
        "uniform bool uRHW;\n"
        "uniform vec2 uViewport;\n"
        "out vec2 vTexCoord;\n"
        "out vec4 vColor;\n"
        "void main() {\n"
        "  if (uRHW) {\n"
        "    gl_Position = vec4(aPos.x / uViewport.x * 2.0 - 1.0, 1.0 - aPos.y / uViewport.y * 2.0, aPos.z * 2.0 - 1.0, 1.0);\n"
        "  } else {\n"
        "    vec4 p = uMVP * vec4(aPos.xyz, 1.0);\n"
        "    p.z = p.z * 2.0 - p.w;\n"
        "    gl_Position = p;\n"
        "  }\n"
        "  vTexCoord = aTexCoord;\n"
        "  vColor = aColor.bgra;\n"
        "}\n";

    const char* fShaderSource =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec2 vTexCoord;\n"
        "in vec4 vColor;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "uniform bool uUseTexture;\n"
        "uniform vec4 uTFactor;\n"
        "uniform ivec3 uColorStage;\n"
        "uniform ivec3 uAlphaStage;\n"
        "uniform int uAlphaFunc;\n"
        "uniform float uAlphaRef;\n"
        "vec4 Arg(int a, vec4 diffuse, vec4 tex) {\n"
        "  int sel = a & 15;\n"
        "  vec4 v = diffuse;\n"
        "  if (sel == 2) v = tex;\n"
        "  else if (sel == 3) v = uTFactor;\n"
        "  if ((a & 32) != 0) v = vec4(v.a);\n"
        "  if ((a & 16) != 0) v = vec4(1.0) - v;\n"
        "  return v;\n"
        "}\n"
        "vec4 Op(int op, vec4 a1, vec4 a2, vec4 diffuse, vec4 tex) {\n"
        "  if (op == 1) return diffuse;\n"
        "  if (op == 2) return a1;\n"
        "  if (op == 3) return a2;\n"
        "  if (op == 5) return clamp(a1 * a2 * 2.0, 0.0, 1.0);\n"
        "  if (op == 6) return clamp(a1 * a2 * 4.0, 0.0, 1.0);\n"
        "  if (op == 7) return clamp(a1 + a2, 0.0, 1.0);\n"
        "  if (op == 8) return clamp(a1 + a2 - 0.5, 0.0, 1.0);\n"
        "  if (op == 9) return clamp((a1 + a2 - 0.5) * 2.0, 0.0, 1.0);\n"
        "  if (op == 10) return clamp(a1 - a2, 0.0, 1.0);\n"
        "  if (op == 11) return clamp(a1 + a2 - a1 * a2, 0.0, 1.0);\n"
        "  if (op == 12) return mix(a2, a1, diffuse.a);\n"
        "  if (op == 13) return mix(a2, a1, tex.a);\n"
        "  if (op == 14) return mix(a2, a1, uTFactor.a);\n"
        "  return a1 * a2;\n"
        "}\n"
        "bool AlphaPass(float a) {\n"
        "  if (uAlphaFunc == 1) return false;\n"
        "  if (uAlphaFunc == 2) return a < uAlphaRef;\n"
        "  if (uAlphaFunc == 3) return abs(a - uAlphaRef) < 0.002;\n"
        "  if (uAlphaFunc == 4) return a <= uAlphaRef;\n"
        "  if (uAlphaFunc == 5) return a > uAlphaRef;\n"
        "  if (uAlphaFunc == 6) return abs(a - uAlphaRef) >= 0.002;\n"
        "  if (uAlphaFunc == 7) return a >= uAlphaRef;\n"
        "  return true;\n"
        "}\n"
        "void main() {\n"
        "  vec4 tex = uUseTexture ? texture(uTexture, vTexCoord).bgra : vec4(1.0);\n"
        "  vec4 c = Op(uColorStage.x, Arg(uColorStage.y, vColor, tex), Arg(uColorStage.z, vColor, tex), vColor, tex);\n"
        "  vec4 a = Op(uAlphaStage.x, Arg(uAlphaStage.y, vColor, tex), Arg(uAlphaStage.z, vColor, tex), vColor, tex);\n"
        "  FragColor = vec4(c.rgb, a.a);\n"
        "  if (!AlphaPass(FragColor.a)) discard;\n"
        "}\n";

    GLuint CompileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, NULL);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
            LOGE("Shader Compilation Failed: %s", infoLog);
        }
        return shader;
    }

    GLuint program = 0;
    GLint uMVPLoc, uRHWLoc, uViewportLoc, uTextureLoc, uUseTextureLoc, uTFactorLoc, uColorStageLoc, uAlphaStageLoc, uAlphaFuncLoc, uAlphaRefLoc;

    GLenum ToGLBlend(DWORD v) {
        switch (v) {
            case D3DBLEND_ZERO: return GL_ZERO;
            case D3DBLEND_ONE: return GL_ONE;
            case D3DBLEND_SRCCOLOR: return GL_SRC_COLOR;
            case D3DBLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
            case D3DBLEND_SRCALPHA: return GL_SRC_ALPHA;
            case D3DBLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
            case D3DBLEND_DESTALPHA: return GL_DST_ALPHA;
            case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
            case D3DBLEND_DESTCOLOR: return GL_DST_COLOR;
            case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
            case D3DBLEND_SRCALPHASAT: return GL_SRC_ALPHA_SATURATE;
        }
        return GL_ONE;
    }

    GLenum ToGLCompare(DWORD v) {
        switch (v) {
            case D3DCMP_NEVER: return GL_NEVER;
            case D3DCMP_LESS: return GL_LESS;
            case D3DCMP_EQUAL: return GL_EQUAL;
            case D3DCMP_LESSEQUAL: return GL_LEQUAL;
            case D3DCMP_GREATER: return GL_GREATER;
            case D3DCMP_NOTEQUAL: return GL_NOTEQUAL;
            case D3DCMP_GREATEREQUAL: return GL_GEQUAL;
        }
        return GL_ALWAYS;
    }

    GLenum ToGLMode(D3DPRIMITIVETYPE type, UINT primCount, GLsizei* pCount) {
        switch (type) {
            case D3DPT_POINTLIST: *pCount = primCount; return GL_POINTS;
            case D3DPT_LINELIST: *pCount = primCount * 2; return GL_LINES;
            case D3DPT_LINESTRIP: *pCount = primCount + 1; return GL_LINE_STRIP;
            case D3DPT_TRIANGLESTRIP: *pCount = primCount + 2; return GL_TRIANGLE_STRIP;
            case D3DPT_TRIANGLEFAN: *pCount = primCount + 2; return GL_TRIANGLE_FAN;
        }
        *pCount = primCount * 3;
        return GL_TRIANGLES;
    }

    GLint ToGLWrap(DWORD v) {
        switch (v) {
            case D3DTADDRESS_MIRROR: return GL_MIRRORED_REPEAT;
            case D3DTADDRESS_CLAMP:
            case D3DTADDRESS_BORDER:
            case D3DTADDRESS_MIRRORONCE: return GL_CLAMP_TO_EDGE;
        }
        return GL_REPEAT;
    }

    DWORD FVFFromStride(UINT stride) {
        switch (stride) {
            case 12: return D3DFVF_XYZ;
            case 16: return D3DFVF_XYZ | D3DFVF_DIFFUSE;
            case 20: return D3DFVF_XYZ | D3DFVF_TEX1;
            case 24: return D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
            case 28: return D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
            case 32: return D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
            case 40: return D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2;
        }
        return D3DFVF_XYZ;
    }
}

IDirect3D8* Direct3DCreate8(UINT SDKVersion) {
    return new IDirect3D8();
}

HRESULT IDirect3D8::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) {
    IDirect3DDevice8* pDevice = new IDirect3DDevice8();
    *ppReturnedDeviceInterface = pDevice;

    GLuint vShader = CompileShader(GL_VERTEX_SHADER, vShaderSource);
    GLuint fShader = CompileShader(GL_FRAGMENT_SHADER, fShaderSource);
    program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        LOGE("Shader Link Failed: %s", infoLog);
    }

    uMVPLoc = glGetUniformLocation(program, "uMVP");
    uRHWLoc = glGetUniformLocation(program, "uRHW");
    uViewportLoc = glGetUniformLocation(program, "uViewport");
    uTextureLoc = glGetUniformLocation(program, "uTexture");
    uUseTextureLoc = glGetUniformLocation(program, "uUseTexture");
    uTFactorLoc = glGetUniformLocation(program, "uTFactor");
    uColorStageLoc = glGetUniformLocation(program, "uColorStage");
    uAlphaStageLoc = glGetUniformLocation(program, "uAlphaStage");
    uAlphaFuncLoc = glGetUniformLocation(program, "uAlphaFunc");
    uAlphaRefLoc = glGetUniformLocation(program, "uAlphaRef");

    glUseProgram(program);
    glUniform1i(uTextureLoc, 0);

    GLint vp[4] = { 0, 0, 1, 1 };
    glGetIntegerv(GL_VIEWPORT, vp);
    pDevice->m_fViewportWidth = (float)(vp[2] > 0 ? vp[2] : 1);
    pDevice->m_fViewportHeight = (float)(vp[3] > 0 ? vp[3] : 1);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    return S_OK;
}

HRESULT IDirect3DTexture8::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) {
    int bpp = (format == D3DFMT_A4R4G4B4 || format == D3DFMT_A1R5G5B5 || format == D3DFMT_X1R5G5B5 || format == D3DFMT_R5G6B5) ? 2 : 4;
    if (Level > 0) {
        if (!pScratch) pScratch = malloc((size_t)width * height * 4);
        pLockedRect->pBits = pScratch;
        pLockedRect->Pitch = (width >> Level ? width >> Level : 1) * bpp;
        return S_OK;
    }
    if (!pLockedData) pLockedData = malloc((size_t)width * height * 4);
    pLockedRect->pBits = pLockedData;
    pLockedRect->Pitch = width * bpp;
    return S_OK;
}

HRESULT IDirect3DTexture8::UnlockRect(UINT Level) {
    if (Level > 0 || !glId || !pLockedData)
        return S_OK;

    const void* pUpload = pLockedData;
    std::vector<DWORD> converted;
    if (format == D3DFMT_A4R4G4B4 || format == D3DFMT_A1R5G5B5 || format == D3DFMT_X1R5G5B5 || format == D3DFMT_R5G6B5) {
        converted.resize((size_t)width * height);
        const WORD* src = (const WORD*)pLockedData;
        for (size_t i = 0; i < converted.size(); ++i) {
            WORD w = src[i];
            DWORD a, r, g, b;
            if (format == D3DFMT_A4R4G4B4) {
                a = ((w >> 12) & 15) * 17; r = ((w >> 8) & 15) * 17; g = ((w >> 4) & 15) * 17; b = (w & 15) * 17;
            } else if (format == D3DFMT_R5G6B5) {
                a = 255; r = ((w >> 11) & 31) * 255 / 31; g = ((w >> 5) & 63) * 255 / 63; b = (w & 31) * 255 / 31;
            } else {
                a = (format == D3DFMT_X1R5G5B5 || (w & 0x8000)) ? 255 : 0;
                r = ((w >> 10) & 31) * 255 / 31; g = ((w >> 5) & 31) * 255 / 31; b = (w & 31) * 255 / 31;
            }
            converted[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
        pUpload = converted.data();
    }

    glBindTexture(GL_TEXTURE_2D, glId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pUpload);
    return S_OK;
}

ULONG IDirect3DTexture8::Release() {
    if (glId) glDeleteTextures(1, &glId);
    if (pLockedData) free(pLockedData);
    if (pScratch) free(pScratch);
    delete this;
    return 0;
}

HRESULT IDirect3DDevice8::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, LPDIRECT3DTEXTURE8* ppTexture) {
    IDirect3DTexture8* tex = new IDirect3DTexture8();
    tex->width = Width;
    tex->height = Height;
    tex->format = Format;
    glGenTextures(1, &tex->glId);
    glBindTexture(GL_TEXTURE_2D, tex->glId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    *ppTexture = tex;
    return S_OK;
}

HRESULT IDirect3DDevice8::SetViewport(const D3DVIEWPORT8* v) {
    glViewport(v->X, v->Y, v->Width, v->Height);
    m_fViewportWidth = (float)(v->Width ? v->Width : 1);
    m_fViewportHeight = (float)(v->Height ? v->Height : 1);
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
        glDepthMask(GL_TRUE);
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (mask) glClear(mask);
    return S_OK;
}

HRESULT IDirect3DDevice8::SetTransform(DWORD type, const GLMATRIX* mat) {
    if (!mat) return S_OK;
    switch (type) {
        case D3DTS_WORLD: m_matWorld = *mat; break;
        case D3DTS_VIEW: m_matView = *mat; break;
        case D3DTS_PROJECTION: m_matProj = *mat; break;
    }
    return S_OK;
}

HRESULT IDirect3DDevice8::GetTransform(DWORD type, GLMATRIX* mat) {
    if (!mat) return S_OK;
    switch (type) {
        case D3DTS_WORLD: *mat = m_matWorld; break;
        case D3DTS_VIEW: *mat = m_matView; break;
        case D3DTS_PROJECTION: *mat = m_matProj; break;
        default: D3DXMatrixIdentity(mat); break;
    }
    return S_OK;
}

HRESULT IDirect3DDevice8::SetRenderState(DWORD Type, DWORD Value) {
    switch (Type) {
        case D3DRS_ZENABLE:
            if (Value) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            break;
        case D3DRS_ZFUNC:
            glDepthFunc(ToGLCompare(Value));
            break;
        case D3DRS_ZWRITEENABLE:
            glDepthMask(Value ? GL_TRUE : GL_FALSE);
            break;
        case D3DRS_ALPHABLENDENABLE:
            if (Value) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            break;
        case D3DRS_SRCBLEND:
            m_dwSrcBlend = Value;
            glBlendFunc(ToGLBlend(m_dwSrcBlend), ToGLBlend(m_dwDestBlend));
            break;
        case D3DRS_DESTBLEND:
            m_dwDestBlend = Value;
            glBlendFunc(ToGLBlend(m_dwSrcBlend), ToGLBlend(m_dwDestBlend));
            break;
        case D3DRS_ALPHATESTENABLE:
            m_bAlphaTest = Value ? TRUE : FALSE;
            break;
        case D3DRS_ALPHAREF:
            m_dwAlphaRef = Value;
            break;
        case D3DRS_ALPHAFUNC:
            m_dwAlphaFunc = Value;
            break;
        case D3DRS_TEXTUREFACTOR:
            m_dwTextureFactor = Value;
            break;
        case D3DRS_CULLMODE:
            // Metin2 relies on D3D's left-handed winding; keep culling off until 3D winding is validated.
            glDisable(GL_CULL_FACE);
            break;
    }
    return S_OK;
}

HRESULT IDirect3DDevice8::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) {
    if (stage >= MAX_STAGES) return S_OK;
    switch (type) {
        case D3DTSS_COLOROP: m_adwColorOp[stage] = value; break;
        case D3DTSS_COLORARG1: m_adwColorArg1[stage] = value; break;
        case D3DTSS_COLORARG2: m_adwColorArg2[stage] = value; break;
        case D3DTSS_ALPHAOP: m_adwAlphaOp[stage] = value; break;
        case D3DTSS_ALPHAARG1: m_adwAlphaArg1[stage] = value; break;
        case D3DTSS_ALPHAARG2: m_adwAlphaArg2[stage] = value; break;
        case D3DTSS_ADDRESSU: m_adwAddressU[stage] = value; break;
        case D3DTSS_ADDRESSV: m_adwAddressV[stage] = value; break;
        case D3DTSS_MAGFILTER: m_adwMagFilter[stage] = value; break;
        case D3DTSS_MINFILTER: m_adwMinFilter[stage] = value; break;
    }
    return S_OK;
}

HRESULT IDirect3DDevice8::SetTexture(DWORD Stage, LPDIRECT3DBaseTexture8 pTexture) {
    if (Stage < MAX_STAGES)
        m_apTexture[Stage] = (IDirect3DTexture8*)pTexture;
    return S_OK;
}

void IDirect3DDevice8::ApplyDrawState(const BYTE* pVertexBase, UINT uStride) {
    ++g_uDbgDraws;
    glUseProgram(program);

    DWORD fvf = m_dwFVF;
    if (!(fvf & (D3DFVF_XYZ | D3DFVF_XYZRHW)) || D3DXGetFVFVertexSize(fvf) != uStride)
        fvf = FVFFromStride(uStride);

    UINT offset = 0;
    bool bRHW = (fvf & D3DFVF_XYZRHW) != 0;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, bRHW ? 4 : 3, GL_FLOAT, GL_FALSE, uStride, pVertexBase + offset);
    offset += bRHW ? 16 : 12;
    if (fvf & D3DFVF_NORMAL) offset += 12;
    if (fvf & D3DFVF_DIFFUSE) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, uStride, pVertexBase + offset);
        offset += 4;
    } else {
        glDisableVertexAttribArray(2);
        glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (fvf & D3DFVF_SPECULAR) offset += 4;
    if ((fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, uStride, pVertexBase + offset);
    } else {
        glDisableVertexAttribArray(1);
        glVertexAttrib4f(1, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    GLMATRIX wv, mvp;
    D3DXMatrixMultiply(&wv, &m_matWorld, &m_matView);
    D3DXMatrixMultiply(&mvp, &wv, &m_matProj);
    glUniformMatrix4fv(uMVPLoc, 1, GL_FALSE, &mvp.m[0][0]);
    glUniform1i(uRHWLoc, bRHW ? 1 : 0);
    glUniform2f(uViewportLoc, m_fViewportWidth, m_fViewportHeight);

    IDirect3DTexture8* pTex = m_apTexture[0];
    glActiveTexture(GL_TEXTURE0);
    if (pTex && pTex->glId) {
        glBindTexture(GL_TEXTURE_2D, pTex->glId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(m_adwAddressU[0]));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(m_adwAddressV[0]));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_adwMagFilter[0] == D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_adwMinFilter[0] == D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR);
        glUniform1i(uUseTextureLoc, 1);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(uUseTextureLoc, 0);
    }

    GLCOLOR tf((unsigned int)m_dwTextureFactor);
    glUniform4f(uTFactorLoc, tf.r, tf.g, tf.b, tf.a);
    glUniform3i(uColorStageLoc, (GLint)m_adwColorOp[0], (GLint)m_adwColorArg1[0], (GLint)m_adwColorArg2[0]);
    glUniform3i(uAlphaStageLoc, (GLint)m_adwAlphaOp[0], (GLint)m_adwAlphaArg1[0], (GLint)m_adwAlphaArg2[0]);
    glUniform1i(uAlphaFuncLoc, m_bAlphaTest ? (GLint)m_dwAlphaFunc : D3DCMP_ALWAYS);
    glUniform1f(uAlphaRefLoc, (m_dwAlphaRef & 0xff) / 255.0f);
}

HRESULT IDirect3DDevice8::DrawPrimitive(D3DPRIMITIVETYPE Type, UINT StartVertex, UINT PrimitiveCount) {
    if (!m_pStreamSource || !m_StreamStride) return S_OK;
    GLsizei count;
    GLenum mode = ToGLMode(Type, PrimitiveCount, &count);
    glBindBuffer(GL_ARRAY_BUFFER, m_pStreamSource->glVbo);
    ApplyDrawState((const BYTE*)0, m_StreamStride);
    glDrawArrays(mode, StartVertex, count);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return S_OK;
}

HRESULT IDirect3DDevice8::DrawIndexedPrimitive(D3DPRIMITIVETYPE Type, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount) {
    if (!m_pStreamSource || !m_StreamStride || !m_pIndexBuffer) return S_OK;
    GLsizei count;
    GLenum mode = ToGLMode(Type, PrimitiveCount, &count);
    glBindBuffer(GL_ARRAY_BUFFER, m_pStreamSource->glVbo);
    ApplyDrawState((const BYTE*)0 + (size_t)m_BaseVertexIndex * m_StreamStride, m_StreamStride);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pIndexBuffer->glIbo);
    UINT indexSize = m_pIndexBuffer->indexSize;
    glDrawElements(mode, count, indexSize == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)(size_t)(StartIndex * indexSize));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return S_OK;
}

HRESULT IDirect3DDevice8::DrawPrimitiveUP(D3DPRIMITIVETYPE Type, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    if (!pVertexStreamZeroData || !VertexStreamZeroStride) return S_OK;
    GLsizei count;
    GLenum mode = ToGLMode(Type, PrimitiveCount, &count);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    ApplyDrawState((const BYTE*)pVertexStreamZeroData, VertexStreamZeroStride);
    glDrawArrays(mode, 0, count);
    // D3D8 semantics: DrawPrimitiveUP resets stream 0.
    m_pStreamSource = NULL;
    m_StreamStride = 0;
    return S_OK;
}

HRESULT IDirect3DDevice8::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE Type, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    if (!pVertexStreamZeroData || !VertexStreamZeroStride || !pIndexData) return S_OK;
    GLsizei count;
    GLenum mode = ToGLMode(Type, PrimitiveCount, &count);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    ApplyDrawState((const BYTE*)pVertexStreamZeroData, VertexStreamZeroStride);
    glDrawElements(mode, count, IndexDataFormat == D3DFMT_INDEX16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, pIndexData);
    m_pStreamSource = NULL;
    m_StreamStride = 0;
    m_pIndexBuffer = NULL;
    return S_OK;
}

HRESULT IDirect3DDevice8::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) { return S_OK; }
HRESULT IDirect3DDevice8::BeginScene() { return S_OK; }
HRESULT IDirect3DDevice8::EndScene() { return S_OK; }
HRESULT IDirect3DDevice8::Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const void* pDirtyRegion) {
#ifdef ANDROID
    static unsigned s_uFrame = 0;
    EGLBoolean ok = eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW));
    if ((s_uFrame++ % 300) == 0)
        LOGI("DBG Present frame=%u swap=%d eglErr=0x%x glErr=0x%x draws=%u vp=%.0fx%.0f", s_uFrame, ok, eglGetError(), glGetError(), g_uDbgDraws, m_fViewportWidth, m_fViewportHeight);
#endif
    return S_OK;
}
HRESULT IDirect3DDevice8::SetStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER8 pStreamData, UINT Stride) {
    if (StreamNumber == 0) {
        m_pStreamSource = pStreamData;
        m_StreamStride = Stride;
    }
    return S_OK;
}
HRESULT IDirect3DDevice8::SetIndices(LPDIRECT3DINDEXBUFFER8 pIndexBuffer, UINT BaseVertexIndex) {
    m_pIndexBuffer = pIndexBuffer;
    m_BaseVertexIndex = BaseVertexIndex;
    return S_OK;
}

#endif
