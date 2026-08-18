// machineinfo_gui.c
// Native Win32 GUI app: Computer Name, User Name, Serial Number (BIOS/WMI),
// IP + MAC Address (per adapter), Monitor Model + Serial Number (WMI WmiMonitorID)
// Pure WinAPI + COM/WMI, static-linked.

#define COBJMACROS
#define _WIN32_DCOM
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <uxtheme.h>
#include <richedit.h>
#include <windowsx.h>
#include <dxgi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

wchar_t *U8W(const char *s) {
 int wlen = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
 wchar_t *buf = (wchar_t *)malloc(wlen * sizeof(wchar_t));
 MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, wlen);
 return buf;
}

#define APP_TITLE "Machine Info Tool"
#define IDI_ICON1 101

#define ID_VAL_COMPNAME 101
#define ID_VAL_USERNAME 102
#define ID_VAL_SERIAL 103
#define ID_VAL_IP 104
#define ID_VAL_MONITOR 105
#define ID_VAL_MODEL 106
#define ID_VAL_CPU 107
#define ID_VAL_WINVER 108
#define ID_VAL_RAM 109
#define ID_VAL_GPU 110
#define ID_VAL_DISK 111
#define ID_VAL_UPTIME 112

HBRUSH hBrushValue;
HBRUSH hBrushBg;
HFONT hFontLabel;
HFONT hFontValue;

char g_compName[256] = "N/A";
char g_userName[256] = "N/A";
char g_serial[512] = "N/A";
char g_ipMacList[4096] = "N/A";
char g_monitorInfo[4096] = "N/A";
char g_sysModel[256] = "N/A";
char g_modelSN[600] = "N/A";
char g_cpuName[256] = "N/A";
char g_winVersion[256] = "N/A";
char g_ramInfo[128] = "N/A";
char g_gpuInfo[1024] = "N/A";
char g_diskInfo[1024] = "N/A";
char g_uptimeInfo[128] = "N/A";

HRESULT ConnectWMI(const wchar_t *ns, IWbemServices **ppSvc, IWbemLocator **ppLoc) {
 HRESULT hres = CoCreateInstance(&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
 &IID_IWbemLocator, (LPVOID *)ppLoc);
 if (FAILED(hres)) return hres;

 BSTR bstrNamespace = SysAllocString(ns);
 hres = IWbemLocator_ConnectServer(*ppLoc, bstrNamespace, NULL, NULL, NULL, 0, NULL, NULL, ppSvc);
 SysFreeString(bstrNamespace);
 if (FAILED(hres)) { IWbemLocator_Release(*ppLoc); return hres; }

 CoSetProxyBlanket((IUnknown *)*ppSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
 RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
 return S_OK;
}

BOOL GetSerialViaWMI(char *outBuf, size_t outBufSize) {
 BOOL success = FALSE;
 IWbemLocator *pLoc = NULL;
 IWbemServices *pSvc = NULL;

 if (FAILED(ConnectWMI(L"ROOT\\CIMV2", &pSvc, &pLoc))) return FALSE;

 IEnumWbemClassObject *pEnumerator = NULL;
 BSTR bstrWQL = SysAllocString(L"WQL");
 BSTR bstrQuery = SysAllocString(L"SELECT SerialNumber FROM Win32_BIOS");
 HRESULT hres = IWbemServices_ExecQuery(pSvc, bstrWQL, bstrQuery,
 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
 SysFreeString(bstrWQL);
 SysFreeString(bstrQuery);

 if (SUCCEEDED(hres) && pEnumerator) {
 IWbemClassObject *pclsObj = NULL;
 ULONG uReturn = 0;
 IEnumWbemClassObject_Next(pEnumerator, WBEM_INFINITE, 1, &pclsObj, &uReturn);
 if (uReturn != 0) {
 VARIANT vtProp;
 VariantInit(&vtProp);
 if (SUCCEEDED(IWbemClassObject_Get(pclsObj, L"SerialNumber", 0, &vtProp, 0, 0))
 && vtProp.vt == VT_BSTR && vtProp.bstrVal != NULL) {
 WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, outBuf, (int)outBufSize, NULL, NULL);
 success = TRUE;
 }
 VariantClear(&vtProp);
 IWbemClassObject_Release(pclsObj);
 }
 IEnumWbemClassObject_Release(pEnumerator);
 }

 IWbemServices_Release(pSvc);
 IWbemLocator_Release(pLoc);
 return success;
}

BOOL GetSerialViaRegistry(char *outBuf, size_t outBufSize) {
 HKEY hKey;
 const char *path = "HARDWARE\\DESCRIPTION\\System\\BIOS";
 BOOL success = FALSE;

 if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
 char buf[256];
 DWORD bufSize = sizeof(buf);
 DWORD type;
 if (RegQueryValueExA(hKey, "SystemSerialNumber", NULL, &type, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
 strncpy(outBuf, buf, outBufSize - 1);
 success = TRUE;
 } else {
 bufSize = sizeof(buf);
 if (RegQueryValueExA(hKey, "BaseBoardSerialNumber", NULL, &type, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
 strncpy(outBuf, buf, outBufSize - 1);
 success = TRUE;
 }
 }
 RegCloseKey(hKey);
 }
 return success;
}

void LoadComputerAndUser() {
 DWORD size = sizeof(g_compName);
 if (!GetComputerNameA(g_compName, &size)) strcpy(g_compName, "N/A");

 DWORD usize = sizeof(g_userName);
 if (!GetUserNameA(g_userName, &usize)) strcpy(g_userName, "N/A");
}

void LoadSerialNumber() {
 strcpy(g_serial, "N/A");
 char buf[512] = {0};

 if (GetSerialViaWMI(buf, sizeof(buf)) && strlen(buf) > 0) {
 strncpy(g_serial, buf, sizeof(g_serial) - 1);
 } else if (GetSerialViaRegistry(buf, sizeof(buf)) && strlen(buf) > 0) {
 strncpy(g_serial, buf, sizeof(g_serial) - 1);
 }

 int len = (int)strlen(g_serial);
 while (len > 0 && (g_serial[len-1] == ' ' || g_serial[len-1] == '\r' || g_serial[len-1] == '\n')) {
 g_serial[--len] = '\0';
 }
 if (len == 0) strcpy(g_serial, "N/A");
}

void LoadSystemModel() {
 strcpy(g_sysModel, "N/A");
 HKEY hKey;
 const char *path = "HARDWARE\\DESCRIPTION\\System\\BIOS";

 if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
 char manuf[128] = "";
 char product[128] = "";
 DWORD bufSize, type;

 bufSize = sizeof(manuf);
 RegQueryValueExA(hKey, "SystemManufacturer", NULL, &type, (LPBYTE)manuf, &bufSize);
 bufSize = sizeof(product);
 RegQueryValueExA(hKey, "SystemProductName", NULL, &type, (LPBYTE)product, &bufSize);
 RegCloseKey(hKey);

 if (strlen(manuf) > 0 && strlen(product) > 0) {
 snprintf(g_sysModel, sizeof(g_sysModel), "%s %s", manuf, product);
 } else if (strlen(product) > 0) {
 strncpy(g_sysModel, product, sizeof(g_sysModel) - 1);
 } else if (strlen(manuf) > 0) {
 strncpy(g_sysModel, manuf, sizeof(g_sysModel) - 1);
 }
 }
}

void LoadCPUName() {
 strcpy(g_cpuName, "N/A");
 HKEY hKey;
 const char *path = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

 if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
 char buf[256] = "";
 DWORD bufSize = sizeof(buf);
 DWORD type;
 if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, &type, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
 char *start = buf;
 while (*start == ' ') start++;
 int len = (int)strlen(start);
 while (len > 0 && start[len-1] == ' ') { start[--len] = '\0'; }
 strncpy(g_cpuName, start, sizeof(g_cpuName) - 1);
 }
 RegCloseKey(hKey);
 }
}

void LoadWinVersion() {
 strcpy(g_winVersion, "N/A");
 HKEY hKey;
 const char *path = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

 if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
 char productName[128] = "";
 char displayVersion[64] = "";
 char buildNumber[64] = "";
 DWORD ubr = 0;
 DWORD bufSize, type;

 bufSize = sizeof(productName);
 RegQueryValueExA(hKey, "ProductName", NULL, &type, (LPBYTE)productName, &bufSize);

 bufSize = sizeof(displayVersion);
 if (RegQueryValueExA(hKey, "DisplayVersion", NULL, &type, (LPBYTE)displayVersion, &bufSize) != ERROR_SUCCESS) {
 bufSize = sizeof(displayVersion);
 RegQueryValueExA(hKey, "ReleaseId", NULL, &type, (LPBYTE)displayVersion, &bufSize);
 }

 bufSize = sizeof(buildNumber);
 RegQueryValueExA(hKey, "CurrentBuildNumber", NULL, &type, (LPBYTE)buildNumber, &bufSize);

 bufSize = sizeof(DWORD);
 RegQueryValueExA(hKey, "UBR", NULL, &type, (LPBYTE)&ubr, &bufSize);

 if (strlen(productName) == 0) strcpy(productName, "Windows");

 long buildNum = strlen(buildNumber) > 0 ? atol(buildNumber) : 0;
 if (buildNum >= 22000) {
 char *pos = strstr(productName, "Windows 10");
 if (pos) {
 pos[9] = '1';
 }
 }

 if (strlen(buildNumber) > 0) {
 snprintf(g_winVersion, sizeof(g_winVersion), "%s %s (Build %s.%lu)",
 productName, displayVersion, buildNumber, ubr);
 } else {
 strncpy(g_winVersion, productName, sizeof(g_winVersion) - 1);
 }

 RegCloseKey(hKey);
 }
}

const char *DDRTypeFromSMBIOSCode(int code) {
 switch (code) {
 case 20: return "DDR";
 case 21: return "DDR2";
 case 22: return "DDR2 FB-DIMM";
 case 24: return "DDR3";
 case 26: return "DDR4";
 case 34: return "DDR5";
 default: return NULL;
 }
}

void FormatSizeGBorMB(double gb, char *out, size_t outSize) {
 if (gb < 1.0) {
 snprintf(out, outSize, "%.0f MB", gb * 1024.0);
 } else {
 snprintf(out, outSize, "%.1f GB", gb);
 }
}

void LoadRAMInfo() {
 strcpy(g_ramInfo, "N/A");
 MEMORYSTATUSEX statex;
 statex.dwLength = sizeof(statex);
 double totalGB = 0;
 if (GlobalMemoryStatusEx(&statex)) {
 totalGB = (double)statex.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
 }

 char ddrPart[64] = "";
 IWbemLocator *pLoc = NULL;
 IWbemServices *pSvc = NULL;
 if (SUCCEEDED(ConnectWMI(L"ROOT\\CIMV2", &pSvc, &pLoc))) {
 IEnumWbemClassObject *pEnumerator = NULL;
 BSTR bstrWQL = SysAllocString(L"WQL");
 BSTR bstrQuery = SysAllocString(L"SELECT Speed, ConfiguredClockSpeed, SMBIOSMemoryType, MemoryType FROM Win32_PhysicalMemory");
 HRESULT hres = IWbemServices_ExecQuery(pSvc, bstrWQL, bstrQuery,
 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
 SysFreeString(bstrWQL);
 SysFreeString(bstrQuery);

 if (SUCCEEDED(hres) && pEnumerator) {
 IWbemClassObject *pclsObj = NULL;
 ULONG uReturn = 0;
 IEnumWbemClassObject_Next(pEnumerator, WBEM_INFINITE, 1, &pclsObj, &uReturn);
 if (uReturn != 0) {
 VARIANT vSpeed, vConfigSpeed, vSmType, vMemType;
 VariantInit(&vSpeed);
 VariantInit(&vConfigSpeed);
 VariantInit(&vSmType);
 VariantInit(&vMemType);

 IWbemClassObject_Get(pclsObj, L"Speed", 0, &vSpeed, 0, 0);
 IWbemClassObject_Get(pclsObj, L"ConfiguredClockSpeed", 0, &vConfigSpeed, 0, 0);
 IWbemClassObject_Get(pclsObj, L"SMBIOSMemoryType", 0, &vSmType, 0, 0);
 IWbemClassObject_Get(pclsObj, L"MemoryType", 0, &vMemType, 0, 0);

 const char *ddrName = NULL;
 if (vSmType.vt == VT_I4 || vSmType.vt == VT_UI4) ddrName = DDRTypeFromSMBIOSCode(vSmType.intVal);
 if (!ddrName && (vMemType.vt == VT_I4 || vMemType.vt == VT_UI4)) ddrName = DDRTypeFromSMBIOSCode(vMemType.intVal);

 int speedMHz = 0;
 if (vConfigSpeed.vt == VT_I4 || vConfigSpeed.vt == VT_UI4) speedMHz = vConfigSpeed.intVal;
 if (speedMHz == 0 && (vSpeed.vt == VT_I4 || vSpeed.vt == VT_UI4)) speedMHz = vSpeed.intVal;

 if (ddrName && speedMHz > 0) {
 snprintf(ddrPart, sizeof(ddrPart), " %s @ %d MT/s", ddrName, speedMHz);
 } else if (ddrName) {
 snprintf(ddrPart, sizeof(ddrPart), " %s", ddrName);
 }

 VariantClear(&vSpeed);
 VariantClear(&vConfigSpeed);
 VariantClear(&vSmType);
 VariantClear(&vMemType);
 IWbemClassObject_Release(pclsObj);
 }
 IEnumWbemClassObject_Release(pEnumerator);
 }
 IWbemServices_Release(pSvc);
 IWbemLocator_Release(pLoc);
 }

 char ramSizeStr[32];
 FormatSizeGBorMB(totalGB, ramSizeStr, sizeof(ramSizeStr));
 snprintf(g_ramInfo, sizeof(g_ramInfo), "%s%s", ramSizeStr, ddrPart);
}

void LoadGPUInfo() {
 g_gpuInfo[0] = '\0';
 IDXGIFactory1 *pFactory = NULL;
 if (SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&pFactory))) {
 IDXGIAdapter1 *pAdapter = NULL;
 UINT i = 0;
 while (IDXGIFactory1_EnumAdapters1(pFactory, i, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
 DXGI_ADAPTER_DESC1 desc;
 if (SUCCEEDED(IDXGIAdapter1_GetDesc1(pAdapter, &desc))) {
 if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
 char name[256];
 WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), NULL, NULL);
 double vramGB = (double)desc.DedicatedVideoMemory / (1024.0 * 1024.0 * 1024.0);

 char sizeStr[32];
 FormatSizeGBorMB(vramGB, sizeStr, sizeof(sizeStr));

 char line[350];
 if (desc.DedicatedVideoMemory > 0) {
 snprintf(line, sizeof(line), "%s (%s VRAM)", name, sizeStr);
 } else {
 snprintf(line, sizeof(line), "%s", name);
 }
 if (strlen(g_gpuInfo) > 0) strcat(g_gpuInfo, "\r\n");
 strcat(g_gpuInfo, line);
 }
 }
 IDXGIAdapter1_Release(pAdapter);
 i++;
 }
 IDXGIFactory1_Release(pFactory);
 }
 if (g_gpuInfo[0] == '\0') strcpy(g_gpuInfo, "N/A");
}

void LoadDiskInfo() {
 g_diskInfo[0] = '\0';
 IWbemLocator *pLoc = NULL;
 IWbemServices *pSvc = NULL;
 if (SUCCEEDED(ConnectWMI(L"ROOT\\CIMV2", &pSvc, &pLoc))) {
 IEnumWbemClassObject *pEnumerator = NULL;
 BSTR bstrWQL = SysAllocString(L"WQL");
 BSTR bstrQuery = SysAllocString(L"SELECT Model, Size FROM Win32_DiskDrive");
 HRESULT hres = IWbemServices_ExecQuery(pSvc, bstrWQL, bstrQuery,
 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
 SysFreeString(bstrWQL);
 SysFreeString(bstrQuery);

 int diskIndex = 0;
 if (SUCCEEDED(hres) && pEnumerator) {
 IWbemClassObject *pclsObj = NULL;
 ULONG uReturn = 0;
 while (1) {
 IEnumWbemClassObject_Next(pEnumerator, WBEM_INFINITE, 1, &pclsObj, &uReturn);
 if (uReturn == 0) break;
 diskIndex++;

 VARIANT vModel, vSize;
 VariantInit(&vModel);
 VariantInit(&vSize);
 IWbemClassObject_Get(pclsObj, L"Model", 0, &vModel, 0, 0);
 IWbemClassObject_Get(pclsObj, L"Size", 0, &vSize, 0, 0);

 char model[128] = "Unknown Disk";
 if (vModel.vt == VT_BSTR && vModel.bstrVal) {
 WideCharToMultiByte(CP_UTF8, 0, vModel.bstrVal, -1, model, sizeof(model), NULL, NULL);
 }

 double sizeGB = 0;
 if (vSize.vt == VT_BSTR && vSize.bstrVal) {
 char sizeBuf[32];
 WideCharToMultiByte(CP_UTF8, 0, vSize.bstrVal, -1, sizeBuf, sizeof(sizeBuf), NULL, NULL);
 double bytes = _atoi64(sizeBuf) * 1.0;
 sizeGB = bytes / (1024.0 * 1024.0 * 1024.0);
 }

 char line[200];
 snprintf(line, sizeof(line), "Disk %d: %s (%.0f GB)", diskIndex, model, sizeGB);
 if (strlen(g_diskInfo) > 0) strcat(g_diskInfo, "\r\n");
 strcat(g_diskInfo, line);

 VariantClear(&vModel);
 VariantClear(&vSize);
 IWbemClassObject_Release(pclsObj);
 }
 IEnumWbemClassObject_Release(pEnumerator);
 }
 IWbemServices_Release(pSvc);
 IWbemLocator_Release(pLoc);
 }
 if (g_diskInfo[0] == '\0') strcpy(g_diskInfo, "N/A");
}

// ---------- เวลาที่เครื่องทำงานมาแล้ว (System Uptime) ----------
// ใช้ GetTickCount64 (ตั้งแต่ Windows Vista/Server 2008 ขึ้นไป) นับ ms ตั้งแต่บูตเครื่องล่าสุด
// รูปแบบสั้นแบบ status bar เช่น "Uptime 1 Day 16 Hour 41 Min"
void LoadUptime() {
    ULONGLONG ms = GetTickCount64();
    ULONGLONG totalSec = ms / 1000;
    ULONGLONG days = totalSec / 86400;
    ULONGLONG hours = (totalSec % 86400) / 3600;
    ULONGLONG minutes = (totalSec % 3600) / 60;

    if (days > 0) {
        snprintf(g_uptimeInfo, sizeof(g_uptimeInfo), "Uptime %llu Day %llu Hour %llu Min", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(g_uptimeInfo, sizeof(g_uptimeInfo), "Uptime %llu Hour %llu Min", hours, minutes);
    } else {
        snprintf(g_uptimeInfo, sizeof(g_uptimeInfo), "Uptime %llu Min", minutes);
    }
}

void LoadIPAndMac() {
 g_ipMacList[0] = '\0';
 ULONG bufLen = 15000;
 PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(bufLen);
 if (!pAddresses) { strcpy(g_ipMacList, "N/A"); return; }

 ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
 DWORD ret = GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen);
 if (ret == ERROR_BUFFER_OVERFLOW) {
 free(pAddresses);
 pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(bufLen);
 ret = GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &bufLen);
 }

 if (ret != NO_ERROR) { strcpy(g_ipMacList, "N/A"); free(pAddresses); return; }

 for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr; pCurr = pCurr->Next) {
 if (pCurr->OperStatus != IfOperStatusUp) continue;
 if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

 char mac[32] = "N/A";
 if (pCurr->PhysicalAddressLength == 6) {
 snprintf(mac, sizeof(mac), "%02X-%02X-%02X-%02X-%02X-%02X",
 pCurr->PhysicalAddress[0], pCurr->PhysicalAddress[1],
 pCurr->PhysicalAddress[2], pCurr->PhysicalAddress[3],
 pCurr->PhysicalAddress[4], pCurr->PhysicalAddress[5]);
 }

 for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress;
 pUnicast; pUnicast = pUnicast->Next) {
 SOCKADDR *sa = pUnicast->Address.lpSockaddr;
 if (sa->sa_family == AF_INET) {
 char ipStr[INET6_ADDRSTRLEN] = {0};
 struct sockaddr_in *sa_in = (struct sockaddr_in *)sa;
 inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, sizeof(ipStr));
 if (strncmp(ipStr, "127.", 4) == 0) continue;

 char line[128];
 snprintf(line, sizeof(line), "%s   (%s)", ipStr, mac);
 if (strlen(g_ipMacList) > 0) strcat(g_ipMacList, "\r\n");
 strcat(g_ipMacList, line);
 }
 }
 }
 if (g_ipMacList[0] == '\0') strcpy(g_ipMacList, "N/A");
 free(pAddresses);
}

void Uint16ArrayToString(VARIANT *v, char *out, size_t outSize) {
 out[0] = '\0';
 if ((v->vt & VT_ARRAY) == 0) return;
 SAFEARRAY *psa = v->parray;
 if (!psa) return;

 long lBound = 0, uBound = -1;
 SafeArrayGetLBound(psa, 1, &lBound);
 SafeArrayGetUBound(psa, 1, &uBound);
 long count = uBound - lBound + 1;
 if (count <= 0) return;

 USHORT *data = NULL;
 if (FAILED(SafeArrayAccessData(psa, (void **)&data))) return;

 size_t j = 0;
 for (long i = 0; i < count && j < outSize - 1; i++) {
 if (data[i] != 0) {
 out[j++] = (char)data[i];
 }
 }
 out[j] = '\0';
 SafeArrayUnaccessData(psa);
}

const char *ResolveManufacturerName(const char *pnpId) {
 static const struct { const char *code; const char *name; } table[] = {
 {"LEN", "Lenovo"}, {"DEL", "Dell"}, {"SAM", "Samsung"},
 {"AUO", "AU Optronics"},{"LGD", "LG Display"}, {"LPL", "LG Philips"},
 {"ACI", "ASUS"}, {"ACR", "Acer"}, {"BNQ", "BenQ"},
 {"HWP", "HP"}, {"HPN", "HP"}, {"PHL", "Philips"},
 {"SNY", "Sony"}, {"VSC", "ViewSonic"}, {"GSM", "LG (Goldstar)"},
 {"CMN", "Chimei Innolux"}, {"CMO", "Chi Mei Optoelectronics"},
 {"SDC", "Samsung Display"}, {"AAA", "AOC"}, {"AOC", "AOC"},
 {"MSI", "MSI"}, {"IVM", "Iiyama"}, {"NEC", "NEC"},
 {"EIZ", "EIZO"}, {"FUS", "Fujitsu"}, {"CPQ", "Compaq"},
 {"IBM", "IBM"}, {"IQT", "Innolux"}, {"IFX", "Infineon"},
 {"IVO", "InfoVision"}, {"IPS", "InnoPanel"}, {"IOT", "I/O Data"},
 {"CTX", "CTX"}, {"IPD", "Ancor"}, {"HSD", "Hannspree/HannStar"},
 {"IZE", "Zenith"}, {"XER", "Xerox"}, {"XRT", "X-Rite"},
 {"VIT", "Viewtec"}, {"VIZ", "VIZIO"}, {"TSB", "Toshiba"},
 {"WDT", "Westinghouse"},{"UNK", "Unknown"},
 };
 for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
 if (_stricmp(pnpId, table[i].code) == 0) return table[i].name;
 }
 return pnpId;
}

void LoadMonitorInfo() {
 g_monitorInfo[0] = '\0';
 IWbemLocator *pLoc = NULL;
 IWbemServices *pSvc = NULL;

 if (FAILED(ConnectWMI(L"ROOT\\WMI", &pSvc, &pLoc))) {
 strcpy(g_monitorInfo, "N/A");
 return;
 }

 IEnumWbemClassObject *pEnumerator = NULL;
 BSTR bstrWQL = SysAllocString(L"WQL");
 BSTR bstrQuery = SysAllocString(L"SELECT UserFriendlyName, SerialNumberID, ManufacturerName FROM WmiMonitorID");
 HRESULT hres = IWbemServices_ExecQuery(pSvc, bstrWQL, bstrQuery,
 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
 SysFreeString(bstrWQL);
 SysFreeString(bstrQuery);

 int monitorIndex = 0;
 if (SUCCEEDED(hres) && pEnumerator) {
 IWbemClassObject *pclsObj = NULL;
 ULONG uReturn = 0;

 while (1) {
 IEnumWbemClassObject_Next(pEnumerator, WBEM_INFINITE, 1, &pclsObj, &uReturn);
 if (uReturn == 0) break;
 monitorIndex++;

 char model[256] = "N/A";
 char serial[256] = "N/A";
 char manuf[256] = "";

 VARIANT vName, vSerial, vManuf;
 VariantInit(&vName);
 VariantInit(&vSerial);
 VariantInit(&vManuf);

 if (SUCCEEDED(IWbemClassObject_Get(pclsObj, L"UserFriendlyName", 0, &vName, 0, 0))) {
 Uint16ArrayToString(&vName, model, sizeof(model));
 }
 if (SUCCEEDED(IWbemClassObject_Get(pclsObj, L"SerialNumberID", 0, &vSerial, 0, 0))) {
 Uint16ArrayToString(&vSerial, serial, sizeof(serial));
 }
 if (SUCCEEDED(IWbemClassObject_Get(pclsObj, L"ManufacturerName", 0, &vManuf, 0, 0))) {
 Uint16ArrayToString(&vManuf, manuf, sizeof(manuf));
 }

 if (strlen(model) == 0) strcpy(model, "N/A");
 if (strlen(serial) == 0) strcpy(serial, "N/A");

 char line[600];
 if (strlen(manuf) > 0) {
 snprintf(line, sizeof(line), "Monitor %d: %s %s SN: %s", monitorIndex, ResolveManufacturerName(manuf), model, serial);
 } else {
 snprintf(line, sizeof(line), "Monitor %d: %s SN: %s", monitorIndex, model, serial);
 }

 if (strlen(g_monitorInfo) > 0) strcat(g_monitorInfo, "\r\n");
 strcat(g_monitorInfo, line);

 VariantClear(&vName);
 VariantClear(&vSerial);
 VariantClear(&vManuf);
 IWbemClassObject_Release(pclsObj);
 }
 IEnumWbemClassObject_Release(pEnumerator);
 }

 IWbemServices_Release(pSvc);
 IWbemLocator_Release(pLoc);

 if (monitorIndex == 0) strcpy(g_monitorInfo, "N/A");
}

WNDPROC g_origEditProc = NULL;

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
 if (msg == WM_CONTEXTMENU) {
 HMENU hMenu = CreatePopupMenu();
 AppendMenuW(hMenu, MF_STRING, 1, L"Copy");
 AppendMenuW(hMenu, MF_STRING, 2, L"Select All");

 POINT pt;
 pt.x = GET_X_LPARAM(lParam);
 pt.y = GET_Y_LPARAM(lParam);
 if (pt.x == -1 && pt.y == -1) {
 RECT rc;
 GetWindowRect(hwnd, &rc);
 pt.x = (rc.left + rc.right) / 2;
 pt.y = (rc.top + rc.bottom) / 2;
 }

 int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
 DestroyMenu(hMenu);

 if (cmd == 1) {
 SendMessageW(hwnd, WM_COPY, 0, 0);
 } else if (cmd == 2) {
 SendMessageW(hwnd, EM_SETSEL, 0, -1);
 }
 return 0;
 }
 return CallWindowProcW(g_origEditProc, hwnd, msg, wParam, lParam);
}

HWND CreateLabel(HWND parent, const char *text, int x, int y, int w, int h) {
 HWND h_ = CreateWindowExW(0, L"STATIC", U8W(text), WS_CHILD | WS_VISIBLE | SS_LEFT,
 x, y, w, h, parent, NULL, GetModuleHandle(NULL), NULL);
 SendMessage(h_, WM_SETFONT, (WPARAM)hFontLabel, TRUE);
 return h_;
}

HWND CreateValueBox(HWND parent, const char *text, int id, int x, int y, int w, int h, BOOL multiline) {
 DWORD style = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY;
 style |= multiline ? (ES_MULTILINE | ES_AUTOVSCROLL) : ES_AUTOHSCROLL;
 HWND h_ = CreateWindowExW(0, MSFTEDIT_CLASS, U8W(text), style,
 x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
 SendMessage(h_, WM_SETFONT, (WPARAM)hFontValue, TRUE);
 SendMessage(h_, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(173, 216, 230));

 // ปรับระยะขอบซ้ายและขวาให้พอดี ไม่ชิดและไม่ห่างจนเกินไป (หน่วยเป็นพิกเซล)
 SendMessage(h_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));

 if (!g_origEditProc) {
 g_origEditProc = (WNDPROC)GetWindowLongPtrW(h_, GWLP_WNDPROC);
 }
 SetWindowLongPtrW(h_, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

 return h_;
}

void ApplyBoldToIPPortion(HWND hEdit, const char *unused) {
    (void)unused;

    int lineCount = (int)SendMessageW(hEdit, EM_GETLINECOUNT, 0, 0);
    if (lineCount <= 0) return;

    // 1. สไตล์สำหรับเลข IP: ตัวหนา + สีดำปกติ
    CHARFORMAT2W cfIP;
    ZeroMemory(&cfIP, sizeof(cfIP));
    cfIP.cbSize = sizeof(CHARFORMAT2W);
    cfIP.dwMask = CFM_BOLD | CFM_COLOR;
    cfIP.dwEffects = CFE_BOLD;
    cfIP.crTextColor = RGB(0, 0, 0);

    // 2. สไตล์สำหรับ MAC Address ในวงเล็บ: ตัวบาง + สีเทาจาง
    CHARFORMAT2W cfMAC;
    ZeroMemory(&cfMAC, sizeof(cfMAC));
    cfMAC.cbSize = sizeof(CHARFORMAT2W);
    cfMAC.dwMask = CFM_BOLD | CFM_COLOR;
    cfMAC.dwEffects = 0; // ตัวบางปกติ
    cfMAC.crTextColor = RGB(128, 128, 128); // สีเทาจาง

    for (int i = 0; i < lineCount; i++) {
        int lineStart = (int)SendMessageW(hEdit, EM_LINEINDEX, i, 0);
        int lineLen = (int)SendMessageW(hEdit, EM_LINELENGTH, lineStart, 0);
        if (lineLen <= 0) continue;

        wchar_t buf[512];
        *((WORD *)buf) = (WORD)(sizeof(buf) / sizeof(wchar_t));
        int copied = (int)SendMessageW(hEdit, EM_GETLINE, i, (LPARAM)buf);
        if (copied <= 0) continue;
        buf[copied < 511 ? copied : 511] = L'\0';

        // ค้นหาตำแหน่งวงเล็บเปิด '(' ในบรรทัด
        wchar_t *marker = wcschr(buf, L'(');
        if (marker) {
            int ipLen = (int)(marker - buf);
            // ตัดช่องว่างส่วนเกินหน้าวงเล็บออก
            while (ipLen > 0 && buf[ipLen - 1] == L' ') {
                ipLen--;
            }

            // ช่วงที่ 1: เฉพาะเลข IP ด้านหน้า -> ตัวหนา + สีดำ
            SendMessageW(hEdit, EM_SETSEL, lineStart, lineStart + ipLen);
            SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfIP);

            // ช่วงที่ 2: ตั้งแต่วงเล็บเป็นต้นไป -> ตัวบาง + สีเทาจาง
            SendMessageW(hEdit, EM_SETSEL, lineStart + ipLen, lineStart + lineLen);
            SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfMAC);
        } else {
            // ถ้าบรรทัดไหนไม่มีวงเล็บ ให้แสดงเป็นตัวบางสีเทาปกติ
            SendMessageW(hEdit, EM_SETSEL, lineStart, lineStart + lineLen);
            SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfMAC);
        }
    }

    // ล้าง selection และคืนค่าเคอร์เซอร์
    SendMessageW(hEdit, EM_SETSEL, 0, 0);
}

typedef struct {
 const char *labelText;
 char *valueBuf;
 int valueId;
 BOOL multiline;
 HWND hLabel;
 HWND hValue;
} RowDef;

RowDef g_rowDefs[10] = {
 {"Computer Name :", g_compName, ID_VAL_COMPNAME, FALSE, NULL, NULL},
 {"Computer / SN :", g_modelSN, ID_VAL_MODEL, FALSE, NULL, NULL},
 {"Processor :", g_cpuName, ID_VAL_CPU, FALSE, NULL, NULL},
 {"RAM :", g_ramInfo, ID_VAL_RAM, FALSE, NULL, NULL},
 {"Graphics :", g_gpuInfo, ID_VAL_GPU, TRUE, NULL, NULL},
 {"Storage :", g_diskInfo, ID_VAL_DISK, TRUE, NULL, NULL},
 {"Monitor / SN :", g_monitorInfo,ID_VAL_MONITOR, TRUE, NULL, NULL},
 {"OS :", g_winVersion, ID_VAL_WINVER, FALSE, NULL, NULL},
 {"User Name :", g_userName, ID_VAL_USERNAME, FALSE, NULL, NULL},
 {"IP (MAC) :", g_ipMacList, ID_VAL_IP, TRUE, NULL, NULL},
};

int CountLines(const char *text) {
 int count = 1;
 for (const char *p = text; *p; p++) {
 if (*p == '\n') count++;
 }
 return count;
}

HWND g_hUptimeStatus = NULL;
HFONT hFontStatus = NULL;
#define ID_TIMER_UPTIME 1

void DoLayout(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int clientW = rc.right - rc.left;

    int labelX = 24, labelW = 120;
    int valueX = 165;
    int rightMargin = 24;
    int valueW = clientW - valueX - rightMargin;
    if (valueW < 100) valueW = 100;

    int rowH = 24;
    int topMargin = 18; 
    int gap = 10;       
    int lineHeightPx = 20; // ปรับความสูงต่อบรรทัดเพิ่มขึ้นจาก 18 เป็น 20 เพื่อให้มีช่องว่างไม่ชิดขอบ
    int multiPadding = 6;  // เพิ่ม Padding เผื่อระยะบน-ล่างให้กล่องสูงพอดีไม่ทับซ้อน

    int y = topMargin;
    for (int i = 0; i < 10; i++) {
        RowDef *r = &g_rowDefs[i];
        if (!r->hLabel || !r->hValue) continue;

        int boxH;
        if (!r->multiline) {
            boxH = rowH;
            // ปรับตำแหน่ง y ของกล่องข้อความธรรมดาให้ตรงกันพอดี ไม่ลอยขึ้นไปทับขอบ
            MoveWindow(r->hLabel, labelX, y + 2, labelW, rowH, TRUE);
            MoveWindow(r->hValue, valueX, y, valueW, rowH, TRUE);
        } else {
            int lines = CountLines(r->valueBuf);
            boxH = lines * lineHeightPx + multiPadding; 
            MoveWindow(r->hLabel, labelX, y + 2, labelW, rowH, TRUE);
            MoveWindow(r->hValue, valueX, y, valueW, boxH, TRUE);
        }
        y += boxH + gap;
    }

    // วางป้าย Uptime ไว้มุมขวาล่างของหน้าต่าง (แบบ status bar)
    if (g_hUptimeStatus) {
        int statusH = 20;
        int statusW = 260;
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int statusX = rcClient.right - statusW - rightMargin;
        int statusY = rcClient.bottom - statusH - 8;
        if (statusX < labelX) statusX = labelX;
        MoveWindow(g_hUptimeStatus, statusX, statusY, statusW, statusH, TRUE);
    }
}

int g_minWindowHeight = 400;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
 switch (msg) {
    case WM_CREATE: {
    hFontLabel = CreateFontA(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    hFontValue = CreateFontA(18, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        THAI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    hBrushValue = CreateSolidBrush(RGB(173, 216, 230));
    hBrushBg = CreateSolidBrush(RGB(240, 240, 240));

    for (int i = 0; i < 10; i++) {
        RowDef *r = &g_rowDefs[i];
        r->hLabel = CreateLabel(hwnd, r->labelText, 0, 0, 10, 10);
        r->hValue = CreateValueBox(hwnd, r->valueBuf, r->valueId, 0, 0, 10, 10, r->multiline);
    }

    // ป้ายสถานะ Uptime มุมขวาล่าง
    hFontStatus = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    g_hUptimeStatus = CreateWindowExW(0, L"STATIC", U8W(g_uptimeInfo),
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0, 0, 10, 10, hwnd, NULL, GetModuleHandle(NULL), NULL);

    SendMessage(g_hUptimeStatus, WM_SETFONT, (WPARAM)hFontStatus, TRUE);

    SetTimer(hwnd, ID_TIMER_UPTIME, 60000, NULL);

DoLayout(hwnd);
ApplyBoldToIPPortion(g_rowDefs[9].hValue, g_ipMacList);

    return 0;
}

 case WM_TIMER: {
 if (wParam == ID_TIMER_UPTIME) {
 LoadUptime();
 SetWindowTextW(g_hUptimeStatus, U8W(g_uptimeInfo));
 }
 return 0;
 }

case WM_SIZE:
    if (wParam != SIZE_MINIMIZED) {
        DoLayout(hwnd);
        if (g_rowDefs[9].hValue) {
            ApplyBoldToIPPortion(g_rowDefs[9].hValue, g_ipMacList);
        }
    }
    return 0;

 case WM_GETMINMAXINFO: {
 MINMAXINFO *mmi = (MINMAXINFO *)lParam;
 mmi->ptMinTrackSize.x = 560;
 mmi->ptMinTrackSize.y = g_minWindowHeight;
 return 0;
 }

 case WM_CTLCOLOREDIT: {
 HDC hdc = (HDC)wParam;
 SetBkColor(hdc, RGB(173, 216, 230));
 SetTextColor(hdc, RGB(0, 0, 0));
 return (LRESULT)hBrushValue;
 }

 case WM_CTLCOLORSTATIC: {
 HDC hdc = (HDC)wParam;
 SetBkMode(hdc, TRANSPARENT);
 if ((HWND)lParam == g_hUptimeStatus) {
 SetTextColor(hdc, RGB(110, 110, 110));  // สีเทา ให้ดูเป็น status bar แยกจาก label ปกติ
 }
 return (LRESULT)hBrushBg;
 }

 case WM_ERASEBKGND: {
 HDC hdc = (HDC)wParam;
 RECT rc;
 GetClientRect(hwnd, &rc);
 FillRect(hdc, &rc, hBrushBg);
 return 1;
 }

 case WM_DESTROY:
 KillTimer(hwnd, ID_TIMER_UPTIME);
 DeleteObject(hBrushValue);
 DeleteObject(hBrushBg);
 DeleteObject(hFontLabel);
 DeleteObject(hFontValue);
 DeleteObject(hFontStatus);
 PostQuitMessage(0);
 return 0;
 }
 return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int ComputeIdealContentHeight() {
    int topMargin = 18;      
    int gap = 10;            
    int rowH = 24;
    int lineHeightPx = 20; 
    int multiPadding = 6;
    int bottomMargin = 24;   // +28 เผื่อพื้นที่ให้ป้าย Uptime ที่มุมขวาล่าง

    int total = topMargin;
    for (int i = 0; i < 10; i++) {
        RowDef *r = &g_rowDefs[i];
        int boxH;
        if (!r->multiline) {
            boxH = rowH;
        } else {
            int lines = CountLines(r->valueBuf);
            boxH = lines * lineHeightPx + multiPadding;
        }
        total += boxH + gap;
    }
    total += bottomMargin;
    return total;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
 WSADATA wsaData;
 WSAStartup(MAKEWORD(2, 2), &wsaData);

 LoadLibraryW(L"Msftedit.dll");

 CoInitializeEx(0, COINIT_MULTITHREADED);
 CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
 RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

 LoadComputerAndUser();
 LoadSystemModel();
 LoadCPUName();
 LoadWinVersion();
 LoadRAMInfo();
 LoadGPUInfo();
 LoadDiskInfo();
 LoadUptime();
 LoadSerialNumber();
 LoadIPAndMac();
 LoadMonitorInfo();

 snprintf(g_modelSN, sizeof(g_modelSN), "%s SN: %s", g_sysModel, g_serial);

 WNDCLASSEXW wc = {0};
 wc.cbSize = sizeof(WNDCLASSEXW);
 wc.lpfnWndProc = WndProc;
 wc.hInstance = hInstance;
 wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
 wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
 wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
 wc.lpszClassName = L"MachineInfoWndClass";
 RegisterClassExW(&wc);

int idealHeight = ComputeIdealContentHeight();
 g_minWindowHeight = idealHeight < 400 ? 400 : idealHeight;

 // ปรับความกว้างเริ่มต้นจาก 710 เป็น 600 เพื่อให้หน้าต่างหดพอดีกับความยาวข้อความด้านขวา
 RECT r = {0, 0, 600, idealHeight};
 AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

 HWND hwnd = CreateWindowExW(0, L"MachineInfoWndClass", U8W(APP_TITLE),
 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
 CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
 NULL, NULL, hInstance, NULL);

 ShowWindow(hwnd, nCmdShow);
 UpdateWindow(hwnd);

 MSG msg;
 while (GetMessageW(&msg, NULL, 0, 0)) {
 TranslateMessage(&msg);
 DispatchMessageW(&msg);
 }

 CoUninitialize();
 WSACleanup();
 return 0;
}