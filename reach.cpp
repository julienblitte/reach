#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <ws2tcpip.h>
#include <stdarg.h>

#include "InterfaceList.h"
#include "IPUtil.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

enum ParamMode { unknownMode, reachMode, dhcpMode, staticMode };

typedef struct _Parameters
{
    ParamMode mode;

    DWORD dwTargetIP, dwTargetMask, dwTargetNet;
    byte bTargetMask;

    DWORD dwSourceIP, dwSourceMask;
    DWORD dwNewIP;

    DWORD dwInterfaceCount, dwInterfaceSelected;
} Parameters;

bool ifaceIsUp(PIP_ADAPTER_ADDRESSES pCurrAddresses)
{
    return (pCurrAddresses->OperStatus == IfOperStatusUp);
}

int ifaceCanReach(PIP_ADAPTER_ADDRESSES pCurrAddresses, DWORD fromIP, byte fromMask)
{
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast;
    int i;

    i = 0;
    pUnicast = pCurrAddresses->FirstUnicastAddress;
    while (pUnicast != NULL)
    {
        void* pAddr;
        uint32_t dwIPAddr;
        BYTE bMask;

        pAddr = &pUnicast->Address.lpSockaddr->sa_data[2];
        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
        {
            dwIPAddr = ntohl(*((DWORD*)pAddr));

            bMask = pUnicast->OnLinkPrefixLength;

#ifdef  _DEBUG
            printf("[DEBUG] Can interface %S (%s/%u) reach address %s/%u => %s\n\n",
                pCurrAddresses->FriendlyName,
                inet_itoa(dwIPAddr), bMask,
                inet_itoa(fromIP), fromMask,
                (inet_communicate(dwIPAddr, fromIP, (byte)bMask, fromMask) ? "YES" : "NO"));
#endif // _DEBUG

            if (inet_communicate(dwIPAddr, fromIP, (byte)bMask, fromMask))
            {
                return i;
            }

            i++;
        }
        pUnicast = pUnicast->Next;
    }

    return -1;
}

void usage(const char *basename)
{
    fprintf(stderr, "Usage: %s <ip>[/<mask>] [<interface>] - configure adapter to reach an IPv4\n", basename);
    fprintf(stderr, "  Add a secondary ipv4 address to Interface to reach target IP.\n");
    fprintf(stderr, "  If Interface has dhcp configuration, the configuration is first switched to static IP configuration\n");
    fprintf(stderr, "  (IP address, gateway, mask and DNS servers are preserved).\n");
    fprintf(stderr, "  Note: if IP is already reachable from Interface, no change is applied.\n\n");

    fprintf(stderr, "Usage: %s dhcp [<interface>] - switch configuration to dhcp\n", basename);
    fprintf(stderr, "  The Interface is switched from static to dhcp configuration, including gateway and DNS servers.\n\n");

    fprintf(stderr, "Usage: %s static [<interface>] - switch configuration to static from dhcp information\n", basename);
    fprintf(stderr, "  The Interface is switched from dhcp to static configuration, using dhcp providing information.\n\n");
}

int systemf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    // Determine the required buffer size by calling vsnprintf with a NULL buffer
    int buf_size = vsnprintf(NULL, 0, format, args);

    // Allocate a buffer of the required size, plus 1 for the terminating null byte
    char* buf = (char*)malloc(buf_size + 1);
    if (!buf)
    {
        perror("systemf: failed to allocate buffer");
        return -1;
    }

    // Re-initialize the argument list and use vsnprintf to write the formatted string to the buffer
    va_end(args);
    va_start(args, format);
    int result = vsnprintf(buf, buf_size + 1, format, args);

    if (result < 0)
    {
        perror("systemf: vsnprintf failed");
    }
    else
    {
#if _DEBUG
        printf("[DEBUG] $ %s\n", buf);
#endif // _DEBUG
        result = system(buf);
    }

    free(buf);
    va_end(args);
    return result;
}

void toStatic(InterfaceList* list, PIP_ADAPTER_ADDRESSES iface, DWORD *dwSourceIP, DWORD* dwSourceMask)
{
    if (iface->Dhcpv4Enabled != 0)
    {
        // count the number of existing IP address
        DWORD i, IPv4Count, DNSCount;
        char** IPv4Table, ** DNSTable;
        char* gateway;

        IPv4Table = list->getIPv4Mask(iface, &IPv4Count);
        DNSTable = list->getDNS(iface, &DNSCount);
        gateway = list->getGateway(iface);

        if (IPv4Count >= 1)
        {
#ifdef _DEBUG
            printf("[DEBUG] Setting IP to static on %S\n", iface->FriendlyName);
#endif // _DEBUG

            *dwSourceIP = inet_atoi(IPv4Table[0]);
            *dwSourceMask = inet_atoi(IPv4Table[1]);

            // setup main ip, mask, gateway
            if (gateway != NULL)
            {
                systemf("netsh interface ipv4 set address \"%S\" static %s %s %s\n", iface->FriendlyName, IPv4Table[0], IPv4Table[1], gateway);
            }
            else
            {
                systemf("netsh interface ipv4 set address \"%S\" static %s %s\n", iface->FriendlyName, IPv4Table[0], IPv4Table[1]);
            }
        }
        for (i = 1; i < IPv4Count; i++)
        {
            // add secondary IP addresses
            systemf("netsh interface ipv4 add address \"%S\" %s %s\n", iface->FriendlyName, IPv4Table[2 * i], IPv4Table[2 * i + 1]);
        }

        // setup dns 1
        if (DNSCount >= 1)
        {
            systemf("netsh interface ipv4 set dns \"%S\" static %s primary\n", iface->FriendlyName, DNSTable[0]);
        }
        // setup dns 2
        if (DNSCount >= 2)
        {
            systemf("netsh interface ipv4 add dnsserver \"%S\" %s index=2\n", iface->FriendlyName, DNSTable[1]);
        }
    }
}

int loadParameters(int argc, char* argv[], Parameters* param)
{
    char* sTargetIP;
    char* sTargetMask;

    char* contextToken;

    // Check arguments
    if (argc <= 1)
    {
        return -1; // no parameter indicated
    }

    param->mode = unknownMode;
    param->dwTargetIP = 0;
    param->bTargetMask = 0;
    param->dwTargetMask = 0;

    // first argument
    if ((argv[1][0] == '/') || (argv[1][0] == '-'))
    {
        // probably "/?" or "-h" or "--help" or similar
#ifdef _DEBUG
        printf("[DEBUG] ARG: Manual requested\n");
#endif // _DEBUG
        return -1;
    }
    else if (_stricmp(argv[1], "dhcp") == 0)
    {
        param->mode = dhcpMode;
#ifdef _DEBUG
        printf("[DEBUG] ARG: Configure to dhcp\n");
#endif // _DEBUG
    }
    else if (_stricmp(argv[1], "static") == 0)
    {
        param->mode = staticMode;
#ifdef _DEBUG
        printf("[DEBUG] ARG: Configure to static\n");
#endif // _DEBUG
    }
    else
    {
        param->mode = reachMode;

        contextToken = NULL;
        sTargetIP = strtok_s(argv[1], "/", &contextToken);
        param->dwTargetIP = inet_atoi(sTargetIP);
        if (param->dwTargetIP == INADDR_NONE)
        {
            fprintf(stderr, "Invalid IPv4 address: %s\n", sTargetIP);
            return 1;
        }

        sTargetMask = strtok_s(NULL, "/", &contextToken);
        if (sTargetMask != 0)
        {
            param->bTargetMask = (byte)strtol(sTargetMask, NULL, 10);
        }
        else
        {
            param->bTargetMask = inet_mask(param->dwTargetIP);
        }

        param->dwTargetMask = inet_itom(param->bTargetMask);

#ifdef _DEBUG
        printf("[DEBUG] ARG: Target address %s/%u\n", inet_itoa(param->dwTargetIP), param->bTargetMask);
#endif // _DEBUG
    }

    // second argument
    param->dwInterfaceSelected = 1;
    if (argc >= 3)
    {
        DWORD indexIface;

        indexIface = atoi(argv[2]);
        if (indexIface >= 1 && indexIface <= param->dwInterfaceCount)
        {
            param->dwInterfaceSelected = indexIface;
        }
        else
        {
            fprintf(stderr, "Invalid interface number: %u\n", indexIface);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    InterfaceList* list;
    PIP_ADAPTER_ADDRESSES iface;

    Parameters param;

    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        printf("WSAStartup failed: %d\n", result);
        return 1;
    }

    // Retrieve network interfaces
    list = new InterfaceList();
    if (list->Count() < 1)
    {
        printf("No network found!\n");
        return 1;
    }

#ifdef _DEBUG
    printf("[DEBUG] Interface list:\n");
    for (DWORD i = 0; i < list->Count(); i++)
    {
        list->show(list->operator[](i));
        printf("\n");
    }
#endif // _DEBUG

    param.dwInterfaceCount = list->Count();
    int ret = loadParameters(argc, argv, &param);
    if (ret != 0)
    {
        usage(argv[0]);

        fprintf(stderr, "Interface list:\n");
        for (DWORD i = 0; i < list->Count(); i++)
        {
            PIP_ADAPTER_ADDRESSES currIface;

            currIface = list->operator[](i);
            if (currIface->OperStatus == IfOperStatusUp)
            {
                fprintf(stderr, "%3u. %1s %S (%u)\n", i + 1, "", currIface->FriendlyName, currIface->IfIndex);
            }
            else
            {
                fprintf(stderr, "%3u. %1s %S (%u)\n", i + 1, "*", currIface->FriendlyName, currIface->IfIndex);
            }
        }
        return EXIT_FAILURE;
    }

    iface = (*list)[param.dwInterfaceSelected - 1];
#ifdef _DEBUG
    printf("[DEBUG] ARG: Interface %S\n", iface->FriendlyName);
#endif // _DEBUG


    // dhcp mode
    if (param.mode == dhcpMode)
    {
        if (iface->Dhcpv4Enabled == 0) // dhcp inactive
        {
            systemf("netsh interface ipv4 set address \"%S\" dhcp\n", iface->FriendlyName);
            systemf("netsh interface ipv4 set dnsservers \"%S\" dhcp\n", iface->FriendlyName);
            return EXIT_SUCCESS;
        }
        else
        {
            fprintf(stderr, "Interface %S is already configured in DHCP!\n", iface->FriendlyName);
            return EXIT_FAILURE;
        }
    }

    // static mode
    if (param.mode == staticMode)
    {
        if (iface->Dhcpv4Enabled != 0) // dhcp active
        {
            toStatic(list, iface, &param.dwSourceIP, &param.dwSourceMask);
            return EXIT_SUCCESS;
        }
        else
        {
            fprintf(stderr, "Interface %S is already configured in static mode!\n", iface->FriendlyName);
            return EXIT_FAILURE;
        }
    }

    // reach mode
    if (param.mode == reachMode)
    {
        if (ifaceCanReach(iface, param.dwTargetIP, param.bTargetMask) != -1)
        {
            fprintf(stderr, "%s is already reachable on this interface!\n", inet_itoa(param.dwTargetIP));
            list->show(iface);
            return EXIT_FAILURE;
        }

        if (iface->Dhcpv4Enabled != 0)
        {
            toStatic(list, iface, &param.dwSourceIP, &param.dwSourceMask);
        }

#ifdef _DEBUG
        printf("[DEBUG] Adding extra IP on %S\n", iface->FriendlyName);
#endif // _DEBUG

        param.dwTargetNet = param.dwTargetIP & param.dwTargetMask;
        param.dwNewIP = param.dwTargetNet | (param.dwSourceIP & ~param.dwSourceMask & ~param.dwTargetMask);
        // collision, we have the new IP == target IP, let's change new IP's last bit
        if (param.dwNewIP == param.dwTargetIP)
        {
            param.dwNewIP ^= 1;
        }
#ifdef _DEBUG
        printf("[DEBUG] Source IP is %s with mask %s\n", inet_itoa(param.dwSourceIP), inet_itoa(param.dwSourceMask));
        printf("[DEBUG] Target IP is %s with mask %s\n", inet_itoa(param.dwTargetIP), inet_itoa(param.dwTargetMask));
        printf("[DEBUG] Secondary Network is %s\n", inet_itoa(param.dwTargetNet));
        printf("[DEBUG] Secondary IP is %s with mask %s\n", inet_itoa(param.dwNewIP), inet_itoa(param.dwTargetMask));
#endif // _DEBUG

        //add secondary IP
        //TODO: if no IP setup, change command
        systemf("netsh interface ipv4 add address \"%S\" %s %s\n", iface->FriendlyName, inet_itoa(param.dwNewIP), inet_itoa(param.dwTargetMask));
        return EXIT_SUCCESS;
    }
   
    // Winsock cleanup
    // NEVER CALLED...
EXIT_CLEAN:
    WSACleanup();

    return EXIT_SUCCESS;
}