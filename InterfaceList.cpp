#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "InterfaceList.h"
#include "IPUtil.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

MIB_IPFORWARDTABLE* InterfaceList::loadRoutingTable()
{
    MIB_IPFORWARDTABLE* result;

    // Get the IP routing table
    DWORD dwSize = 0;
    DWORD dwRetVal = GetIpForwardTable(NULL, &dwSize, FALSE);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER)
    {
        result = (MIB_IPFORWARDTABLE*)malloc(dwSize);
        dwRetVal = GetIpForwardTable(result, &dwSize, FALSE);
        if (dwRetVal != NO_ERROR)
        {
            fprintf(stderr, "Error: GetIpForwardTable failed with error %d\n", dwRetVal);
            return NULL;
        }
    }
    else
    {
        fprintf(stderr, "Error: GetIpForwardTable failed with error %d\n", dwRetVal);
        return NULL;
    }

    return result;
}

PIP_ADAPTER_ADDRESSES InterfaceList::loadInterfaces()
{
    PIP_ADAPTER_ADDRESSES result;

    // Get list of network adapters
    ULONG bufferLength = 0;
    int ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &bufferLength);
    if (ret != ERROR_BUFFER_OVERFLOW)
    {
        fprintf(stderr, "Error: GetAdaptersAddresses failed: %d\n", ret);
        return NULL;
    }

    result = (PIP_ADAPTER_ADDRESSES)malloc(bufferLength);
    if (result == NULL)
    {
        fprintf(stderr, "Error: InterfaceList::loadInterfaces: malloc failed\n");
        return NULL;
    }

    ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, result, &bufferLength);
    if (ret != NO_ERROR)
    {
        fprintf(stderr, "Error: GetAdaptersAddresses failed: %d\n", ret);
        free(result);
        return NULL;
    }

    return result;
}


// Define a comparison function for use with qsort()
int compare_addresses(const void* addr1, const void* addr2)
{
    PIP_ADAPTER_ADDRESSES a1 = (PIP_ADAPTER_ADDRESSES)addr1;
    PIP_ADAPTER_ADDRESSES a2 = (PIP_ADAPTER_ADDRESSES)addr2;
    return InterfaceList::interfaceRank(a1) - InterfaceList::interfaceRank(a2);
}

PIP_ADAPTER_ADDRESSES InterfaceList::sortInterfaces(PIP_ADAPTER_ADDRESSES pAddresses, DWORD* count)
{
    // Count the number of items in the list
    *count = 0;
    PIP_ADAPTER_ADDRESSES cur = pAddresses;
    while (cur != NULL)
    {
        (*count)++;
        cur = cur->Next;
    }

    // Allocate memory for the array of addresses
    PIP_ADAPTER_ADDRESSES sorted_addrs = (PIP_ADAPTER_ADDRESSES)malloc((*count) * sizeof(IP_ADAPTER_ADDRESSES));
    if (sorted_addrs == NULL)
    {
        fprintf(stderr, "Error: memory allocation failed\n");
        *count = 0;
        return NULL;
    }

    // Populate the array of addresses
    cur = pAddresses;
    for (DWORD i = 0; i < (*count); i++)
    {
        sorted_addrs[i] = *cur;
        cur = cur->Next;
    }

    // Sort the array by IfIndex
    qsort(sorted_addrs, (*count), sizeof(IP_ADAPTER_ADDRESSES), compare_addresses);

    // Set the count parameter and return the sorted array
    return sorted_addrs;
}

/*
PIP_ADAPTER_ADDRESSES InterfaceList::sortInterfaces(PIP_ADAPTER_ADDRESSES pAddresses, DWORD *count)
{
    PIP_ADAPTER_ADDRESSES pCurrentAddress = pAddresses;
    PIP_ADAPTER_ADDRESSES pSortedAdapters = NULL;

    *count = 0;

    while (pCurrentAddress != NULL)
    {
        PIP_ADAPTER_ADDRESSES pNewNode;

        pNewNode = (PIP_ADAPTER_ADDRESSES)malloc(sizeof(_IP_ADAPTER_ADDRESSES_LH));
        if (!pNewNode)
        {
            fprintf(stderr, "Error:  InterfaceList::sortInterfaces: malloc failed\n");
            return NULL;
        }
        (*count)++;

        memcpy(pNewNode, pCurrentAddress, sizeof(_IP_ADAPTER_ADDRESSES_LH));
        pNewNode->IfIndex = InterfaceList::interfaceRank(pCurrentAddress);

        // Insertion de l'élément dans la nouvelle liste triée
        if (pSortedAdapters == NULL || pNewNode->IfIndex < pSortedAdapters->IfIndex)
        {
            // Cas particulier : l'élément doit être placé en tête de liste
            pNewNode->Next = pSortedAdapters;
            pSortedAdapters = pNewNode;
        }
        else
        {
            // Recherche de l'élément précédent dans la nouvelle liste
            PIP_ADAPTER_ADDRESSES pNode = pSortedAdapters;
            while (pNode->Next != NULL && pNode->Next->IfIndex < pNewNode->IfIndex)
            {
                pNode = pNode->Next;
            }
            // Insertion de l'élément après l'élément précédent
            pNewNode->Next = pNode->Next;
            pNode->Next = pNewNode;
        }

        // Passage à l'élément suivant de la liste chaînée
        pCurrentAddress = pCurrentAddress->Next;
    }

    return pSortedAdapters;
}
*/

DWORD InterfaceList::interfaceRank(PIP_ADAPTER_ADDRESSES pAddresses)
{
    DWORD result;

    result = 0;

    // inteface up?
    switch (pAddresses->OperStatus)
    {
    case IfOperStatusUp:
        break;
    case IfOperStatusDormant:
        result += 1024;
        break;
    case IfOperStatusNotPresent:
        result += 2048;
        break;
    case IfOperStatusDown:
        result += 4096;
        break;
    default:
        result += 8192;
    }

    // physical link or dial-up/VPN?
    // (pAddresses->ConnectionType != NET_IF_CONNECTION_DEDICATED)

    // type ethernet?
    switch (pAddresses->IfType)
    {
        // Ethernet    
    case IF_TYPE_ETHERNET_CSMACD:
        result += 4;
        break;
        // WIFI
    case IF_TYPE_IEEE80211:
        result += 8;
        break;
        // VPN
    case IF_TYPE_PPP:
        result += 16;
        break;
        // Tunnel
    case IF_TYPE_TUNNEL:
        result += 32;
        break;
        // Loopback
    case IF_TYPE_SOFTWARE_LOOPBACK:
        result += 128;
        break;
    default:
        result += 64;
        break;
    }

    return result;
}

InterfaceList::InterfaceList()
{
    PIP_ADAPTER_ADDRESSES interfaceListRaw;

    this->pIpForwardTable = InterfaceList::loadRoutingTable();
    interfaceListRaw = InterfaceList::loadInterfaces();
    this->interfaceContent = InterfaceList::sortInterfaces(interfaceListRaw, &this->interfaceCount);

    return;
}

InterfaceList::~InterfaceList()
{
    free(this->interfaceContent);
}

PIP_ADAPTER_ADDRESSES InterfaceList::operator [](DWORD index)
{
    return interfaceContent+index;
}

DWORD InterfaceList::Count()
{
    return this->interfaceCount;
}

char**InterfaceList::getIPv4Mask(PIP_ADAPTER_ADDRESSES iface, DWORD *count)
{
    char** result;
    DWORD number;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast;

    *count = 0;

    if (iface == NULL)
    {
        return NULL;
    }

    number = 0;
    pUnicast = iface->FirstUnicastAddress;
    while (pUnicast != NULL)
    {
        void* pAddr = &pUnicast->Address.lpSockaddr->sa_data[2];
        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
        {
            number++;
        }
        pUnicast = pUnicast->Next;
    }

    // IPv4 + Mask list
    result = (char **)malloc(2*number*sizeof(char*));
    if (result == NULL)
    {
        fprintf(stderr, "Error: InterfaceList::getIPv4Mask malloc error!\n");
        return NULL;
    }

    number = 0;
    pUnicast = iface->FirstUnicastAddress;
    while (pUnicast != NULL)
    {
        void* pAddr = &pUnicast->Address.lpSockaddr->sa_data[2];
        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
        {
            // IPv4
            result[number] = (char *)malloc(INET_ADDRSTRLEN);
            if (result[number] == NULL)
            {
                fprintf(stderr, "malloc error!\n");
                return NULL;
            }
            inet_ntop(AF_INET, pAddr, result[number], INET_ADDRSTRLEN);
            number++;

            result[number] = inet_itoa(inet_itom(pUnicast->OnLinkPrefixLength));
            number++;
        }
        pUnicast = pUnicast->Next;
    }

    (*count) = number/2;
    return result;
}

char *InterfaceList::getGateway(PIP_ADAPTER_ADDRESSES iface)
{
    DWORD dwDefaultGateway = 0;
    char* result;

    for (DWORD i = 0; i < pIpForwardTable->dwNumEntries; i++)
    {
        if (pIpForwardTable->table[i].dwForwardIfIndex == iface->IfIndex && pIpForwardTable->table[i].dwForwardDest == 0)
        {
            dwDefaultGateway = pIpForwardTable->table[i].dwForwardNextHop;

            result = (char*)malloc(INET_ADDRSTRLEN);
            if (result == NULL)
            {
                fprintf(stderr, "Error: InterfaceList::getGateway: malloc error!\n");
                return NULL;
            }

            struct in_addr inAddr;
            //inAddr.s_addr = htonl(dwDefaultGateway);
            inAddr.s_addr = dwDefaultGateway;
            inet_ntop(AF_INET, &inAddr, result, INET_ADDRSTRLEN);

            return result;
        }
    }

    // no gateway found
    return NULL;
}

char** InterfaceList::getDNS(PIP_ADAPTER_ADDRESSES iface, DWORD* count)
{
    char** result;
    PIP_ADAPTER_DNS_SERVER_ADDRESS pDnsServer;
    DWORD i;

    // count
    (*count) = 0;
    pDnsServer = iface->FirstDnsServerAddress;
    while (pDnsServer != NULL)
    {
        if (pDnsServer->Address.lpSockaddr->sa_family == AF_INET)
        {
            (*count)++;
        }
        pDnsServer = pDnsServer->Next;
    }

    result = (char**)malloc((*count) * sizeof(char*));
    if (result == NULL)
    {
        fprintf(stderr, "Error: InterfaceList::getDNS: malloc error!\n");
        return NULL;
    }

    // DNS Servers
    i = 0;
    pDnsServer = iface->FirstDnsServerAddress;
    while (pDnsServer != NULL)
    {
        if (pDnsServer->Address.lpSockaddr->sa_family == AF_INET)
        {
            void* pAddr = &pDnsServer->Address.lpSockaddr->sa_data[2];
            result[i] = (char*)malloc(INET_ADDRSTRLEN);
            if (result[i] == NULL)
            {
                fprintf(stderr, "Error: InterfaceList::getDNS: malloc error!\n");
                return NULL;
            }
            inet_ntop(AF_INET, pAddr, result[i], INET_ADDRSTRLEN);
            i++;
        }
        pDnsServer = pDnsServer->Next;
    }

    return result;
}

void InterfaceList::show(PIP_ADAPTER_ADDRESSES pCurrAddresses)
{
    printf("Interface %S\n", pCurrAddresses->FriendlyName);

    if (pCurrAddresses->Dhcpv4Enabled)
    {
        printf("  IPv4 policy: dhcp\n");
    }
    else
    {
        printf("  IPv4 policy: static\n");
    }

    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
    while (pUnicast != NULL)
    {
        char ipAddr[INET6_ADDRSTRLEN];
        void* pAddr = &pUnicast->Address.lpSockaddr->sa_data[2];
        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
        {
            inet_ntop(AF_INET, pAddr, ipAddr, INET_ADDRSTRLEN);

            DWORD prefixLength = pUnicast->OnLinkPrefixLength;
            printf("  IPv4 address: %s/%u\n", ipAddr, prefixLength);
        }
        else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
        {
            inet_ntop(AF_INET6, pAddr, ipAddr, INET6_ADDRSTRLEN);
            printf("  IPv6 address: %s\n", ipAddr);
        }
        else
        {
            ipAddr[0] = '\0';
        }
        pUnicast = pUnicast->Next;
    }

    // Affichage des adresses de la passerelle
    /*
    PIP_ADAPTER_GATEWAY_ADDRESS_LH pGatewayAddress = pCurrAddresses->FirstGatewayAddress;
    while (pGatewayAddress != NULL)
    {
        char gatewayAddr[INET6_ADDRSTRLEN];
        void* pAddr = &pGatewayAddress->Address.lpSockaddr->sa_data[2];
        if (pGatewayAddress->Address.lpSockaddr->sa_family == AF_INET)
        {
            inet_ntop(AF_INET, pAddr, gatewayAddr, INET_ADDRSTRLEN);
        }
        else if (pGatewayAddress->Address.lpSockaddr->sa_family == AF_INET6)
        {
            inet_ntop(AF_INET6, pAddr, gatewayAddr, INET6_ADDRSTRLEN);
        }
        else
        {
            gatewayAddr[0] = '\0';
        }
        printf("  Gateway: %s\n", gatewayAddr);
        pGatewayAddress = pGatewayAddress->Next;
    }
    */

    // Find the default gateway IP address for the adapter
    DWORD dwAdapterIndex = pCurrAddresses->IfIndex;
    DWORD dwDefaultGateway = 0;
    for (DWORD i = 0; i < pIpForwardTable->dwNumEntries; i++)
    {
        if (pIpForwardTable->table[i].dwForwardIfIndex == dwAdapterIndex && pIpForwardTable->table[i].dwForwardDest == 0)
        {
            dwDefaultGateway = pIpForwardTable->table[i].dwForwardNextHop;

            char gateway[INET_ADDRSTRLEN];
            struct in_addr inAddr;
            //inAddr.s_addr = htonl(dwDefaultGateway);
            inAddr.s_addr = dwDefaultGateway;
            inet_ntop(AF_INET, &inAddr, gateway, INET_ADDRSTRLEN);

            // Print the default gateway IP address
            printf("  Gateway: %s\n", gateway);
            break;
        }
    }

    // DNS Servers
    PIP_ADAPTER_DNS_SERVER_ADDRESS pDnsServer = pCurrAddresses->FirstDnsServerAddress;
    while (pDnsServer != NULL)
    {
        char dnsAddr[INET6_ADDRSTRLEN];
        void* pAddr = &pDnsServer->Address.lpSockaddr->sa_data[2];
        if (pDnsServer->Address.lpSockaddr->sa_family == AF_INET)
        {
            inet_ntop(AF_INET, pAddr, dnsAddr, INET_ADDRSTRLEN);
        }
        else if (pDnsServer->Address.lpSockaddr->sa_family == AF_INET6)
        {
            inet_ntop(AF_INET6, pAddr, dnsAddr, INET6_ADDRSTRLEN);
        }
        else
        {
            dnsAddr[0] = '\0';
        }
        printf("  DNS server: %s\n", dnsAddr);
        pDnsServer = pDnsServer->Next;
    }

    printf("  MAC Address: ");
    for (ULONG i = 0; i < pCurrAddresses->PhysicalAddressLength; i++)
    {
        printf("%02x", pCurrAddresses->PhysicalAddress[i]);
        if (i != pCurrAddresses->PhysicalAddressLength - 1)
        {
            printf("-");
        }
    }
    printf("\n");

#ifdef _DEBUG
    printf("  [DEBUG] IfIndex: %u\n", pCurrAddresses->IfIndex);
    printf("  [DEBUG] Internal interface ranking: %u\n", InterfaceList::interfaceRank(pCurrAddresses));
#endif // _DEBUG
}


int InterfaceList::toStaticAddr(PIP_ADAPTER_ADDRESSES adapterAddress)
{
    // Set the new IP address and subnet mask
    const wchar_t* ipAddress = L"192.168.1.100";
    const wchar_t* subnetMask = L"255.255.255.0";


    // Delete the old IP address if it exists
    for (IP_ADAPTER_UNICAST_ADDRESS* address = adapterAddress->FirstUnicastAddress; address != NULL; address = address->Next)
    {
        if (address->Address.lpSockaddr->sa_family == AF_INET && ((sockaddr_in*)address->Address.lpSockaddr)->sin_addr.s_addr != INADDR_LOOPBACK)
        {
            MIB_UNICASTIPADDRESS_ROW addressRow = {};
            addressRow.Address.si_family = AF_INET;
            addressRow.InterfaceLuid = adapterAddress->Luid;
            addressRow.InterfaceIndex = adapterAddress->IfIndex;
            addressRow.Address.Ipv4.sin_addr.S_un.S_addr = ((sockaddr_in*)address->Address.lpSockaddr)->sin_addr.s_addr;
            if (DeleteUnicastIpAddressEntry(&addressRow) != NO_ERROR)
            {
                std::cerr << "Error: DeleteUnicastIpAddressEntry failed\n";
                return 1;
            }
        }
    }

    // Create the new IP address
    SOCKADDR_INET ipAddressInet = {};
    ipAddressInet.si_family = AF_INET;
    InetPton(AF_INET, ipAddress, &ipAddressInet.Ipv4.sin_addr);
    SOCKADDR_INET subnetMaskInet = {};
    subnetMaskInet.si_family = AF_INET;
    InetPton(AF_INET, subnetMask, &subnetMaskInet.Ipv4.sin_addr);
    MIB_UNICASTIPADDRESS_ROW newAddressRow = {};
    newAddressRow.Address = ipAddressInet;
    newAddressRow.InterfaceLuid = adapterAddress->Luid;
    newAddressRow.InterfaceIndex = adapterAddress->IfIndex;

    DWORD subnetMaskInt = (DWORD)(ntohl(subnetMaskInet.Ipv4.sin_addr.s_addr));
    newAddressRow.OnLinkPrefixLength = inet_mtoi(subnetMaskInt);
    if (CreateUnicastIpAddressEntry(&newAddressRow) != NO_ERROR)
    {
        std::cerr << "Error: CreateUnicastIpAddressEntry failed\n";
        return 1;
    }
    
    return 0;
}

