cmake_minimum_required(VERSION 3.10.2)
project("Metin2Mobile")

# Target port: android (Gradle/NDK) or web (Emscripten). Gradle always sets ANDROID.
if(NOT DEFINED M2_TARGET)
    if(ANDROID)
        set(M2_TARGET android)
    elseif(EMSCRIPTEN)
        set(M2_TARGET web)
    else()
        set(M2_TARGET android)
    endif()
endif()

if(M2_TARGET STREQUAL "web")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../web/emflags.cmake)
endif()

if(M2_TARGET STREQUAL "android")
    add_definitions(-D__ANDROID__)
    add_definitions(-DENABLE_MOBILE_CONTROLS)
    # Some older Granny versions use these for 64-bit
    add_definitions(-DGRANNY_ONLY_TYPES)
endif()

add_definitions(-DENABLE_OPENGL)
add_definitions(-DLOCALE_SERVICE_EUROPE)
add_definitions(-DNDEBUG) # match the Windows release build: asserts are compiled out

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/android_compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../Extern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Extern/include/Python2
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/EterBase
    ${CMAKE_CURRENT_SOURCE_DIR}/EterLib
    ${CMAKE_CURRENT_SOURCE_DIR}/EterPythonLib
    ${CMAKE_CURRENT_SOURCE_DIR}/GameLib
    ${CMAKE_CURRENT_SOURCE_DIR}/UserInterface
    ${CMAKE_CURRENT_SOURCE_DIR}/MilesLib
    ${CMAKE_CURRENT_SOURCE_DIR}/EffectLib
    ${CMAKE_CURRENT_SOURCE_DIR}/SphereLib
    ${CMAKE_CURRENT_SOURCE_DIR}/eterPack
    ${CMAKE_CURRENT_SOURCE_DIR}/EterGrnLib
    ${CMAKE_CURRENT_SOURCE_DIR}/EterImageLib
    ${CMAKE_CURRENT_SOURCE_DIR}/SpeedTreeLib
    ${CMAKE_CURRENT_SOURCE_DIR}/PRTerrainLib
    ${CMAKE_CURRENT_SOURCE_DIR}/ScriptLib
    ${CMAKE_CURRENT_SOURCE_DIR}/EterLocale
    ${CMAKE_CURRENT_SOURCE_DIR}/CWebBrowser
)

set(ETER_BASE_SOURCES
    android_compat/win_stub.cpp
    platform/m2platform.cpp
    EterBase/Poly/Poly.cpp
    EterBase/Poly/SymTable.cpp
    android_compat/devil_stub.cpp
    android_compat/gdi_font.cpp
    android_compat/speedtree_stub.cpp
    android_compat/ime_stub.cpp
    android_compat/mswindow_stub.cpp
    EterBase/CPostIt.cpp
    EterBase/CRC32.cpp
    EterBase/Debug.cpp
    EterBase/FileBase.cpp
    EterBase/FileDir.cpp
    EterBase/FileLoader.cpp
    EterBase/MappedFile.cpp
    EterBase/Random.cpp
    EterBase/StdAfx.cpp
    EterBase/Stl.cpp
    EterBase/TempFile.cpp
    EterBase/Timer.cpp
    EterBase/Utils.cpp
    EterBase/cipher.cpp
    EterBase/error.cpp
    EterBase/lzo.cpp
    EterBase/tea.cpp
)

set(ETER_LIB_SOURCES
    EterLib/IME.cpp
    EterLib/MSWindow.cpp
    EterLib/DibBar.cpp
    EterLib/Camera.cpp
    EterLib/GrpDevice.cpp
    EterLib/GrpObjectInstance.cpp
    EterLib/GrpScreen.cpp
    EterLib/GrpTexture.cpp
    EterLib/GrpImageTexture.cpp
    EterLib/GrpOpenGL.cpp
    EterLib/GrpBase.cpp
    EterLib/GrpColor.cpp
    EterLib/GrpColorInstance.cpp
    EterLib/GrpImage.cpp
    EterLib/GrpImageInstance.cpp
    EterLib/GrpIndexBuffer.cpp
    EterLib/GrpMath.cpp
    EterLib/GrpVertexBuffer.cpp
    EterLib/GrpText.cpp
    EterLib/GrpTextInstance.cpp
    EterLib/MSApplication.cpp
    EterLib/Input.cpp
    EterLib/NetDevice.cpp
    EterLib/ResourceManager.cpp
    EterLib/NetAddress.cpp
    EterLib/NetDatagramReceiver.cpp
    EterLib/NetDatagramSender.cpp
    EterLib/NetStream.cpp
    EterLib/StdAfx.cpp
    EterLib/AttributeData.cpp
    EterLib/AttributeInstance.cpp
    EterLib/CollisionData.cpp
    EterLib/Resource.cpp
    EterLib/TargaResource.cpp
    EterLib/TextFileLoader.cpp
    EterLib/Util.cpp
    EterLib/BlockTexture.cpp
    EterLib/ColorTransitionHelper.cpp
    EterLib/CullingManager.cpp
    EterLib/Decal.cpp
    EterLib/EnvironmentMap.cpp
    EterLib/FileLoaderThread.cpp
    EterLib/GrpCollisionObject.cpp
    EterLib/GrpD3DXBuffer.cpp
    EterLib/GrpDIB.cpp
    EterLib/GrpDetector.cpp
    EterLib/GrpExpandedImageInstance.cpp
    EterLib/GrpFontTexture.cpp
    EterLib/GrpLightManager.cpp
    EterLib/GrpMarkInstance.cpp
    EterLib/GrpPixelShader.cpp
    EterLib/GrpRatioInstance.cpp
    EterLib/GrpShadowTexture.cpp
    EterLib/GrpSubImage.cpp
    EterLib/GrpVertexBufferDynamic.cpp
    EterLib/GrpVertexBufferStatic.cpp
    EterLib/GrpVertexShader.cpp
    EterLib/JpegFile.cpp
    EterLib/LensFlare.cpp
    EterLib/Mutex.cpp
    EterLib/NetDatagram.cpp
    EterLib/NetPacketHeaderMap.cpp
    EterLib/PathStack.cpp
    EterLib/ReferenceObject.cpp
    EterLib/ScreenFilter.cpp
    EterLib/SkyBox.cpp
    EterLib/StateManager.cpp
    EterLib/TextBar.cpp
    EterLib/TextTag.cpp
    EterLib/Thread.cpp
    EterLib/lineintersect_utils.cpp
    EterLib/parser.cpp
)

set(ETER_IMAGE_LIB_SOURCES
    EterImageLib/DXTCImage.cpp
    EterImageLib/Image.cpp
    EterImageLib/StdAfx.cpp
    EterImageLib/TGAImage.cpp
    EterImageLib/stb_image.cpp
)

set(GAME_LIB_SOURCES
    GameLib/Area.cpp
    GameLib/AreaLoaderThread.cpp
    GameLib/ActorInstance.cpp
    GameLib/ActorInstanceAttach.cpp
    GameLib/ActorInstanceBattle.cpp
    GameLib/ActorInstanceBlend.cpp
    GameLib/ActorInstanceCollisionDetection.cpp
    GameLib/ActorInstanceData.cpp
    GameLib/ActorInstanceEvent.cpp
    GameLib/ActorInstanceFly.cpp
    GameLib/ActorInstanceMotion.cpp
    GameLib/ActorInstanceMotionEvent.cpp
    GameLib/ActorInstancePosition.cpp
    GameLib/ActorInstanceRender.cpp
    GameLib/ActorInstanceRotation.cpp
    GameLib/ActorInstanceSync.cpp
    GameLib/ActorInstanceWeaponTrace.cpp
    GameLib/FlyingInstance.cpp
    GameLib/GameEventManager.cpp
    GameLib/ItemData.cpp
    GameLib/ItemManager.cpp
    GameLib/MapManager.cpp
    GameLib/MapOutdoor.cpp
    GameLib/PropertyManager.cpp
    GameLib/RaceData.cpp
    GameLib/RaceManager.cpp
    GameLib/StdAfx.cpp
    GameLib/AreaTerrain.cpp
    GameLib/DungeonBlock.cpp
    GameLib/FlyTarget.cpp
    GameLib/FlyTrace.cpp
    GameLib/FlyingData.cpp
    GameLib/FlyingObjectManager.cpp
    GameLib/GameType.cpp
    GameLib/GameUtil.cpp
    GameLib/MapBase.cpp
    GameLib/MapOutdoorCharacterShadow.cpp
    GameLib/MapOutdoorIndexBuffer.cpp
    GameLib/MapOutdoorLoad.cpp
    GameLib/MapOutdoorQuadtree.cpp
    GameLib/MapOutdoorRender.cpp
    GameLib/MapOutdoorRenderHTP.cpp
    GameLib/MapOutdoorRenderSTP.cpp
    GameLib/MapOutdoorUpdate.cpp
    GameLib/MapOutdoorWater.cpp
    GameLib/MapType.cpp
    GameLib/MapUtil.cpp
    GameLib/MonsterAreaInfo.cpp
    GameLib/PhysicsObject.cpp
    GameLib/Property.cpp
    GameLib/PropertyLoader.cpp
    GameLib/RaceDataFile.cpp
    GameLib/RaceMotionData.cpp
    GameLib/SnowEnvironment.cpp
    GameLib/SnowParticle.cpp
    GameLib/TerrainDecal.cpp
    GameLib/TerrainPatch.cpp
    GameLib/TerrainQuadtree.cpp
    GameLib/WeaponTrace.cpp
)

set(USER_INTERFACE_SOURCES
    UserInterface/ProcessCRC.cpp
    UserInterface/GuildMarkDownloader.cpp
    UserInterface/GuildMarkUploader.cpp
    UserInterface/InsultChecker.cpp
    UserInterface/PythonApplication.cpp
    UserInterface/PythonApplicationModule.cpp
    UserInterface/PythonPlayerModule.cpp
    UserInterface/PythonPlayer.cpp
    UserInterface/PythonNetworkStream.cpp
    UserInterface/PythonNetworkStreamModule.cpp
    UserInterface/PythonNetworkStreamPhaseGame.cpp
    UserInterface/PythonNetworkStreamPhaseLoading.cpp
    UserInterface/PythonNetworkStreamPhaseLogin.cpp
    UserInterface/PythonNetworkStreamPhaseSelect.cpp
    UserInterface/UserInterface.cpp
    UserInterface/StdAfx.cpp
    UserInterface/AccountConnector.cpp
    UserInterface/AffectFlagContainer.cpp
    UserInterface/CameraProcedure.cpp
    UserInterface/GameType.cpp
    UserInterface/InstanceBase.cpp
    UserInterface/InstanceBaseBattle.cpp
    UserInterface/InstanceBaseEffect.cpp
    UserInterface/InstanceBaseEvent.cpp
    UserInterface/InstanceBaseMotion.cpp
    UserInterface/InstanceBaseMovement.cpp
    UserInterface/InstanceBaseTransform.cpp
    UserInterface/Locale.cpp
    UserInterface/MarkImage.cpp
    UserInterface/MarkManager.cpp
    UserInterface/NetworkActorManager.cpp
    UserInterface/PythonAcce.cpp
    UserInterface/PythonApplicationCamera.cpp
    UserInterface/PythonApplicationCursor.cpp
    UserInterface/PythonApplicationEvent.cpp
    UserInterface/PythonApplicationLogo.cpp
    UserInterface/PythonApplicationProcedure.cpp
    UserInterface/PythonApplicationWebPage.cpp
    UserInterface/PythonBackground.cpp
    UserInterface/PythonBackgroundModule.cpp
    UserInterface/PythonCharacterManager.cpp
    UserInterface/PythonCharacterManagerModule.cpp
    UserInterface/PythonCharacterModule.cpp
    UserInterface/PythonChat.cpp
    UserInterface/PythonChatModule.cpp
    UserInterface/PythonEffectModule.cpp
    UserInterface/PythonEventManager.cpp
    UserInterface/PythonEventManagerMoudle.cpp
    UserInterface/PythonExceptionSender.cpp
    UserInterface/PythonExchange.cpp
    UserInterface/PythonExchangeModule.cpp
    UserInterface/PythonFlyModule.cpp
    UserInterface/PythonGameEventManagerModule.cpp
    UserInterface/PythonGuild.cpp
    UserInterface/PythonIME.cpp
    UserInterface/PythonIMEModule.cpp
    UserInterface/PythonItem.cpp
    UserInterface/PythonItemModule.cpp
    UserInterface/PythonMessenger.cpp
    UserInterface/PythonMiniMap.cpp
    UserInterface/PythonMiniMapModule.cpp
    UserInterface/PythonNetworkDatagram.cpp
    UserInterface/PythonNetworkDatagramModule.cpp
    UserInterface/PythonNetworkStreamCommand.cpp
    UserInterface/PythonNetworkStreamEvent.cpp
    UserInterface/PythonNetworkStreamPhaseGameActor.cpp
    UserInterface/PythonNetworkStreamPhaseGameItem.cpp
    UserInterface/PythonNetworkStreamPhaseHandShake.cpp
    UserInterface/PythonNetworkStreamPhaseOffline.cpp
    UserInterface/PythonNonPlayer.cpp
    UserInterface/PythonNonPlayerModule.cpp
    UserInterface/PythonPackModule.cpp
    UserInterface/PythonPlayerEventHandler.cpp
    UserInterface/PythonPlayerInput.cpp
    UserInterface/PythonPlayerInputKeyboard.cpp
    UserInterface/PythonPlayerInputMouse.cpp
    UserInterface/PythonPlayerSkill.cpp
    UserInterface/PythonProfilerModule.cpp
    UserInterface/PythonQuest.cpp
    UserInterface/PythonSafeBox.cpp
    UserInterface/PythonShop.cpp
    UserInterface/PythonSkill.cpp
    UserInterface/PythonSoundManagerModule.cpp
    UserInterface/PythonSystem.cpp
    UserInterface/PythonSystemModule.cpp
    UserInterface/PythonTextTail.cpp
    UserInterface/PythonTextTailModule.cpp
    UserInterface/ServerStateChecker.cpp
    UserInterface/ServerStateCheckerModule.cpp
    UserInterface/WiseLogicXTrap.cpp
)

set(ETER_PACK_SOURCES
    eterPack/EterPack.cpp
    eterPack/EterPackCursor.cpp
    eterPack/EterPackManager.cpp
    eterPack/EterPackPolicy_CSHybridCrypt.cpp
    eterPack/md5.c
    eterPack/StdAfx.cpp
)

set(ETER_GRN_LIB_SOURCES
    EterGrnLib/Material.cpp
    EterGrnLib/Mesh.cpp
    EterGrnLib/Model.cpp
    EterGrnLib/ModelInstance.cpp
    EterGrnLib/ModelInstanceCollisionDetection.cpp
    EterGrnLib/ModelInstanceModel.cpp
    EterGrnLib/ModelInstanceRender.cpp
    EterGrnLib/ModelInstanceUpdate.cpp
    EterGrnLib/StdAfx.cpp
    EterGrnLib/Thing.cpp
    EterGrnLib/ThingInstance.cpp
    EterGrnLib/LODController.cpp
    EterGrnLib/ModelInstanceMotion.cpp
    EterGrnLib/Motion.cpp
)

set(EFFECT_LIB_SOURCES
    EffectLib/EffectData.cpp
    EffectLib/EffectElementBase.cpp
    EffectLib/EffectElementBaseInstance.cpp
    EffectLib/EffectInstance.cpp
    EffectLib/EffectManager.cpp
    EffectLib/EffectMesh.cpp
    EffectLib/EffectMeshInstance.cpp
    EffectLib/EffectUpdateDecorator.cpp
    EffectLib/EmitterProperty.cpp
    EffectLib/FrameController.cpp
    EffectLib/ParticleInstance.cpp
    EffectLib/ParticleProperty.cpp
    EffectLib/ParticleSystemData.cpp
    EffectLib/ParticleSystemInstance.cpp
    EffectLib/SimpleLightData.cpp
    EffectLib/SimpleLightInstance.cpp
    EffectLib/StdAfx.cpp
    EffectLib/Type.cpp
)

set(ETER_PYTHON_LIB_SOURCES
    EterPythonLib/PythonGraphic.cpp
    EterPythonLib/PythonGraphicImageModule.cpp
    EterPythonLib/PythonGraphicModule.cpp
    EterPythonLib/PythonGraphicTextModule.cpp
    EterPythonLib/PythonGraphicThingModule.cpp
    EterPythonLib/PythonGridSlotWindow.cpp
    EterPythonLib/PythonSlotWindow.cpp
    EterPythonLib/PythonWindow.cpp
    EterPythonLib/PythonWindowManager.cpp
    EterPythonLib/PythonWindowManagerModule.cpp
    EterPythonLib/StdAfx.cpp
)

set(SCRIPT_LIB_SOURCES
    ScriptLib/PythonDebugModule.cpp
    ScriptLib/PythonLauncher.cpp
    ScriptLib/PythonMarshal.cpp
    ScriptLib/PythonUtils.cpp
    ScriptLib/Resource.cpp
    ScriptLib/StdAfx.cpp
)

set(SPHERE_LIB_SOURCES
    SphereLib/frustum.cpp
    SphereLib/sphere.cpp
    SphereLib/spherepack.cpp
    SphereLib/StdAfx.cpp
)

# Per-adapter port implementations
if(M2_TARGET STREQUAL "android")
    set(PLATFORM_SOURCES
        platform/posix/M2PosixNet.cpp
        UserInterface/AndroidMain.cpp
    )
elseif(M2_TARGET STREQUAL "web")
    set(PLATFORM_SOURCES
        platform/web/WebMain.cpp
        platform/web/M2WebNet.cpp
        platform/web/WebLazyFS.cpp
        # SAB loopback transport (shared with the wasm server modules)
        ${CMAKE_CURRENT_SOURCE_DIR}/../server/web/m2lb_bridge.cpp
    )
endif()

set(MILES_LIB_SOURCES
    MilesLib/mss_port.cpp
    MilesLib/SoundBase.cpp
    MilesLib/SoundData.cpp
    MilesLib/SoundInstance2D.cpp
    MilesLib/SoundInstance3D.cpp
    MilesLib/SoundInstanceStream.cpp
    MilesLib/SoundManager.cpp
    MilesLib/SoundManager2D.cpp
    MilesLib/SoundManager3D.cpp
    MilesLib/SoundManagerStream.cpp
    MilesLib/Stdafx.cpp
    MilesLib/Type.cpp
)

set(SPEED_TREE_LIB_SOURCES
    SpeedTreeLib/BoundaryShapeManager.cpp
    SpeedTreeLib/SpeedGrassRT.cpp
    SpeedTreeLib/SpeedGrassWrapper.cpp
    SpeedTreeLib/SpeedTreeForest.cpp
    SpeedTreeLib/SpeedTreeForestDirectX8.cpp
    SpeedTreeLib/SpeedTreeWrapper.cpp
    SpeedTreeLib/StdAfx.cpp
)

set(PR_TERRAIN_LIB_SOURCES
    PRTerrainLib/Terrain.cpp
    PRTerrainLib/TextureSet.cpp
    PRTerrainLib/StdAfx.cpp
)

set(ETER_LOCALE_SOURCES
    EterLocale/Arabic.cpp
    EterLocale/Japanese.cpp
    EterLocale/StringCodec.cpp
    EterLocale/StringCodec_Vietnamese.cpp
    EterLocale/StdAfx.cpp
)

if(M2_TARGET STREQUAL "android" OR M2_TARGET STREQUAL "web")
    # the in-game IE browser shell stays a Windows-only feature on the ports
    set(C_WEB_BROWSER_SOURCES "")
else()
    set(C_WEB_BROWSER_SOURCES
        CWebBrowser/CWebBrowser.c
    )
    add_definitions(-DENABLE_WEB_BROWSER)
endif()

set(OPENGR2NDMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/OpenGr2ndma CACHE PATH "OpenGr2ndma checkout")
set(OPENGR2NDMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENGR2NDMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${OPENGR2NDMA_DIR} ${CMAKE_CURRENT_BINARY_DIR}/opengr2ndma)

set(M2_CORE_SOURCES
    ${USER_INTERFACE_SOURCES}
    ${ETER_BASE_SOURCES}
    ${ETER_LIB_SOURCES}
    ${ETER_IMAGE_LIB_SOURCES}
    ${GAME_LIB_SOURCES}
    ${ETER_PACK_SOURCES}
    ${ETER_GRN_LIB_SOURCES}
    ${EFFECT_LIB_SOURCES}
    ${ETER_PYTHON_LIB_SOURCES}
    ${SCRIPT_LIB_SOURCES}
    ${SPHERE_LIB_SOURCES}
    ${MILES_LIB_SOURCES}
    ${SPEED_TREE_LIB_SOURCES}
    ${PR_TERRAIN_LIB_SOURCES}
    ${ETER_LOCALE_SOURCES}
    ${C_WEB_BROWSER_SOURCES}
    ${PLATFORM_SOURCES}
)

if(M2_TARGET STREQUAL "android")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Extern/lib/android/${ANDROID_ABI})
    add_library(metin2_mobile SHARED ${M2_CORE_SOURCES})
elseif(M2_TARGET STREQUAL "web")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Extern/lib/web)
    add_executable(metin2_web ${M2_CORE_SOURCES})
    # .html output makes emscripten emit metin2_web.html (from
    # web/shell.html) plus metin2_web.js/.wasm
    set_target_properties(metin2_web PROPERTIES SUFFIX .html)
endif()

# Common Metin2 Libraries (Static). jpeg/png are excluded on web — the
# emscripten ports (-sUSE_LIBPNG/-sUSE_LIBJPEG) supply them there.
set(METIN2_EXTERN_LIBS
    python2.7
    cryptopp
    lzo2
)
if(M2_TARGET STREQUAL "android")
    list(APPEND METIN2_EXTERN_LIBS jpeg png)
endif()

if(M2_TARGET STREQUAL "android")
    set(EXTERN_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../Extern/lib/android/${ANDROID_ABI})
    link_directories(${EXTERN_LIB_PATH})

    find_library(log-lib log)
    find_library(android-lib android)
    find_library(gles3-lib GLESv3)

    target_compile_options(metin2_mobile PRIVATE -include windows.h)

    target_link_libraries(metin2_mobile
        OpenGr2ndma::opengr2ndma
        ${log-lib}
        ${android-lib}
        ${gles3-lib}
        EGL
        ${METIN2_EXTERN_LIBS}
        z
        m
    )
elseif(M2_TARGET STREQUAL "web")
    set(EXTERN_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../Extern/lib/web)
    link_directories(${EXTERN_LIB_PATH})

    target_compile_options(metin2_web PRIVATE -include windows.h)
    # BEFORE: pyconfig.h (ILP32, no fork/dlopen) must shadow the posix one in
    # Extern/include/Python2 — Python.h includes it with angle brackets.
    target_include_directories(metin2_web BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Extern/include/Python2-web)

    # emscripten flags were applied by web/emflags.cmake (see web/README.md)
    target_link_libraries(metin2_web
        OpenGr2ndma::opengr2ndma
        ${METIN2_EXTERN_LIBS}
        z
        m
    )
endif()
