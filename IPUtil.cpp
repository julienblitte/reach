#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <ws2tcpip.h>

// replace inet_addr deprecated:
DWORD inet_atoi(const char* aAddr)
{
    struct in_addr ipv4_addr;

    if (inet_pton(AF_INET, aAddr, &ipv4_addr) <= 0)
    {
        return NULL;
    }

    return (DWORD)ntohl(ipv4_addr.s_addr);
    //return (DWORD)ipv4_addr.s_addr;
}

char* inet_itoa(DWORD iAddr)
{
    DWORD value;
    char* result;

    value = htonl(iAddr);

    result = (char*)malloc(INET_ADDRSTRLEN);
    if (!result)
        return NULL;

    if (inet_ntop(AF_INET, &value, result, INET_ADDRSTRLEN) == NULL)
    {
        return NULL;
    }

    return result;
}

bool inet_communicate(DWORD fromIP, DWORD toIP, byte fromMask, byte toMask)
{
    byte biggestMask;

    if (fromMask > toMask)
    {
        biggestMask = fromMask;
    }
    else
    {
        biggestMask = toMask;
    }

    if (biggestMask > 32)
    {
        biggestMask = 32;
    }

    fromIP >>= (32 - biggestMask);
    toIP >>= (32 - biggestMask);

    return (fromIP == toIP);
}

byte inet_mask(DWORD IPv4)
{
    // 10.x.x.x
    if ((IPv4 >> 24) == 0x0000000A) return 8; // class A

    // 172.16.x.x
    if ((IPv4 >> 12) == 0x00000AC1) return 16; // class B

    // 192.168.x.x
    if ((IPv4 >> 16) == 0x0000C0A8) return 24; // class C

    // ZeroConfig - 169.254.x.x
    if ((IPv4 >> 16) == 0x0000A9FE) return 16; // class B

    return 31;
}

BYTE inet_mtoi(DWORD subnetMask)
{
    //const SOCKADDR_INET* subnetMaskInet;
    //subnetMask = (DWORD)(ntohl(subnetMaskInet->Ipv4.sin_addr.s_addr));

    UINT8 prefixLength = 0;
    while ((subnetMask & 0x80000000) == 0)
    {
        prefixLength++;
        subnetMask <<= 1;
    }

    return 32 - prefixLength;
}

DWORD inet_itom(UINT8 mask)
{
    DWORD subnetMask;

    if (mask >= 32)
    {
        return 0xffffffff;
    }

    subnetMask = 0;
    while (mask)
    {
        subnetMask >>= 1;
        subnetMask |= 0x80000000;
        mask--;
    }

    return subnetMask;
}
