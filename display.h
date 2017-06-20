#pragma once
#include "stdafx.h"
#include "global.h"

//
// ��ӡ��ǰ�Ĵ�����Ϣ
//
void displayRegisters(const CONTEXT &ct)
{
	cout << "�Ĵ�����Ϣ��" << endl;
	printf("eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n", ct.Eax, ct.Ebx, ct.Ecx, ct.Edx, ct.Esi, ct.Edi);
	printf("eip=%08x esp=%08x ebp=%08x\n", ct.Eip, ct.Esp, ct.Ebp);
	printf("cs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x efl=%08x\n", ct.SegCs, ct.SegSs, ct.SegDs, ct.SegEs, ct.SegFs, ct.SegGs, ct.EFlags);
}

//
// ��ӡջ��Ϣ
//
void displayStack(DEBUG_EVENT de, const CONTEXT &ct, DWORD num=10)
{
	DWORD size = num * 8;
	char* buff = new char[size];

	// ��ȡջ��Ϣ
	DWORD dwRead = 0;
	ReadProcessMemory(OpenProcess(PROCESS_ALL_ACCESS, FALSE, de.dwProcessId),
		(LPVOID)ct.Esp,
		buff,
		size,
		&dwRead
		);

	cout << "ջ��Ϣ" << endl;
	DWORD esp = ct.Esp;
	for (int i = 0; i < num; ++i)
	{
		printf("%08x  |  %08x \n", esp + 8*i, ((DWORD*)buff)[i]);
	}

	delete[] buff;
}