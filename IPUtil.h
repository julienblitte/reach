#pragma once
#include <winsock2.h>

/*
p: print
n: numeric
a: ascii
i: int
m: mask
*/


DWORD inet_atoi(const char* aAddr);
char* inet_itoa(DWORD iAddr);
bool inet_communicate(DWORD fromIP, DWORD toIP, byte fromMask, byte toMask);
byte inet_mask(DWORD IPv4);
BYTE inet_mtoi(DWORD subnetMask);
DWORD inet_itom(UINT8 mask);
