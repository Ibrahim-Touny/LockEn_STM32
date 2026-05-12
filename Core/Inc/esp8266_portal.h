#ifndef ESP8266_PORTAL_H
#define ESP8266_PORTAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool EspPortal_InitSoftApHttp(const char *ssid, const char *pass);
void EspPortal_Poll(void);

void EspPortal_SetLcdLines(const char *line1, const char *line2);
bool EspPortal_TryGetRemoteKey(char *outChar);

#ifdef __cplusplus
}
#endif

#endif
