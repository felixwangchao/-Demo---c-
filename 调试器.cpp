#include "stdafx.h"
#include "global.h"
#include "display.h"
#include "breakpoint.h"
#include <algorithm>

vector <breakpoint*> bpList;

int HrdBpCount();
void Assembler(string,LPVOID);
void RepairMemory();
void ReinstallBp();
breakpoint* findHdrBp(LPVOID addr);
bp_mm* findMMBp(LPVOID addr);
bp_int3* getCurrentStepBp(LPVOID addr, DWORD& dwNum);
bool isBreakpoint(LPVOID addr);
void editMemoryByte(LPVOID addr, byte b);
void editMemoryDword(LPVOID, DWORD);
void dump();
void displayImport(DWORD);
void displayExport(DWORD);
void displayModule();
void displayMemory(LPVOID addr, int mode);
void disassembly(LPVOID addr, int nNum = 7);
bool delBp(DWORD size);
void displayExchain();
void getAddressInfo(DWORD addr);
void stackBacktracking();


// ��������
void IsCall(DWORD & nextCall);

// ���Ų���
SIZE_T GetSymAddress(const char* pszName);
BOOL GetSymName(SIZE_T nAddress, string& strName);

bp_int3* getCurrentConditionBp(LPVOID addr);
unsigned int CALLBACK threadProc(void *pArg);

bool cmp(const breakpoint* x, const breakpoint* y)
{
	return x->type < y->type;
}


// ��¼��ǰ�ϵ�

typedef struct _CURRENT_BP
{
	breakpoint* currentbp;
	bool flag;
	int time;
}CURRENT_BP;

CURRENT_BP cur = CURRENT_BP{ nullptr, false };

// ��¼��ǰָ��
string cmd="";

// ������ڵ�Ķϵ�
DWORD dwOEp;
bp_int3 bp;

// ����ǰ�ڴ�ϵ�Ľṹ��
bp_mm * current_mm;
bp_mm * current_mm_copy;

int _tmain(int argc, _TCHAR* argv[])
{

	// 0. ������
	if (IsDebuggerPresent())
	{
		printf("���ڵ�����!\n");
		//return 0;
	}

	// 1. �������ԻỰ
	//	1.1 ����һ����δ���еĽ���.�Խ��е���
	STARTUPINFO si = { sizeof( STARTUPINFO ) };
	PROCESS_INFORMATION pi = { 0 };
	BOOL bRet = FALSE;
	bRet = CreateProcess( PATH ,
						  NULL ,
						  NULL ,
						  NULL ,
						  FALSE ,
						  DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE ,
						  NULL ,
						  NULL ,
						  &si ,
						  &pi
						  );

	if( bRet == FALSE ) {
		//DEBUG("��������ʧ��");
		return 0;
	}

	// �����ȴ��¼�
	g_WaitStop = CreateEvent(NULL,  /*��ȫ������*/
		FALSE, /*�Ƿ������ֶ�״̬*/
		FALSE, /*�Ƿ����ź�*/
		NULL); /*�¼����������*/

	g_WaitRun = CreateEvent(NULL,  /*��ȫ������*/
		FALSE, /*�Ƿ������ֶ�״̬*/
		FALSE, /*�Ƿ����ź�*/
		NULL); /*�¼����������*/

	// �����û������߳�
	uintptr_t uThread = _beginthreadex(0, 0, threadProc, 0, 0, 0);

	// ����ṹ��, ���öѿռ�Ļص�����
	//cs_opt_mem memop;
	memop.calloc = calloc;
	memop.free = free;
	memop.malloc = malloc;
	memop.realloc = realloc;
	memop.vsnprintf = (cs_vsnprintf_t)vsprintf_s;
	// ע��ѿռ�����麯��
	cs_option( 0 , CS_OPT_MEM , (size_t)&memop );

	//csh handle;
	cs_open( CS_ARCH_X86 ,
			 CS_MODE_32 ,
			 &handle
			 );

	// ��ʼ�������


	while( true ) {

		dwReturnCode = DBG_EXCEPTION_NOT_HANDLED;// DBG_CONTINUE;

		WaitForDebugEvent( &de , -1 );

		switch( de.dwDebugEventCode ) {

			case EXCEPTION_DEBUG_EVENT:
			{

				// 1. ���������Ϣ���, ׼�����û����н���
				// 2. ��ȡOPCODE
				ct = { CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS | CONTEXT_SEGMENTS | CONTEXT_INTEGER };

				GetThreadContext( OpenThread( THREAD_ALL_ACCESS , FALSE , de.dwThreadId ) , &ct );

				if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
				{
					static bool bHdrBp = false;
					static breakpoint* pHdrBp;
					breakpoint* tmp;

					if (bHdrBp == true)
					{
						bHdrBp = false;
						ReinstallBp();
						pHdrBp = nullptr;
						dwReturnCode = DBG_EXCEPTION_HANDLED;

						break;
					}

					tmp = findHdrBp((LPVOID)(ct.Eip));
					if (tmp != nullptr)
					{
						// ˵����ǰ�����ǵ���������һ��Ӳ���ϵ�
						RepairMemory();
						bIsMM = false;
					}

					if (bIsMM)
					{
						// �����ǰ�ڴ�ϵ���Ϣ���ⱻ�������ͨ�����ݻָ�
						if (current_mm == nullptr)
						{
							current_mm = current_mm_copy;
						}

						// �жϵ�ǰEip�Ƿ������ڴ�ϵ㣬���û�����ڴ�ϵ㣬�Ҳ���һ���û�TF���ʱ���Ҳ���һ�������ϵ�ʱ�����°�װ���жϵ�
						bool bIsCurrentMM = (current_mm->address == (LPVOID)ct.Eip);
						
						if (bIsTF == false && !bIsCurrentMM)
						{
							current_mm->install();
							current_mm = nullptr;
							dwReturnCode = DBG_EXCEPTION_HANDLED;
							break;
						}
					}

					if (bCdBrNotTrigged)
					{
						bCdBrNotTrigged = false;
						ReinstallBp();
						dwReturnCode = DBG_EXCEPTION_HANDLED;
						break;
					}
					
					bIsTF = false;

					disassembly((LPVOID)ct.Eip);
					SetEvent(g_WaitStop);
					WaitForSingleObject(g_WaitRun, -1);
					ReinstallBp();


					if (tmp != nullptr)
					{
						bHdrBp = true;
						pHdrBp = tmp;
						tmp->repair();
						setBreakpoint_tf();
					}
				}
				
				else if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
				{

					// ��ڵ�ϵ�
					static bool bFirst = true;
					if (bFirst)
					{
						bFirst = false;
						bp = bp_int3((LPVOID)dwOEp);
						bp.install();
						break;
					}
					bp.repair();

					// ��������ϵ�
					ct.Eip = ct.Eip - 1;
					SetThreadContext(OpenThread(THREAD_ALL_ACCESS, FALSE, de.dwThreadId), &ct);

					RepairMemory();
					bIsMM = false;

					bp_int3* bp = getCurrentConditionBp((LPVOID)ct.Eip);

					if (bp != nullptr)
					{
						bool bResult;
						string cmd;
						string op1, op2, operation;
						cmd = bp->condition;

						istringstream is(cmd);
						is >> op1 >> operation >> op2;

						DWORD opcode1, opcode2;

						if (op1.compare("eax") == 0)
						{
							opcode1 = ct.Eax;
						}
						else if (op1.compare("ecx") == 0)
						{
							opcode1 = ct.Ecx;
						}
						else if (op1.compare("edx") == 0)
						{
							opcode1 = ct.Edx;
						}

						if (op2.compare("eax") == 0)
						{
							opcode2 = ct.Eax;
						}
						else if (op2.compare("ecx") == 0)
						{
							opcode2 = ct.Ecx;
						}
						else if (op2.compare("edx") == 0)
						{
							opcode2 = ct.Edx;
						}
						else
						{
							opcode2 = std::stoi(op2,nullptr,16);
						}

						if (operation.compare(">") == 0)
						{
							bResult = opcode1 > opcode2;
						}
						else if (operation.compare("==") == 0)
						{
							bResult = opcode1 == opcode2;
						}
						else if (operation.compare("<") == 0)
						{
							bResult = opcode1 < opcode2;
						}
						printf("Result : %d\n",bResult);
						bCdBrNotTrigged = !bResult;
					}
					else
					{
						DWORD dwNum;
						bp = getCurrentStepBp((LPVOID)ct.Eip, dwNum);
						if (bp != nullptr)
						{
							bp->repair();
							delBp(dwNum);
						}
					}

					setBreakpoint_tf();

				}
				else if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
				{
					bp_mm* bm;

					bm = findMMBp((LPVOID)ct.Eip);
					if (bm == NULL)
					{
						dwReturnCode = DBG_EXCEPTION_NOT_HANDLED;
						break;
					}

					// ������Ҫ�жϵ�ǰ�ص��Ƿ���һ�������ϵ㣬����ǣ�������Ҫ��bNeedStop��λ
					// Ȼ�������һ��

					bm->repair();
					current_mm = bm;
					current_mm_copy = bm;


					// �����ǰ����һ�������ϵ�
					if (isBreakpoint((LPVOID)ct.Eip) != true)
					{
						bIsMM = true;
						setBreakpoint_tf();	
					}
				}
			}
			dwReturnCode = DBG_EXCEPTION_HANDLED;
			break;

			case CREATE_PROCESS_DEBUG_EVENT:
				dwOEp = (DWORD)de.u.CreateProcessInfo.lpStartAddress;
				hProcess = (HANDLE)de.u.CreateProcessInfo.hProcess;
				dwPid = GetProcessId(hProcess);
				dwBaseOfImage = (DWORD)de.u.CreateProcessInfo.lpBaseOfImage;

				if (SymInitialize(hProcess, PDB_PATH, FALSE) == TRUE)
				{
					DWORD64 mouduleAddress = SymLoadModule64(hProcess, 
															de.u.CreateProcessInfo.hFile,
															NULL, 
															NULL,
															(DWORD64)de.u.CreateProcessInfo.lpBaseOfImage, 
															0);

					if (mouduleAddress == 0)
					{
						printf("symmoudule64 load failed\n");
					}
				}
				else
				{
					printf("symmoudule64 init failed\n");
				}
				
				break;
			case CREATE_THREAD_DEBUG_EVENT:

				break;
			case EXIT_PROCESS_DEBUG_EVENT:

				break;
			case EXIT_THREAD_DEBUG_EVENT:

				break;
			case LOAD_DLL_DEBUG_EVENT:
			{
				DWORD64 moduleAddress = SymLoadModule64(hProcess, 
														de.u.LoadDll.hFile, 
														NULL, 
														NULL, 
														(DWORD64)de.u.LoadDll.lpBaseOfDll, 
														 0);

				if (moduleAddress == 0)
				{
					
					printf("SymLoadModule64 LoadDll failed -- %d\n",GetLastError());
				}
				
				CloseHandle(de.u.LoadDll.hFile);
			}

				break;
			case OUTPUT_DEBUG_STRING_EVENT:

				break;
			case RIP_EVENT:
				break;
			case UNLOAD_DLL_DEBUG_EVENT:

				break;
		}

		// 4. �ظ�������ϵͳ
		ContinueDebugEvent( de.dwProcessId ,
							de.dwThreadId ,
							dwReturnCode );

		// 5. �ظ�ִ�е�2��.
	}
	
	return 0;
}

bool setCurrentBp()
{
	cur.currentbp = nullptr;
	cur.flag = false;
	cur.time = 0;

	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		if ((*iter)->isCurrent((LPVOID)(ct.Eip - 1)))
		{
			cur.currentbp = *iter;
			cur.flag = true;
			cur.time = 0;
			return true;
		}
	}
	return false;
}

bool delBp(DWORD size)
{
	breakpoint* current = bpList[size];
	current->repair();
	bpList.erase(bpList.begin() + size);
	delete current;
	return true;
}

void displayExport(DWORD dwBaseOfImage)
{
	// ��ȡDOSͷ
	DWORD addr = 0;

	char* dosHeader = new char[sizeof(IMAGE_DOS_HEADER)];
	DWORD dwRead = 0;
	ReadProcessMemory(hProcess,
		(LPVOID)dwBaseOfImage,
		dosHeader,
		sizeof(IMAGE_DOS_HEADER),
		&dwRead);

	WORD magic = 0x5a4d;
	if (((IMAGE_DOS_HEADER*)dosHeader)->e_magic != magic)
	{
		printf("[!]������Ч��PE�ļ���\n");
		return;
	}

	// ��ȡNTͷ��ַ
	addr = ((IMAGE_DOS_HEADER*)dosHeader)->e_lfanew + dwBaseOfImage;
	delete[] dosHeader;
	char* ntHeader = new char[sizeof(IMAGE_NT_HEADERS) + sizeof(IMAGE_OPTIONAL_HEADER32) + sizeof(IMAGE_DATA_DIRECTORY)*16 + 100];
	ReadProcessMemory(hProcess,
		(LPVOID)addr,
		ntHeader,
		sizeof(IMAGE_NT_HEADERS) + sizeof(IMAGE_OPTIONAL_HEADER32) + sizeof(IMAGE_DATA_DIRECTORY) * 16 + 100,
		&dwRead);

	magic = 0x4550;
	if (((IMAGE_NT_HEADERS*)ntHeader)->Signature != magic)
	{
		printf("[!]������Ч��PE�ļ���\n");
		return;
	}
	
	// ��ȡ��ѡͷ
	IMAGE_OPTIONAL_HEADER32 pOptHdr = ((IMAGE_NT_HEADERS*)ntHeader)->OptionalHeader;
	IMAGE_DATA_DIRECTORY* pDataDir = pOptHdr.DataDirectory;
	DWORD dwExpTabRva = pDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	delete[] ntHeader;

	if (dwExpTabRva == 0)
	{
		printf("[*]������Ϊ��\n");
		return;
	}

	addr = dwExpTabRva + dwBaseOfImage;
	IMAGE_EXPORT_DIRECTORY *pExp = new IMAGE_EXPORT_DIRECTORY;

	ReadProcessMemory(hProcess,
		(LPVOID)addr,
		pExp,
		sizeof(IMAGE_EXPORT_DIRECTORY),
		&dwRead);

	// ��ӡDLL��
	char* pDllName = new char[50];
	DWORD dwDllName = pExp->Name + dwBaseOfImage;
	ReadProcessMemory(hProcess,
		(LPVOID)dwDllName,
		pDllName,
		50,
		&dwRead);

	printf("DLL : %s\n", pDllName);
	delete[] pDllName;

	// ��ȡ������ַ���������Ʊ�������ű�
	DWORD dwAddrTabRva = pExp->AddressOfFunctions + dwBaseOfImage;
	DWORD dwNameTabRva = pExp->AddressOfNames + dwBaseOfImage;
	DWORD dwOrdinalRva = pExp->AddressOfNameOrdinals + dwBaseOfImage;

	DWORD dwCount = pExp->NumberOfFunctions;
	DWORD dwNameCount = pExp->NumberOfNames;

	DWORD *pAddrTab = new DWORD[dwCount];
	DWORD *pNameTab = new DWORD[dwNameCount];
	WORD *pOrdinalTab = new WORD[dwNameCount];

	ReadProcessMemory(hProcess,
		(LPVOID)dwAddrTabRva,
		pAddrTab,
		sizeof(DWORD)*dwCount,
		&dwRead);
	ReadProcessMemory(hProcess,
		(LPVOID)dwNameTabRva,
		pNameTab,
		sizeof(DWORD)*dwNameCount,
		&dwRead);
	ReadProcessMemory(hProcess,
		(LPVOID)dwOrdinalRva,
		pOrdinalTab,
		sizeof(WORD)*dwNameCount,
		&dwRead);

	for (DWORD i = 0; i < dwCount; i++)
	{
		printf("\tRVA : %08x\t", pAddrTab[i]);

		DWORD j = 0;
		
		for (; j < dwNameCount; j++) {
			if (i == pOrdinalTab[j]) {

				DWORD dwFunName = pNameTab[j] + dwBaseOfImage;

				char *pFunName = new char[100];
				ZeroMemory(pFunName, 100);

				ReadProcessMemory(hProcess,
					(LPVOID)dwFunName,
					pFunName,
					100,
					&dwRead);

				printf("%s\n", pFunName);
				delete[] pFunName;
				break;
			}
		}
		if (j >= dwNameCount) {

			if (pAddrTab[i] != 0) {
				printf("ordinals:[%d]\n", pExp->Base + i);
			}
		}
	}

	delete[] pAddrTab;
	delete[] pNameTab;
	delete[] pOrdinalTab;
	delete[] pExp;
}

void displayImport(DWORD dwBaseOfImage)
{
	// ��ȡDOSͷ
	DWORD addr = 0;

	char* dosHeader = new char[sizeof(IMAGE_DOS_HEADER)];
	DWORD dwRead = 0;
	ReadProcessMemory(hProcess,
		(LPVOID)dwBaseOfImage,
		dosHeader,
		sizeof(IMAGE_DOS_HEADER),
		&dwRead);
	WORD magic = 0x5a4d;
	if (((IMAGE_DOS_HEADER*)dosHeader)->e_magic != magic)
	{
		printf("[!]������Ч��PE�ļ���\n");
		return;
	}

	// ��ȡNTͷ��ַ
	addr = ((IMAGE_DOS_HEADER*)dosHeader)->e_lfanew + dwBaseOfImage;
	delete[] dosHeader;
	char* ntHeader = new char[sizeof(IMAGE_NT_HEADERS) + sizeof(IMAGE_OPTIONAL_HEADER32) + sizeof(IMAGE_DATA_DIRECTORY) * 16 + 100];
	ReadProcessMemory(hProcess,
		(LPVOID)addr,
		ntHeader,
		sizeof(IMAGE_NT_HEADERS) + sizeof(IMAGE_OPTIONAL_HEADER32) + sizeof(IMAGE_DATA_DIRECTORY) * 16 + 100,
		&dwRead);

	magic = 0x4550;
	if (((IMAGE_NT_HEADERS*)ntHeader)->Signature != magic)
	{
		printf("[!]������Ч��PE�ļ���\n");
		return;
	}

	// ��ȡ��ѡͷ
	IMAGE_OPTIONAL_HEADER32 pOptHdr = ((IMAGE_NT_HEADERS*)ntHeader)->OptionalHeader;
	IMAGE_DATA_DIRECTORY* pDataDir = pOptHdr.DataDirectory;
	DWORD dwImpTabRva = pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

	if (dwImpTabRva == 0)
	{
		printf("[*]�����Ϊ��\n");
		return;
	}

	addr = dwImpTabRva + dwBaseOfImage;
	IMAGE_IMPORT_DESCRIPTOR *pImp = new IMAGE_IMPORT_DESCRIPTOR;
	delete[] ntHeader;

	ReadProcessMemory(hProcess,
		(LPVOID)addr,
		pImp,
		sizeof(IMAGE_IMPORT_DESCRIPTOR),
		&dwRead);

	char* pDllName;

	while (pImp->Characteristics != 0)
	{
		DWORD dwDllName = dwBaseOfImage + pImp->Name;
		pDllName = new char[50];
		ReadProcessMemory(hProcess,
			(LPVOID)dwDllName,
			pDllName,
			50,
			&dwRead);
		printf("DLL %s\n", pDllName);
		delete[] pDllName;
		pDllName = nullptr;

		// ��ȡINT �� IAT
		IMAGE_THUNK_DATA32 *pInt = new IMAGE_THUNK_DATA32;
		IMAGE_THUNK_DATA32 *pIat = new IMAGE_THUNK_DATA32;
		DWORD dwINT = dwBaseOfImage + ((IMAGE_IMPORT_DESCRIPTOR*)pImp)->OriginalFirstThunk;
		DWORD dwIAT = dwBaseOfImage + ((IMAGE_IMPORT_DESCRIPTOR*)pImp)->FirstThunk;
		ReadProcessMemory(hProcess,
			(LPVOID)dwINT,
			pInt,
			sizeof(IMAGE_THUNK_DATA32),
			&dwRead);
		ReadProcessMemory(hProcess,
			(LPVOID)dwIAT,
			pIat,
			sizeof(IMAGE_THUNK_DATA32),
			&dwRead);
		
		while (((IMAGE_THUNK_DATA32*)pInt)->u1.AddressOfData)
		{
			IMAGE_THUNK_DATA32* pINT = (IMAGE_THUNK_DATA32*)pInt;
			IMAGE_THUNK_DATA32* pIAT = (IMAGE_THUNK_DATA32*)pIat;
			if (IMAGE_SNAP_BY_ORDINAL(pINT->u1.Ordinal) == TRUE)
			{
				printf("\t���: %08x\t%d\n", *(DWORD*)pIAT, LOWORD(pINT->u1.Ordinal));
			}
			else
			{
				DWORD dwImpName = dwBaseOfImage + pINT->u1.Function;
				char* pImpName = new char[sizeof(IMAGE_IMPORT_BY_NAME)+100];
				ReadProcessMemory(hProcess,
					(LPVOID)dwImpName,
					pImpName,
					sizeof(IMAGE_IMPORT_BY_NAME)+100,
					&dwRead);

				IMAGE_IMPORT_BY_NAME* pIMPname = ((IMAGE_IMPORT_BY_NAME*)pImpName);


				printf("\t[%08d]%08x\t%s\n", pIMPname->Hint, *(DWORD*)pIAT, ((IMAGE_IMPORT_BY_NAME*)pImpName)->Name);
				delete [] pImpName;
			}
			dwINT = dwINT + sizeof(IMAGE_THUNK_DATA32);
			dwIAT = dwIAT + sizeof(IMAGE_THUNK_DATA32);
			ReadProcessMemory(hProcess,
				(LPVOID)dwINT,
				pInt,
				sizeof(IMAGE_THUNK_DATA32),
				&dwRead);
			ReadProcessMemory(hProcess,
				(LPVOID)dwIAT,
				pIat,
				sizeof(IMAGE_THUNK_DATA32),
				&dwRead);
		}

		delete pIat;
		delete pInt;

		addr = addr + sizeof(IMAGE_IMPORT_DESCRIPTOR);
		ReadProcessMemory(hProcess,
			(LPVOID)addr,
			pImp,
			sizeof(IMAGE_IMPORT_DESCRIPTOR),
			&dwRead);
	}
	delete pImp;
}

void displayBPList()
{
	int i = 0;
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		printf(" %d ", i++);
		(*iter)->show();
	}
}

void displayHelp()
{
	cout << "������������Ϣ\n"
		<< "1.ִ�������\n"
		<< "\tt\t\t\t��������\n"
		<< "\tp\t\t\t��������\n"
		<< "\tg\t\t\t��ʼִ��\n"
		<< "2.�ϵ������\n"
		<< "\tbp address\t\t����ϵ�\n"
		<< "\tba address\t\tӲ���ϵ�\n"
		<< "\tbm address\t\t�ڴ�ϵ�\n"
		<< "\tbt address\t\t�����ϵ�\n"
		<< "\tbapi name\t\tAPI�ϵ�\n"
		<< "\tbc num\t\t\t�����num�Ŷϵ�\n"
		<< "3.�ڴ�༭�����\n"
		<< "\teb address byte\t\t�ڸõ�ַд���ֽ���Ϣ\n"
		<< "\ted address dw\t\t�ڸõ�ַд��4�ֽ���Ϣ\n"
		<< "\ta address\t\t�ڸõ�ַд�������\n"
		<< "4.��ʾ�����\n"
		<< "\th\t\t\t��ʾ������Ϣ\n"
		<< "\tk\t\t\t��ʾ���ö�ջ\n"
		<< "\tr\t\t\t��ʾ�Ĵ�����Ϣ\n"
		<< "\tbl\t\t\t��ʾ�ϵ��б�\n"
		<< "\tlm\t\t\t��ʾģ����Ϣ\n"
		<< "\tcls\t\t\t�����Ļ\n"
		<< "\texchain\t\t\t��ʾ�쳣������\n"
		<< "\ts (num)\t\t\t��ʾջ��Ϣ,num��������\n"
		<< "\tx address\t\t��ʾһ����ַ���ĸ������\n"
		<< "\tu (addr) (num)\t\t�����addrָ���ĵ�ַnum��\n"
		<< "\tdd address\t\t��ʾ�õ�ַ���ڴ棬�����ֽ�Ϊ��λ\n"
		<< "\tdb address\t\t��ʾ�õ�ַ���ڴ棬���ֽ�Ϊ��λ\n"
		<< "\tda address\t\t��ʾ�õ�ַ����ascii�ַ���\n"
		<< "\texport address\t\t��ʾ�Ըõ�ַΪ��ַ�ĵ�����\n"
		<< "\timport address\t\t��ʾ�Ըõ�ַΪ��ַ�ĵ����\n"
		<< "5. DUMP����\n"
		<< "\tdump\t\t\t����DMP�ļ�������ΪMydump.dmp\n"
		<< endl;
}


unsigned int CALLBACK threadProc(void *pArg)
{
	string operation, address, size;
	string buff;
	bool currentbp_flag;
	while (true)
	{
		WaitForSingleObject(g_WaitStop,-1);

		currentbp_flag = setCurrentBp();

		while (true) {
			operation.clear();
			address.clear();
			size.clear();

			cout << "debugger> ";
			getline(cin, buff);

			if (buff.compare("") == 0)
			{
				buff = cmd;
			}
			else
			{
				cmd = buff;
			}

			istringstream is(buff);
			is >> operation>> address >> size;

			if (operation.compare("t") == 0)
			{
				bIsTF = true;
				setBreakpoint_tf();
				SetEvent(g_WaitRun);
				break;
			}
			else if (operation.compare("p") == 0)
			{
				DWORD nextCall = 0;
				IsCall(nextCall);
				if (nextCall == 0)
				{
					bIsTF = true;
					setBreakpoint_tf();
					SetEvent(g_WaitRun);
					break;
				}
				else
				{

					if (isBreakpoint((LPVOID)nextCall))
					{
						printf("[!]�õ�ַ�Ѿ����ڶϵ㣡\n");
						break;
					}
					string msg = "�����ϵ�";
					breakpoint *bp = new bp_int3((LPVOID)nextCall,msg);
					bp->type = STEP_OUT_BREAKPOINT;

					if (bp->install())
					{
						bpList.push_back(bp);
						sort(bpList.begin(), bpList.end(),cmp);
						SetEvent(g_WaitRun);
						break;
					}
					else
					{
						printf("[!]�ϵ㰲װʧ�ܣ�\n");
						delete bp;
						break;
					}
				}
			}
			else if (operation.compare("g") == 0)
			{
				SetEvent(g_WaitRun);
				break;
			}

			//
			//	����ϵ�
			//
			else if (operation.compare("bp") == 0)
			{
				LPVOID addr;
				try
				{
					addr = (LPVOID)(std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч�ĵ�ַ��\n");
					continue;
				}
				
				if (isBreakpoint(addr))
				{
					printf("[!]�õ�ַ�Ѿ����ڶϵ㣡\n");
					break;
				}

				breakpoint *bp = new bp_int3(addr);

				if (bp->install())
				{
					bpList.push_back(bp);
					sort(bpList.begin(), bpList.end(),cmp);
					bp->repair();
				}
				else
				{
					delete bp;
				}
			}

			else if (operation.compare("bt") == 0)
			{
				LPVOID addr;
				try
				{
					addr = (LPVOID)(std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч�ĵ�ַ��\n");
					continue;
				}
				
				if (isBreakpoint(addr))
				{
					printf("[!]�õ�ַ�Ѿ����ڶϵ�!\n");
					break;
				}

				cout << "������������\n"
					<< "������ʽ �Ĵ��� ������ �Ĵ���/������\n"
					<< "Ŀǰ�Ĵ���ֻ֧��eax��ecx��edx\n"
					<< "������ֻ֧�� > < == \n"
					<< "����������������" 
					<< endl;

				string cmd;
				getline(cin, cmd);

				string op1, op2, operation;

				istringstream is(cmd);
				is >> op1>> operation >> op2;

				if (op1.size() == 0 || op2.size() == 0 || operation.size() == 0)
				{
					printf("[!]������ʽ����!\n");
					continue;
				}

				if (op1.compare("eax") != 0 && op1.compare("ecx") != 0 && op1.compare("edx") != 0)
				{
					printf("[!]������ʽ����!\n");
					continue;
				}

				if (operation.compare("<") != 0 && operation.compare(">") != 0 && operation.compare("==") != 0)
				{
					printf("[!]������ʽ����!\n");
					continue;
				}

				if (op2.compare("eax") != 0 && op2.compare("ecx") != 0 && op2.compare("edx") != 0)
				{
					try
					{
						LPVOID addr = (LPVOID)std::stoi(op2, nullptr, 16);
					}
					catch (exception& e)
					{
						printf("[!]������ʽ����!\n");
						continue;
					}
				}

				string msg1 = "�����ϵ�";
				string msg2 = cmd;
				breakpoint *bp = new bp_int3(addr, msg1, msg2);
				
				if (bp->install())
				{
					bpList.push_back(bp);
					sort(bpList.begin(), bpList.end(),cmp);
					bp->repair();
				}
				else
				{
					delete bp;
				}
			}

			else if (operation.compare("bapi") == 0)
			{
				const char* pszName = address.c_str();
				DWORD dwApi = 0;
				dwApi = GetSymAddress(pszName);
				if (dwApi != 0)
				{
					LPVOID addr = (LPVOID)dwApi;
					printf("[*]API:%s --- 0x%08x\n", pszName, dwApi);

					if (isBreakpoint(addr))
					{
						printf("[!]�õ�ַ�Ѿ����ڶϵ㣡\n");
						break;
					}
					string msg = "API�ϵ�";
					breakpoint *bp = new bp_int3(addr,msg);

					if (bp->install())
					{
						bpList.push_back(bp);
						sort(bpList.begin(), bpList.end(),cmp);
						bp->repair();
					}
					else
					{
						delete bp;
					}
				}
				else
				{
					printf("[!]�Ҳ���API:%s�ĵ�ַ\n",pszName);
				}
			}
			else if (operation.compare("bc") == 0)
			{

				DWORD size;
				try
				{
					size = (std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]��Ŵ���\n");
					continue;
				}

				if (size >= bpList.size() || size < 0)
				{
					printf("[!]��Ŵ���\n");
				}
				else
				{
					delBp(size);
				}
			}
			else if (operation.compare("x") == 0)
			{
				if (address.size() > 0)
				{
					DWORD addr;
					try
					{
						addr = stoi(address,0,16);
					}
					catch (exception& e)
					{
						printf("[!]������һ����Ч�ĵ�ַ");
						continue;
					}
					
					getAddressInfo(addr);
				}
			}
			else if (operation.compare("k") == 0)
			{
				stackBacktracking();
			}
			else if (operation.compare("r") == 0)
			{
				displayRegisters(ct);
			}
			else if (operation.compare("h") == 0)
			{
				displayHelp();
			}
			else if (operation.compare("exchain") == 0)
			{
				displayExchain();
			}
			else if (operation.compare("import") == 0)
			{
				DWORD addr;
				try
				{
					addr = std::stoi(address,nullptr,16);
				}
				catch (exception& e)
				{
					cout << "[!]������һ����Ч�ĵ�ַ��" << endl;
					continue;
				}

				displayImport(addr);
			}
			else if (operation.compare("export") == 0)
			{
				DWORD addr;
				try
				{
					addr = std::stoi(address, nullptr, 16);
				}
				catch (exception& e)
				{
					cout << "[!]������һ����Ч�ĵ�ַ��" << endl;
					continue;
				}

				displayExport(addr);
			}
			else if (operation.compare("s") == 0)
			{

				DWORD size;
				try
				{
					size = (std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч����ֵ!\n");
					continue;
				}

				if (address.size() > 0)
					displayStack(de, ct, size);
				else
					displayStack(de, ct);
			}
			else if (operation.compare("bl") == 0)
			{
				displayBPList();
			}
			else if (operation.compare("lm") == 0)
			{
				displayModule();
			}
			else if (operation.compare("u") == 0)
			{
				if (size.length() > 0)
				{

					LPVOID addr;
					DWORD s;
					try
					{
						addr = (LPVOID)(std::stoi(address, nullptr, 16));
						s = std::stoi(size, nullptr, 16);
					}
					catch (exception& e)
					{
						printf("[!]������һ����Ч����ֵ!\n");
						continue;
					}
					disassembly(addr, s);

				}

				else
				{
					if (address.size() == 0)
					{
						disassembly((LPVOID)ct.Eip);
					}
					else
					{

						LPVOID addr;
						try
						{
							addr = (LPVOID)(std::stoi(address, nullptr, 16));
						}
						catch (exception& e)
						{
							printf("[!]������һ����Ч����ֵ!\n");
							continue;
						}
						disassembly(addr);
					}
				}
			}

			else if (operation.compare("eb") == 0)
			{

				byte b;
				try
				{
					b = (byte)stoi(size, nullptr, 16);
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч����ֵ!\n");
					continue;
				}

				if (size.length() == 1 || size.length() == 2)
				{
					editMemoryByte((LPVOID)stoi(address, nullptr, 16), b);
				}
				else
				{
					cout << "[!]������һ����Ч��ֵ��";
				}
			}

			else if (operation.compare("ed") == 0)
			{
				if (size.length() > 0 && size.length() <= 8)
				{
					LPVOID addr;
					DWORD s;
					try
					{
						addr = (LPVOID)(std::stoi(address, nullptr, 16));
						s = std::stoi(size, nullptr, 16);
					}
					catch (exception& e)
					{
						printf("[!]������һ����Ч����ֵ!\n");
						continue;
					}

					editMemoryDword(addr, s);
				}
				else
				{
					cout << "[!]������һ����Ч��ֵ��";
				}
			}

			else if (operation.compare("a") == 0)
			{
				if (address.size() > 0 && address.size() <= 8)
				{

					LPVOID addr;
					try
					{
						addr = (LPVOID)(std::stoi(address, nullptr, 16));
					}
					catch (exception& e)
					{
						printf("[!]������һ����Ч����ֵ!\n");
						continue;
					}

					printf("[*]��������ָ�� > ");
					string cmd;
					getline(cin, cmd);
					Assembler(cmd,addr);
				}
				else
				{
					printf("[!]������һ����Ч�ĵ�ַ\n");
				}
			}
			else if (operation.compare("dump") == 0)
			{
				dump();
			}
			else if (operation.compare("dd") == 0)
			{
				LPVOID addr;
				try
				{
					addr = (LPVOID)(std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч����ֵ!\n");
					continue;
				}

				displayMemory(addr, 1);
			}

			else if (operation.compare("db") == 0)
			{
				LPVOID addr;
				try
				{
					addr = (LPVOID)(std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч����ֵ!\n");
					continue;
				}
				displayMemory(addr, 0);
			}
		
			else if (operation.compare("da") == 0)
			{
				LPVOID addr;
				try
				{
					addr = (LPVOID)(std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч����ֵ!\n");
					continue;
				}
				displayMemory(addr, 2);
			}
			else if (operation.compare("cls")==0)
			{
				system("cls");
			}
			else if (operation.compare("bm") == 0)
			{
				LPVOID addr;
				try
				{
					addr = (LPVOID)(std::stoi(address, nullptr, 16));
				}
				catch (exception& e)
				{
					printf("[!]������һ����Ч����ֵ!\n");
					continue;
				}
				breakpoint* bm = new bp_mm(addr);
				if (bm->install())
				{
					bpList.push_back(bm);
					sort(bpList.begin(), bpList.end(),cmp);
					bm->repair();
				}
				else
				{
					delete bm;
				}
			}

			//
			//	Ӳ���ϵ�
			//
			else if (operation.compare("ba") == 0)
			{
				if (address.length() != 0)
				{
					if (HrdBpCount() < 4)
					{
						LPVOID addr;
						try
						{
							addr = (LPVOID)(std::stoi(address, nullptr, 16));
						}
						catch (exception& e)
						{
							printf("[!]������һ����Ч����ֵ!\n");
							continue;
						}
						breakpoint *ba = new bp_hdr(addr);
						if (ba->install())
						{
							bpList.push_back(ba);
							sort(bpList.begin(), bpList.end(),cmp);
							ba->repair();
						}
						else
						{
							delete ba;
						}
					}
					else
					{
						printf("[!]ֻ�������ĸ�Ӳ���ϵ�\n");
					}
				}
				else
				{
					printf("[!]������һ���Ϸ��ĵ�ַ��\n");
				}
			}
			else
			{
				printf("[!]������һ����Ч��ָ�\n");
			}
		}
	}
	return 0;
}

bp_mm* findMMBp(LPVOID addr)
{
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		if ((*iter)->type == MEMORY_BREAKPOINT && (*iter)->isCurrent(addr))
			return (bp_mm*)(*iter);
	}
	return nullptr;
}

breakpoint* findHdrBp(LPVOID addr)
{
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		if ((*iter)->address == addr && (*iter)->type == HARDWARE_BREAKPOINT)
			return (*iter);
	}
	return nullptr;
}

int HrdBpCount()
{
	int count = 0;
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		if ((*iter)->type == HARDWARE_BREAKPOINT)
			count++;
	}
	return count;
}


void RepairMemory()
{
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		(*iter)->repair();
	}
}

void ReinstallBp()
{
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		(*iter)->install();
	}
}

bool isBreakpoint(LPVOID addr)
{
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		if ((*iter)->address == addr)
		{
			return true;
		}
	}
	return false;
}

bp_int3* getCurrentConditionBp(LPVOID addr)
{
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{
		if ((*iter)->address == addr && (*iter)->type == CONDITION_BREAKPOINT)
		{
			return (bp_int3*)(*iter);
		}
	}
	return nullptr;
}

bp_int3* getCurrentStepBp(LPVOID addr, DWORD& dwNum)
{
	DWORD i = 0;
	vector <breakpoint*>::iterator iter;
	for (iter = bpList.begin(); iter != bpList.end(); ++iter)
	{

		if ((*iter)->address == addr && (*iter)->type == STEP_OUT_BREAKPOINT)
		{
			dwNum = i;
			return (bp_int3*)(*iter);
		}
		i++;
	}
	return nullptr;
}

DWORD getAsmLineNum(DWORD dwBase, DWORD dwRange)
{
	DWORD dwRead = 0;
	char* buff = new char[dwRange + 16];
	ReadProcessMemory(hProcess,
		(LPVOID)dwBase,
		buff,
		dwRange+16,
		&dwRead
		);

	// ��������
	cs_insn* ins = nullptr;

	int count = 0;
	count = cs_disasm(handle,
		(uint8_t*)buff,
		dwRange+16,
		(DWORD)dwBase,
		0,
		&ins
		);

	for (int i = 0; i < count; i++)
	{
		if (ins[i].address > (dwBase + dwRange))
			return i - 1;
		else if (ins[i].address == (dwBase + dwRange))
			return i;
	}
	delete[] buff;
	cs_free(ins, count);
}

void getAddressInfo(DWORD addr)
{
	string pszName;
	GetSymName(addr, pszName);
	if (pszName.size() == 0)
	{
		printf("%08x\tû���ҵ�������Ϣ\n");
	}
	else
	{
		DWORD dwBase = GetSymAddress(pszName.c_str());
		DWORD dwLine = getAsmLineNum(dwBase, addr - dwBase);
		printf("%08x|\t%s +%d��\n", addr,pszName.c_str(), dwLine+1);
	}
}

void stackBacktracking()
{
	DWORD* buff = new DWORD[2];

	printf("EBP\t|��ַ \t |\t��Ϣ\n");

	printf("%08x|", ct.Ebp);
	// ���Ȼ�ȡ��ǰ��
	getAddressInfo(ct.Eip);

	// ��ȡջ��Ϣ
	DWORD dwRead = 0;
	ReadProcessMemory(OpenProcess(PROCESS_ALL_ACCESS, FALSE, de.dwProcessId),
		(LPVOID)ct.Ebp,
		buff,
		8,
		&dwRead
		);

	do 
	{
		printf("%08x|", buff[0]);
		getAddressInfo(buff[1]);
		ReadProcessMemory(OpenProcess(PROCESS_ALL_ACCESS, FALSE, de.dwProcessId),
			(LPVOID)buff[0],
			buff,
			8,
			&dwRead
			);
		
	} while (buff[0]!=0);
}

void displayModule()
{
	HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,dwPid);

	if (hModuleSnap == INVALID_HANDLE_VALUE)
		return;

	MODULEENTRY32 moduleInfo = { sizeof(MODULEENTRY32) };

	printf("���ػ�ַ | ģ���С |        ģ����         |   ģ��·��\n");
	Module32First(hModuleSnap, &moduleInfo);
	do
	{
		printf("%08X | ", moduleInfo.modBaseAddr);
		printf("%08d | ", moduleInfo.modBaseSize);
		wprintf(L"%-22s|", moduleInfo.szModule);
		wprintf(L"%s\n", moduleInfo.szExePath);

	} while (Module32Next(hModuleSnap, &moduleInfo));
}

DWORD getImageRange()
{
	HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPid);

	if (hModuleSnap == INVALID_HANDLE_VALUE)
		return 0;

	MODULEENTRY32 moduleInfo = { sizeof(MODULEENTRY32) };

	Module32First(hModuleSnap, &moduleInfo);
	do
	{
		if ((DWORD)moduleInfo.modBaseAddr == dwBaseOfImage)
		{
			return moduleInfo.modBaseSize;
		}
	} while (Module32Next(hModuleSnap, &moduleInfo));

	return 0;
}

void dump()
{
	DWORD dwSize;
	DWORD dwRead;
	dwSize = getImageRange();
	if (dwSize == 0)
	{
		printf("[!]��ȡģ����Ϣʧ��!\n");
		return;
	}
	char *buff = new char[dwSize];
	ReadProcessMemory(hProcess, (LPVOID)dwBaseOfImage, buff, dwSize, &dwRead);

	HANDLE hFile;
	hFile = CreateFile(L"Mydump.dmp", 
					GENERIC_WRITE, 
					FILE_SHARE_READ, 
					NULL,
					CREATE_NEW, 
					FILE_ATTRIBUTE_NORMAL,
					NULL);
	DWORD dwDataLen;
	WriteFile(hFile, buff, dwSize, &dwDataLen, NULL);
	printf("Size:%x  --  Written:%x\n", dwSize, dwDataLen);
	CloseHandle(hFile);
	delete[] buff;
}

void displayExchain()
{
	EXCEPTION_REGISTRATION_RECORD* fs0;
	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, de.dwThreadId);
	fs0 = GetThreadSEHAddress(hThread);

	DWORD dwRead = 0;
	EXCEPTION_REGISTRATION_RECORD except = { 0 };
	ReadProcessMemory(hProcess, (LPVOID)fs0, &except, sizeof(EXCEPTION_REGISTRATION_RECORD), &dwRead);

	DWORD dwCount = -1;

	do 
	{
		dwCount++;
		string symbol;
		GetSymName((DWORD)except.Handler, symbol);

		printf("-----------------[%d]--------------\n",dwCount);
		printf("Next:\t%08x\nHandle:\t%08x", except.Next, except.Handler);
		if (symbol.size() > 0)
		{
			printf("\t<%s>\n", symbol.c_str());
		}
		else
		{
			printf("\n ");
		}
		ReadProcessMemory(hProcess, (LPVOID)except.Next, &except, sizeof(EXCEPTION_REGISTRATION_RECORD), &dwRead);
		
	} while (except.Next != (LPVOID)-1);

	string symbol;
	GetSymName((DWORD)except.Handler, symbol);

	dwCount++;
	printf("-----------------[%d]--------------\n", dwCount);
	printf("Handle:\t%08x ", except.Handler);
	if (symbol.size() > 0)
	{
		printf("\t<%s>\n", symbol.c_str());
	}
	else
	{
		printf("\n ");
	}

}

void displayModeByte(LPVOID addr, char* buff)
{
	DWORD address = (DWORD)addr;
	for (int i = 0; i < 4; i++)
	{
		printf("%08x|", address + 16*i);
		for (int j = 0; j < 16; j++)
		{
			byte c = (byte)buff[i * 16 + j];
			printf("%02x ",unsigned int(c));
		}

		printf("\n");
	}
	delete[] buff;
}

void displayModeDword(LPVOID addr, char* buff)
{
	DWORD address = (DWORD)addr;
	for (int i = 0; i < 4; i++)
	{
		printf("%08x|", address + 16 * i);
		for (int j = 0; j < 4; j++)
		{
			for (int k = 0; k < 4; k++)
			{
				byte c = (byte)buff[16 * i + 4 * j + (3 - k)];
				printf("%02x", unsigned int(c));
			}
			printf(" ");
		}
		printf("\n");
	}
	delete[] buff;
}

void displayModeAscii(LPVOID addr, char* buff)
{
	printf("%08x|", addr);
	for (int i = 0; i < strlen(buff); i++)
	{
		if (buff[i] < 127 && buff[i]>31)
			printf("%c", buff[i]);
		else
		{
			printf("\n");
			break;
		}
	}
	printf("\n");
	delete[] buff;
}

void displayMemory(LPVOID addr, int mode = 0)
{
	//
	// mode = 0   ---> bb
	// mode = 1   ---> bd
	// mode = 2   ---> ba
	//

	char* buff = new char[65];
	ZeroMemory(buff, 65);
	cout << "��ַ    |      ����       " << endl;
	DWORD dwWrite = 0;
	ReadProcessMemory(hProcess,
		addr,
		buff,
		64,
		&dwWrite
		);

	switch (mode)
	{
	case 1:
		displayModeDword(addr, buff);
		break;
	case 2:
		displayModeAscii(addr, buff);
		break;
	default:
		displayModeByte(addr, buff);
		break;
	}
}

void editMemoryByte(LPVOID addr, byte b)
{
	DWORD dwSize = 0;
	byte localByte = b;
	WriteProcessMemory(hProcess, addr, &localByte, 1, &dwSize);
}

void editMemoryDword(LPVOID addr, DWORD dw)
{
	DWORD dwSize = 0;
	DWORD localDword = dw;
	WriteProcessMemory(hProcess, addr, &localDword, 4, &dwSize);
}

void IsCall(DWORD & nextCall)
{
	char buff[32];
	DWORD dwRead = 0;
	ReadProcessMemory(hProcess,
		(LPVOID)ct.Eip,
		buff,
		32,
		&dwRead);

	cs_insn* ins = nullptr;

	int count = 0;
	count = cs_disasm(handle,
		(uint8_t*)buff,
		32,
		ct.Eip,
		0,
		&ins
		);

	if (strcmp(ins[0].mnemonic, "call") == 0)
	{
		nextCall = ins[1].address;
	}
	else
	{
		nextCall = 0;
	}
}

void disassembly(LPVOID addr, int nNum)
{
	char buff[500];
	int total = 0;

	// ��ӡ������Ϣ
	cout << "��  ַ   | " << "             ������             | " << "ָ��\n";

	// ��ȡָ����Ϣ
	while (total < nNum)
	{
		DWORD dwWrite = 0;
		ReadProcessMemory(hProcess,
			addr,
			buff,
			500,
			&dwWrite
			);

		// ��������
		cs_insn* ins = nullptr;

		int count = 0;
		count = cs_disasm(handle,
			(uint8_t*)buff,
			120,
			(DWORD)addr,
			0,
			&ins
			);
		
		int printNum;
		if (count < nNum - total)
		{
			printNum = count;
		}
		else
		{
				printNum = nNum - total;
		}

		for (int i = 0; i <= printNum; ++i)
		{
			printf("%08X | ", ins[i].address);

			for (int j = 0; j < 16; j += 2)
			{
				if (ins[i].bytes[j] == 0xcd)
				{
					printf("  ");
				}
				else
					printf("%02x", ins[i].bytes[j]);

				if (ins[i].bytes[j+1] == 0xcd)
				{
					printf("  ");
				}
				else
					printf("%02x",  ins[i].bytes[j + 1]);
			}

			printf("|");

			if (strcmp(ins[i].mnemonic,"call") == 0)
			{
				string symbol;
				string addr = ins[i].op_str;
				GetSymName(std::stoi(addr, nullptr, 16), symbol);
				printf("%s %s", ins[i].mnemonic, ins[i].op_str);
				if (symbol.size()>0)
					cout << " <" << symbol << ">" << endl;
			}
			else
			printf("%s %s\n", ins[i].mnemonic, ins[i].op_str);
		}

		total += count;

		cs_free(ins, count);
	}
}

void Assembler(string instruction, LPVOID addr)
{

	const char* cmd = instruction.c_str();
	DWORD address = (DWORD)addr;
	ks_engine *ks;
	ks_err err;
	size_t count;
	unsigned char *encode;
	size_t size;

	err = ks_open(KS_ARCH_X86, KS_MODE_32, &ks);
	if (err != KS_ERR_OK) {
		printf("ERROR: failed on ks_open(), quit\n");
		return ;
	}

	if (ks_asm(ks, cmd, 0, &encode, &size, &count) != KS_ERR_OK) {
		printf("ERROR: ks_asm() failed & count = %lu, error = %u\n",
			count, ks_errno(ks));
	}
	else {
		size_t i;

		printf("%s = ", cmd);
		for (i = 0; i < size; i++) {
			printf("%02x ", encode[i]);
			byte c = (byte)encode[i];
			editMemoryByte((LPVOID)(address + i), c);
		}
		printf("\n");
		printf("Compiled: %lu bytes, statements: %lu\n", size, count);
	}

	// NOTE: free encode after usage to avoid leaking memory
	ks_free(encode);

	// close Keystone instance when done
	ks_close(ks);
}

SIZE_T GetSymAddress(const char* pszName)
{
	char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME*sizeof(TCHAR)];
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	// �������ֲ�ѯ������Ϣ�������pSymbol��
	if (!SymFromName(hProcess, pszName, pSymbol))
	{
		return 0;
	}
	return (SIZE_T)pSymbol->Address;	// ���غ�����ַ
}

BOOL GetSymName(SIZE_T nAddress, string& strName)
{
	DWORD64 dwDisplacement = 0;
	char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME*sizeof(TCHAR)];
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	if (!SymFromAddr(hProcess, nAddress, &dwDisplacement, pSymbol))
		return FALSE;

	strName = pSymbol->Name;
	return TRUE;
}