#include "esp8266_portal.h"
#include "ESP8266.h"
#include <stdio.h>
#include <string.h>

static char s_lcd1[17] = "";
static char s_lcd2[17] = "";

static volatile bool s_remoteKeyPending = false;
static volatile char s_remoteKey        = 0;

/* HTML must stay under 1024 bytes (TX buffer limit).
 * Polls /lcd every 400 ms. Script is as compact as possible.             */
static const char s_indexHtml[] =
    "<!doctype html><html><head>"
    "<meta charset='utf-8'/>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
    "<title>Lock</title></head>"
    "<body style='font-family:monospace;padding:10px'>"
    "<pre id='d' style='background:#000;color:#0f0;padding:8px;font-size:18px'>...</pre>"
    "<p id='s' style='color:#888;font-size:11px'>...</p>"
    "<input id='k' placeholder='Enter=# Backspace=*' autofocus/>"
    "<script>"
    "var n=0;"
    "function t(){"
    "fetch('/lcd').then(function(r){return r.json();})"
    ".then(function(j){"
    "document.getElementById('d').textContent=(j.l1||'').padEnd(16)+'\\n'+(j.l2||'').padEnd(16);"
    "document.getElementById('s').textContent='ok';n=0;"
    "}).catch(function(){n++;document.getElementById('s').textContent='err'+n;})}"
    "setInterval(t,400);t();"
    "document.getElementById('k').addEventListener('keydown',function(e){"
    "var u=null;"
    "if(e.key==='Enter')u='/key?c=%0D';"
    "else if(e.key==='Backspace')u='/key?c=%08';"
    "else if(e.key.length===1)u='/key?c='+encodeURIComponent(e.key);"
    "if(u){fetch(u);e.preventDefault();}"
    "});"
    "</script></body></html>\n";

/* Send HTTP response.
 * Do NOT call Wifi_RxClear() here — the ESP library already does it
 * internally before each AT+CIPSEND, and calling it ourselves would
 * wipe the next +IPD that may already be queued in the buffer.           */
static void http_send(uint8_t linkId, const char *body, const char *contentType)
{
    char header[128];
    int bodyLen   = (int)strlen(body);
    int headerLen = snprintf(header, sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %d\r\n"
                             "Connection: close\r\n\r\n",
                             contentType, bodyLen);
    if (headerLen <= 0) { Wifi_TcpIp_Close(linkId); return; }

    if (!Wifi_TcpIp_SendDataTcp(linkId, (uint16_t)headerLen, (uint8_t *)header))
    {
        Wifi_TcpIp_Close(linkId);
        return;
    }
    Wifi_TcpIp_SendDataTcp(linkId, (uint16_t)bodyLen, (uint8_t *)body);
    Wifi_TcpIp_Close(linkId);
}

static char url_decode_char(const char *p)
{
    if (p[0] == '%' && p[1] && p[2])
    {
        int hi = (p[1] >= 'a') ? p[1]-'a'+10 : (p[1] >= 'A') ? p[1]-'A'+10 : p[1]-'0';
        int lo = (p[2] >= 'a') ? p[2]-'a'+10 : (p[2] >= 'A') ? p[2]-'A'+10 : p[2]-'0';
        return (char)((hi << 4) | lo);
    }
    return p[0];
}

static bool parse_ipd(const char *rx, uint8_t *outLinkId, const char **outPayload)
{
    const char *p = strstr(rx, "+IPD,");
    if (!p) return false;
    p += 5;
    int linkId = 0, len = 0;
    if (sscanf(p, "%d,%d:", &linkId, &len) != 2) return false;
    const char *colon = strchr(p, ':');
    if (!colon) return false;
    *outLinkId  = (uint8_t)linkId;
    *outPayload = colon + 1;
    (void)len;
    return true;
}

static void handle_http(uint8_t linkId, const char *payload)
{
    if (strncmp(payload, "GET / ",    6) == 0 ||
        strncmp(payload, "GET /HTT", 8) == 0)
    {
        http_send(linkId, s_indexHtml, "text/html; charset=utf-8");
        return;
    }
    if (strncmp(payload, "GET /lcd", 8) == 0)
    {
        char json[64];
        snprintf(json, sizeof(json),
                 "{\"l1\":\"%s\",\"l2\":\"%s\"}\n", s_lcd1, s_lcd2);
        http_send(linkId, json, "application/json");
        return;
    }
    if (strncmp(payload, "GET /key?c=", 11) == 0)
    {
        s_remoteKey        = url_decode_char(payload + 11);
        s_remoteKeyPending = true;
        http_send(linkId, "OK\n", "text/plain");
        return;
    }
    http_send(linkId, "404\n", "text/plain");
}

/* ── Public API ─────────────────────────────────────────────────────────── */

bool EspPortal_InitSoftApHttp(const char *ssid, const char *pass)
{
    bool init_ok = false;
    for (int attempt = 0; attempt < 4 && !init_ok; attempt++)
    {
        Wifi_RxClear();
        HAL_Delay(400);
        Wifi_RxClear();
        init_ok = Wifi_Init();
    }
    if (!init_ok) return false;

    if (!Wifi_SetMode(WifiMode_SoftAp))                                    return false;
    if (!Wifi_SoftAp_Create((char*)ssid, (char*)pass, 1,
                             WifiEncryptionType_WPA2_PSK, 4, false))       return false;
    if (!Wifi_TcpIp_SetMultiConnection(true))                              return false;
    if (!Wifi_TcpIp_SetEnableTcpServer(80))                                return false;
    return true;
}

void EspPortal_Poll(void)
{
    uint8_t     linkId  = 0;
    const char *payload = NULL;

    if (!parse_ipd((const char *)Wifi.RxBuffer, &linkId, &payload))
        return;

    handle_http(linkId, payload);

    /* Clear AFTER the full response is sent so we don't wipe a second
     * +IPD that arrived while we were transmitting.                      */
    Wifi_RxClear();
}

void EspPortal_SetLcdLines(const char *line1, const char *line2)
{
    if (!line1) line1 = "";
    if (!line2) line2 = "";
    strncpy(s_lcd1, line1, 16); s_lcd1[16] = '\0';
    strncpy(s_lcd2, line2, 16); s_lcd2[16] = '\0';
}

bool EspPortal_TryGetRemoteKey(char *outChar)
{
    if (!outChar || !s_remoteKeyPending) return false;
    *outChar           = s_remoteKey;
    s_remoteKeyPending = false;
    return true;
}