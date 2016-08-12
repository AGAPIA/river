#include "CommonExecutionController.h"
#include "../revtracer/DebugPrintFlags.h"

#include <Windows.h>

#include <Psapi.h>
#include <tlhelp32.h>
#include <Winternl.h>


class DefaultObserver : public ExecutionObserver {
public :
	unsigned int ExecutionBegin(void *ctx, void *address) {
		return EXECUTION_ADVANCE;
	}

	unsigned int ExecutionControl(void *ctx, void *address) {
		return EXECUTION_ADVANCE;
	}

	unsigned int ExecutionEnd(void *ctx) {
		return EXECUTION_TERMINATE;
	}

	void TerminationNotification(void *ctx) { }
} defaultObserver;


CommonExecutionController::CommonExecutionController() {
	execState = NEW;

	path = L"";
	cmdLine = L"";
	entryPoint = nullptr;
	featureFlags = EXECUTION_FEATURE_REVERSIBLE | EXECUTION_FEATURE_TRACKING;

	context = NULL;

	observer = &defaultObserver;

	virtualSize = commitedSize = 0;
	updated = false;

	trackCb = nullptr;
	markCb = nullptr;
}

int CommonExecutionController::GetState() const {
	return (int)execState;
}

bool CommonExecutionController::SetPath(const wstring &p) {
	if (execState == RUNNING) {
		return false;
	}

	path = p;
	execState = INITIALIZED;
	return true;
}

bool CommonExecutionController::SetCmdLine(const wstring &c) {
	if (execState == RUNNING) {
		return false;
	}

	cmdLine = c;
	return true;
}

bool CommonExecutionController::SetEntryPoint(void *ep) {
	if (execState == RUNNING) {
		return false;
	}

	entryPoint = ep;
	return true;
}

bool CommonExecutionController::SetExecutionFeatures(unsigned int feat) {
	featureFlags = feat;
	return true;
}

void CommonExecutionController::SetExecutionObserver(ExecutionObserver * obs) {
	observer = obs;
}

void CommonExecutionController::SetTrackingObserver(rev::TrackCallbackFunc track, rev::MarkCallbackFunc mark) {
	trackCb = track;
	markCb = mark;
}

unsigned int CommonExecutionController::ExecutionBegin(void *address, void *cbCtx) {
	execState = SUSPENDED_AT_START;
	return observer->ExecutionBegin(cbCtx, address);
}

unsigned int CommonExecutionController::ExecutionControl(void *address, void *cbCtx) {
	return observer->ExecutionControl(cbCtx, address);
}

unsigned int CommonExecutionController::ExecutionEnd(void *cbCtx) {
	execState = SUSPENDED_AT_TERMINATION;
	return observer->ExecutionEnd(cbCtx);
}

struct _VM_COUNTERS_ {
	ULONG PeakVirtualSize;
	ULONG VirtualSize; // << VM use 
	ULONG PageFaultCount;
	ULONG PeakWorkingSetSize;
	ULONG WorkingSetSize;
	ULONG QuotaPeakPagedPoolUsage;
	ULONG QuotaPagedPoolUsage;
	ULONG QuotaPeakNonPagedPoolUsage;
	ULONG QuotaNonPagedPoolUsage;
	ULONG PagefileUsage; // << Commited use
	ULONG PeakPagefileUsage;
};

bool CommonExecutionController::UpdateLayout() {
	if (updated) {
		return true;
	}

	ULONG retLen;
	_VM_COUNTERS_ ctrs;

	HANDLE hProcess = GetProcessHandle();

	NtQueryInformationProcess(
		hProcess,
		(PROCESSINFOCLASS)3,
		&ctrs,
		sizeof(ctrs),
		&retLen
	);

	if ((ctrs.VirtualSize == virtualSize) && (ctrs.PagefileUsage == commitedSize)) {
		return true;
	}

	virtualSize = ctrs.VirtualSize;
	commitedSize = ctrs.PagefileUsage;

	sec.clear();
	mod.clear();

	wchar_t mf[MAX_PATH];

	for (DWORD dwAddr = 0; dwAddr < 0x7FFF0000;) {
		MEMORY_BASIC_INFORMATION32 mbi;

		if (0 == VirtualQueryEx(hProcess, (LPCVOID)dwAddr, (PMEMORY_BASIC_INFORMATION)&mbi, sizeof(mbi))) {
			return false;
		}

		VirtualMemorySection vms;
		vms.BaseAddress = dwAddr;
		vms.Protection = mbi.Protect;
		vms.RegionSize = mbi.RegionSize;
		vms.State = mbi.State;
		vms.Type = mbi.Type;
		sec.push_back(vms);

		DWORD dwSz = GetMappedFileNameW(hProcess, (LPVOID)dwAddr, mf, (sizeof(mf) / sizeof(mf[0])) - 1);
		if (0 != dwSz) {
			wchar_t *module = mf;
			for (DWORD i = 1; i < dwSz; ++i) {
				if (mf[i - 1] == '\\') {
					module = &mf[i];
				}
			}

			for (wchar_t *t = module; *t != L'\0'; ++t) {
				*t = tolower(*t);
			}

			if ((0 == mod.size()) || (0 != wcscmp(module, mod[mod.size() - 1].Name))) {
				ModuleInfo mi;
				wcscpy_s(mi.Name, module);
				mi.ModuleBase = dwAddr;
				mi.Size = mbi.RegionSize;

				mod.push_back(mi);
			}
			else {
				ModuleInfo &mi = mod[mod.size() - 1];

				mi.Size = dwAddr - mi.ModuleBase + mbi.RegionSize;
			}
		}


		dwAddr = mbi.BaseAddress + mbi.RegionSize;
	}

	updated = true;
	return true;
}

bool CommonExecutionController::GetProcessVirtualMemory(VirtualMemorySection *&sections, int &sectionCount) {
	if (!UpdateLayout()) {
		return false;
	}

	sectionCount = sec.size();
	sections = &sec[0];

	return true;
}

bool CommonExecutionController::GetModules(ModuleInfo *&modules, int &moduleCount) {
	if (!UpdateLayout()) {
		return false;
	}

	moduleCount = mod.size();
	modules = &mod[0];
	return true;
}

int GeneratePrefix(char *buff, int size, ...) {
	va_list va;

	va_start(va, size);
	int sz = vsnprintf_s(
		buff,
		size,
		size - 1,
		"[%3s|%5s|%3s|%c] ",
		va
	);
	va_end(va);

	return sz;
}

HANDLE hDbg = CreateFileW(
	L"execution.log",
	GENERIC_WRITE,
	FILE_SHARE_READ,
	NULL,
	CREATE_ALWAYS,
	0,
	NULL
);

void vDebugPrintf(const DWORD printMask, const char *fmt, va_list args) {
	DWORD dwWr;
	char tmpBuff[512];

	char pfxBuff[] = "[___|_____|___|_]      ";
	static char lastChar = '\n';

	const char messageTypes[][4] = {
		"___",
		"ERR",
		"INF",
		"DBG"
	};

	const char executionStages[][6] = {
		"_____",
		"BRHND",
		"DIASM",
		"TRANS",
		"REASM",
		"RUNTM",
		"INSPT",
		"CNTNR"
	};

	const char codeTypes[][4] = {
		"___",
		"NAT",
		"RIV",
		"TRK",
		"SYM"
	};

	const char codeDirections[] = {
		'_', 'F', 'B'
	};

	if ('\n' == lastChar) {

		int sz = GeneratePrefix(
			pfxBuff,
			sizeof(pfxBuff),
			messageTypes[(printMask & PRINT_MESSAGE_MASK) >> PRINT_MESSAGE_SHIFT],
			executionStages[(printMask & PRINT_EXECUTION_MASK) >> PRINT_EXECUTION_SHIFT],
			codeTypes[(printMask & PRINT_CODE_TYPE_MASK) >> PRINT_CODE_TYPE_SHIFT],
			codeDirections[(printMask & PRINT_CODE_DIRECTION_MASK) >> PRINT_CODE_DIRECTION_SHIFT]
		);
		//debugLog.Write(pfxBuff, sz);

		WriteFile(hDbg, pfxBuff, sz, &dwWr, NULL);
	}

	int sz = vsnprintf_s(tmpBuff, sizeof(tmpBuff) - 1, sizeof(tmpBuff) - 1, fmt, args);
	
	if (sz) {
		//debugLog.Write(tmpBuff, sz);

		WriteFile(hDbg, tmpBuff, sz, &dwWr, NULL);
		lastChar = tmpBuff[sz - 1];
	}
}

void DebugPrintf(const unsigned int printMask, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vDebugPrintf(printMask, fmt, va);
	va_end(va);
}

void CommonExecutionController::DebugPrintf(const unsigned long printMask, const char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vDebugPrintf(printMask, fmt, va);
	va_end(va);
}

void CommonExecutionController::GetCurrentRegisters(void *ctx, rev::ExecutionRegs *regs) {
	gcr(ctx, regs);
}

void *CommonExecutionController::GetMemoryInfo(void *ctx, void *ptr) {
	return gmi(ctx, ptr);
}

void CommonExecutionController::SetSymbolicConstructor(rev::SymExeConstructorFunc constr) {
	symbolicConstructor = constr;
}

void CommonExecutionController::MarkMemoryName(void *ctx, rev::ADDR_TYPE addr, const char *name) {
	mmn(ctx, addr, name);
}

void CommonExecutionController::MarkMemoryValue(void *ctx, rev::ADDR_TYPE addr, rev::DWORD value) {
	mmv(ctx, addr, value);
}