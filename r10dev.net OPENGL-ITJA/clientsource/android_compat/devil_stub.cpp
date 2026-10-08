#ifdef M2_PORT
#include <IL/il.h>

extern "C" {
ILAPI void ILAPIENTRY ilInit(void) {}
ILAPI void ILAPIENTRY ilShutDown(void) {}
ILAPI void ILAPIENTRY ilGenImages(ILsizei Num, ILuint* Images) { static ILuint s_next = 1; for (ILsizei i = 0; i < Num; ++i) Images[i] = s_next++; }
ILAPI void ILAPIENTRY ilDeleteImages(ILsizei, const ILuint*) {}
ILAPI void ILAPIENTRY ilBindImage(ILuint) {}
ILAPI ILboolean ILAPIENTRY ilEnable(ILenum) { return IL_TRUE; }
ILAPI ILboolean ILAPIENTRY ilOriginFunc(ILenum) { return IL_TRUE; }
ILAPI ILint ILAPIENTRY ilGetInteger(ILenum) { return 0; }
ILAPI ILboolean ILAPIENTRY ilLoad(ILenum, ILconst_string) { return IL_FALSE; }
ILAPI ILboolean ILAPIENTRY ilSave(ILenum, ILconst_string) { return IL_FALSE; }
ILAPI ILboolean ILAPIENTRY ilConvertImage(ILenum, ILenum) { return IL_FALSE; }
ILAPI ILuint ILAPIENTRY ilCopyPixels(ILuint, ILuint, ILuint, ILuint, ILuint, ILuint, ILenum, ILenum, void*) { return 0; }
ILAPI void ILAPIENTRY ilSetPixels(ILint, ILint, ILint, ILuint, ILuint, ILuint, ILenum, ILenum, void*) {}
ILAPI ILboolean ILAPIENTRY ilTexImage(ILuint, ILuint, ILuint, ILubyte, ILenum, ILenum, void*) { return IL_FALSE; }
}
#endif
