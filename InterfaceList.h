#pragma once

#include <iostream>
#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <ws2tcpip.h>


class InterfaceList
{
private:
	PIP_ADAPTER_ADDRESSES interfaceContent;
	MIB_IPFORWARDTABLE* pIpForwardTable;
	DWORD interfaceCount;

	MIB_IPFORWARDTABLE* loadRoutingTable();
	PIP_ADAPTER_ADDRESSES loadInterfaces();
	PIP_ADAPTER_ADDRESSES sortInterfaces(PIP_ADAPTER_ADDRESSES pAddresses, DWORD *count);

public:
	InterfaceList();
	~InterfaceList();
	DWORD Count();
	PIP_ADAPTER_ADDRESSES operator [](DWORD index);
	void show(PIP_ADAPTER_ADDRESSES pCurrAddresses);
	int toStaticAddr(PIP_ADAPTER_ADDRESSES adapterAddress);
	static DWORD interfaceRank(PIP_ADAPTER_ADDRESSES pAddresses);
	char** getIPv4Mask(PIP_ADAPTER_ADDRESSES iface, DWORD* count);
	char* getGateway(PIP_ADAPTER_ADDRESSES iface);
	char ** getDNS(PIP_ADAPTER_ADDRESSES iface, DWORD* count);
};
