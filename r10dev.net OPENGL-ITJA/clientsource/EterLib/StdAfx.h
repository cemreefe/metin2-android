#pragma once

#ifdef M2_PORT
    #define ENABLE_OPENGL
    #define _UNICODE
    #define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "../UserInterface/Locale_inc.h"

#define _WIN32_DCOM

#pragma warning(disable:4710)	// not inlined
#pragma warning(disable:4786)	// character 255 �Ѿ�°� ����
#pragma warning(disable:4244)	// type conversion possible lose of data

#pragma warning(disable:4018)
#pragma warning(disable:4245)
#pragma warning(disable:4512)
#pragma warning(disable:4201)

#if _MSC_VER >= 1400
#pragma warning(disable:4201 4512 4238 4239)
#endif

#ifndef ENABLE_OPENGL
#include <d3d8.h>
#include <d3dx8.h>
#else
#include "GrpOpenGL.h"
#endif

#define DIRECTINPUT_VERSION 0x0800

#ifndef M2_PORT
#include <dinput.h>
#pragma warning ( disable : 4201 )
#include <mmsystem.h>
#pragma warning ( default : 4201 )
#include <process.h>
#include <direct.h>
#else
#include <unistd.h>
#define _getcwd getcwd
#define _mkdir(x) mkdir(x, 0777)
#endif
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <malloc.h>

#ifndef ENABLE_OPENGL
#ifndef M2_PORT
#pragma comment(lib, "winmm.lib")
#endif
#ifndef M2_PORT
#pragma comment(lib, "d3d8.lib")
#endif
#ifndef M2_PORT
#pragma comment(lib, "d3dx8.lib")
#endif
#endif

#include "../EterBase/StdAfx.h"
#include "../EterBase/Debug.h"
#include "../EterLocale/CodePageId.h"

#if !defined(VC_EXTRALEAN)
#ifndef M2_PORT
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif
#endif

/*
#include "Pool.h"
#include "Dynamic.h"
#include "Event.h"
#include "FuncObject.h"
#include "ReferenceObject.h"
#include "Ref.h"

#include "Util.h"
#include "TextFileLoader.h"
#include "parser.h"

#include "Resource.h"
#include "ResourceManager.h"

#include "MSWindow.h"
#include "MSApplication.h"
#include "Mutex.h"
#include "Thread.h"

#include "GrpBase.h"

#include "GrpDIB.h"
#include "GrpMath.h"
#include "GrpDevice.h"

#include "CollisionData.h"
#include "GrpCollisionObject.h"
#include "GrpScreen.h"
#include "CullingManager.h"

// Attribute
#include "AttributeData.h"
#include "AttributeInstance.h"

#include "GrpObjectInstance.h"
#include "GrpRatioInstance.h"

#include "GrpD3DXBuffer.h"

#include "GrpTexture.h"
#include "GrpImageTexture.h"
#include "GrpFontTexture.h"

#include "GrpText.h"
#include "GrpImage.h"
#include "GrpSubImage.h"

#include "GrpIndexBuffer.h"
#include "GrpVertexBuffer.h"
#include "GrpVertexBufferStatic.h"
#include "GrpVertexBufferDynamic.h"
#include "GrpVertexShader.h"
#include "GrpPixelShader.h"

#include "GrpShadowTexture.h"
#include "GrpImageInstance.h"
#include "GrpExpandedImageInstance.h"
#include "GrpTextInstance.h"
#include "GrpLightManager.h"

#include "TargaResource.h"

#include "NetDevice.h"
#include "NetAddress.h"
// #include "NetStream.h"
#include "NetPacketHeaderMap.h"
#include "NetDatagramSender.h"
#include "NetDatagramReceiver.h"

#include "Input.h"
#include "IME.h"

#include "PathStack.h"
//#include "Property.h"

#include "Profiler.h"

#include "StateManager.h"

#include "ColorTransitionHelper.h"
#include "LensFlare.h"
#include "ScreenFilter.h"
#include "EnvironmentMap.h"

#include "lineintersect_utils.h"

#include "Decal.h"
*/
