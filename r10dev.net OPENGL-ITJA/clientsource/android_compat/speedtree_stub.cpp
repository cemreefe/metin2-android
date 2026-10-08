#ifdef M2_PORT
#include <SpeedTreeRT.h>

void CSpeedTreeRT::SetTime(float) {}
void CSpeedTreeRT::SetNumWindMatrices(unsigned int) {}
float CSpeedTreeRT::SetWindStrength(float fNewStrength, float, float) { return fNewStrength; }
#endif
