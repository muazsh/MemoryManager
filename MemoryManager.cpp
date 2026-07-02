#include "MemoryManager.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

struct StackBoundary {
	uintptr_t m_start;
	uintptr_t m_end;
};

#ifdef _WIN32
// ------------------- Windows -------------------
#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>

typedef struct _THREAD_BASIC_INFORMATION {
	NTSTATUS ExitStatus;
	PVOID TebBaseAddress;
	CLIENT_ID ClientId;
	KAFFINITY AffinityMask;
	KPRIORITY Priority;
	LONG BasePriority;
} THREAD_BASIC_INFORMATION;

typedef NTSTATUS(NTAPI* NtQueryInformationThread_t)(
	HANDLE ThreadHandle,
	ULONG ThreadInformationClass,
	PVOID ThreadInformation,
	ULONG ThreadInformationLength,
	PULONG ReturnLength
	);

static void GetThreadStackBoundaries(LinkedList<StackBoundary>& pStacks) {

	auto pNtQueryInformationThread = reinterpret_cast<NtQueryInformationThread_t>(
		GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread")
		);
	if (!pNtQueryInformationThread) {
		return;
	}

	DWORD pid = GetCurrentProcessId();
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return;

	THREADENTRY32 te;
	te.dwSize = sizeof(te);

	if (Thread32First(snapshot, &te)) {
		do {
			if (te.th32OwnerProcessID == pid) {
				HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
				if (!hThread) continue;

				THREAD_BASIC_INFORMATION tbi;
				if (pNtQueryInformationThread(hThread, 0 /* ThreadBasicInformation */,
					&tbi, sizeof(tbi), nullptr) == 0) {
					NT_TIB* tib = reinterpret_cast<NT_TIB*>(tbi.TebBaseAddress);
					pStacks.push_front({
						reinterpret_cast<uintptr_t>(tib->StackLimit),
						reinterpret_cast<uintptr_t>(tib->StackBase) });
				}
				CloseHandle(hThread);
			}
		} while (Thread32Next(snapshot, &te));
	}

	CloseHandle(snapshot);
	return;
}

bool IsAssignedToGlobalOrStatic(const void* p)
{
	uint8_t* baseAddress = reinterpret_cast<uint8_t*>(GetModuleHandle(NULL));
	if (!baseAddress) return false;

	// Parse PE Headers
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(baseAddress);
	PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(baseAddress + dosHeader->e_lfanew);
	PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

	for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
		if (strncmp(reinterpret_cast<const char*>(sectionHeader[i].Name), ".data", 5) == 0) {
			uint8_t* startAttr = baseAddress + sectionHeader[i].VirtualAddress;
			uint8_t* endAttr = startAttr + sectionHeader[i].Misc.VirtualSize;
			for (uint8_t* ptr = startAttr; ptr < endAttr; ptr+=4) {
				if (*reinterpret_cast<const uint8_t**>(ptr) == reinterpret_cast<const uint8_t*>(p)) {
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

#else
// ------------------- Linux -------------------
#include <fstream>
#include <sstream>
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <cstdint>

static void GetThreadStackBoundaries(LinkedList<StackBoundary>& pStacks) {
	pid_t pid = getpid();
	std::string taskDir = "/proc/" + std::to_string(pid) + "/task";
	DIR* dir = opendir(taskDir.c_str());
	if (!dir) return;

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		if (entry->d_name[0] == '.') continue;
		std::string tidStr(entry->d_name);
		std::string mapsFile = taskDir + "/" + tidStr + "/maps";

		std::ifstream f(mapsFile);
		if (!f.is_open()) continue;

		std::string line;
		while (std::getline(f, line)) {
			if (line.find("[stack") != std::string::npos) {
				std::stringstream ss(line);
				std::string addr;
				ss >> addr;
				auto dash = addr.find('-');
				if (dash != std::string::npos) {
					uintptr_t start = std::stoull(addr.substr(0, dash), nullptr, 16);
					uintptr_t end = std::stoull(addr.substr(dash + 1), nullptr, 16);
					pStacks.push_front({ start, end });
				}
			}
		}
	}
	closedir(dir);
	return;
}

extern "C" {
	extern char __data_start;
	extern char _edata;
	extern char __bss_start;
	extern char _end;
}

bool IsAssignedToGlobalOrStatic(const void* p)
{
	for (uint8_t* ptr = reinterpret_cast<uint8_t*>(&__data_start); 
		ptr < reinterpret_cast<uint8_t*>(&_edata); ptr++)
	{
		if (*reinterpret_cast<const uint8_t**>(ptr) == reinterpret_cast<const uint8_t*>(p)) 
		{
			return true;
		}
	}

	for (uint8_t* ptr = reinterpret_cast<uint8_t*>(&__bss_start);
		ptr < reinterpret_cast<uint8_t*>(&_end); ptr++)
	{
		if (*reinterpret_cast<const uint8_t**>(ptr) == reinterpret_cast<const uint8_t*>(p)) 
		{
			return true;
		}
	}
	return false;
}

#endif

#ifdef _WIN32
#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#else
#include <execinfo.h>
#include <unistd.h>
#include <cxxabi.h>
#endif

char* GetStackTrace(unsigned int maxFrames = 64) {
	// allocate an initial buffer (will grow if needed)
	size_t bufSize = 16 * 1024; // 16 KB
	char* buffer = (char*)malloc(bufSize);
	if (!buffer) return nullptr;
	buffer[0] = '\0';
	size_t offset = 0;

#ifdef _WIN32
	void* stack[64];
	USHORT frames = CaptureStackBackTrace(0, maxFrames, stack, NULL);

	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);

	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	for (USHORT i = 3; i < frames; i++) {
		char line[512];
		if (SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol)) {
			_snprintf_s(line, sizeof(line), _TRUNCATE, "%s\n", symbol->Name);
		}
		else {
			_snprintf_s(line, sizeof(line), _TRUNCATE, "???\n");
		}

		size_t len = strlen(line);
		if (offset + len + 1 > bufSize) {
			bufSize *= 2;
			buffer = (char*)realloc(buffer, bufSize);
			if (!buffer) return nullptr;
		}
		memcpy(buffer + offset, line, len);
		offset += len;
		buffer[offset] = '\0';
	}
	free(symbol);

#else
	void* stack[64];
	int frames = backtrace(stack, maxFrames);
	char** symbols = backtrace_symbols(stack, frames);

	if (symbols) {
		for (int i = 3; i < frames; i++) {
			char line[1024];
			const char* toWrite = symbols[i];

			// attempt demangling
			char* mangled = nullptr;
			char* demangled = nullptr;
			size_t sz = 0;
			int status = 0;

			const char* begin = strchr(symbols[i], '(');
			const char* plus = begin ? strchr(begin, '+') : nullptr;
			if (begin && plus && begin + 1 < plus) {
				mangled = strndup(begin + 1, plus - begin - 1);
				demangled = abi::__cxa_demangle(mangled, nullptr, &sz, &status);
				free(mangled);
				if (status == 0 && demangled) {
					toWrite = demangled;
				}
			}

			snprintf(line, sizeof(line), "%s\n", toWrite);

			size_t len = strlen(line);
			if (offset + len + 1 > bufSize) {
				bufSize *= 2;
				buffer = (char*)realloc(buffer, bufSize);
				if (!buffer) {
					if (demangled) free(demangled);
					free(symbols);
					return nullptr;
				}
			}
			memcpy(buffer + offset, line, len);
			offset += len;
			buffer[offset] = '\0';

			if (demangled) free(demangled);
		}
		free(symbols);
	}
#endif

	return buffer; // caller must free()
}

struct Element
{
	void* m_ptr;
	bool m_isGarbage;
	char const* m_stackTrace;
	std::size_t m_size;
	Element* m_next;
	Element() : m_ptr(nullptr),
		m_next(nullptr),
		m_isGarbage(true),
		m_stackTrace(nullptr),
		m_size(0) {}
};

Element* g_allocatedPointersHead = nullptr;
Element* g_deletedPointersHead = nullptr;

std::recursive_mutex g_alloc_dealloc_mtx;

static unsigned GetAllocatedPointersCount()
{
	unsigned counter = 0;
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		counter++;
		ite = ite->m_next;
	}
	return counter;
}

static void* MyNew(std::size_t size)
{
	auto trace = GetStackTrace();
	void* ptr = std::malloc(size);
	if (ptr)
	{
		Element* ptrElement = (Element*)std::malloc(sizeof(Element));
		if (ptrElement)
		{
			ptrElement->m_isGarbage = false;
			ptrElement->m_ptr = ptr;
			ptrElement->m_stackTrace = trace;
			ptrElement->m_size = size;
			ptrElement->m_next = nullptr;
			g_alloc_dealloc_mtx.lock();
			if (g_allocatedPointersHead == nullptr)
			{
				g_allocatedPointersHead = ptrElement;
			}
			else
			{
				ptrElement->m_next = g_allocatedPointersHead;
				g_allocatedPointersHead = ptrElement;
			}
			g_alloc_dealloc_mtx.unlock();
			return ptr;
		}
		free(ptr);
	}
	free(trace);
	throw std::bad_alloc{};
}

void* operator new(std::size_t size)
{
	return MyNew(size);
}

void* operator new[](std::size_t size)
{
	return MyNew(size);
}

void operator delete(void* p) noexcept
{
	if (p) {
		g_alloc_dealloc_mtx.lock();
		auto ite1 = g_allocatedPointersHead;
		auto ite2 = g_allocatedPointersHead;
		if (ite1 && ite1->m_ptr == p)
		{
			g_allocatedPointersHead = g_allocatedPointersHead->m_next;
		}
		else
		{
			while (ite1)
			{
				ite1 = ite1->m_next;
				if (ite1 && ite1->m_ptr == p)
				{
					ite2->m_next = ite1->m_next;
					break;
				}
				ite2 = ite2->m_next;
			}
		}

		if (ite1) { // move deleted pointer element from allocated to deleted list.
			ite1->m_next = nullptr;
			if (g_deletedPointersHead == nullptr) {
				g_deletedPointersHead = ite1;
			}
			else {
				ite1->m_next = g_deletedPointersHead;
				g_deletedPointersHead = ite1;
			}
		}
		free(p);
		g_alloc_dealloc_mtx.unlock();
	}
}

void operator delete[](void* p) noexcept
{
	delete(p);
}

void ResetAllocatedPointers()
{
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		ite->m_isGarbage = true;
		ite = ite->m_next;
	}
}

static bool IsPatternFound(const void* data, size_t dataSize, const void* element, size_t patternSize) {
	if (patternSize == 0 || dataSize < patternSize) {
		return false;
	}

	for (size_t i = 0; i <= dataSize - patternSize; i+=4) {
		if (std::memcmp(reinterpret_cast<const uint8_t*>(data) + i, element, patternSize) == 0) {
			return true;
		}
	}
	return false;
}

Element* RemoveElementFromDeletedList(Element* pPrev, Element* pCurr) {
	if (!pCurr) {
		return nullptr;
	}
	if (pPrev) {
		pPrev->m_next = pCurr->m_next;
		free((void*)pCurr->m_stackTrace);
		free(pCurr);
		return pPrev->m_next;
	}
	auto temp = g_deletedPointersHead;
	g_deletedPointersHead = g_deletedPointersHead->m_next;
	free((void*)temp->m_stackTrace);
	free(temp);
	return g_deletedPointersHead;
}

void DetectDanglingPointers() {
	struct IsInAllocatedChecker {
		static bool IsInAllocated(void* p) {
			if (!p) {
				return false;
			}
			Element* ptr = g_allocatedPointersHead;
			while (ptr) {
				if (ptr->m_ptr == p) {
					return true;
				}
				ptr = ptr->m_next;
			}
			return false;
		}
	};
	g_alloc_dealloc_mtx.lock();
	auto iter = g_deletedPointersHead;
	Element prev;
	while (iter != nullptr)
	{
		if (IsAssignedToGlobalOrStatic(iter->m_ptr))
		{
			if (IsInAllocatedChecker::IsInAllocated(iter->m_ptr))
			{
				iter = RemoveElementFromDeletedList(prev.m_next, iter);
				continue;
			}
			else
			{
				printf("\n\033[37;41m Dangling pointer of the deleted pointer allocated in:");
				printf("\033[0m\n %s", iter->m_stackTrace);
			}
		}
		prev.m_next = iter;
		iter = iter->m_next;
	}
	LinkedList<StackBoundary> stacks;
	GetThreadStackBoundaries(stacks);
	//scan all threads stacks.
	for (auto stack = stacks.head; stack != nullptr; stack = stack->next) {
		auto ite = g_deletedPointersHead;
		auto stackSize = stack->data.m_end - stack->data.m_start;
		Element prev;
		while (ite != nullptr)
		{
			if (IsInAllocatedChecker::IsInAllocated(ite->m_ptr))
			{
				ite = RemoveElementFromDeletedList(prev.m_next, ite);
				continue;
			}
			else {
				if (IsPatternFound(reinterpret_cast<void*>(stack->data.m_start), stackSize, &ite->m_ptr, sizeof(void*)))
				{
					printf("\n\033[37;41m Dangling pointer of the deleted pointer allocated in:");
					printf("\033[0m\n %s", ite->m_stackTrace);
				}
			}
			prev.m_next = ite;
			ite = ite->m_next;
		}
	}

	//scan reachable heap.
	auto ite = g_deletedPointersHead;
	prev.m_next = nullptr;
	while (ite != nullptr)
	{
		if (IsInAllocatedChecker::IsInAllocated(ite->m_ptr))
		{
			ite = RemoveElementFromDeletedList(prev.m_next, ite);
			continue;
		}
		else {
			auto ite2 = g_allocatedPointersHead;
			while (ite2 != nullptr)
			{

				if (IsPatternFound(ite2->m_ptr, ite2->m_size, &ite->m_ptr, sizeof(void*)))
				{
					printf("\n\033[37;41m Dangling pointer of the deleted pointer allocated in:");
					printf("\033[0m\n %s", ite->m_stackTrace);
				}

				ite2 = ite2->m_next;
			}
		}
		prev.m_next = ite;
		ite = ite->m_next;
	}
	g_alloc_dealloc_mtx.unlock();
}

void DetectMemoryLeak()
{
	g_alloc_dealloc_mtx.lock();
	ResetAllocatedPointers();
	auto iter = g_allocatedPointersHead;
	while (iter != nullptr)
	{
		if (iter->m_isGarbage && IsAssignedToGlobalOrStatic(iter->m_ptr)) {
			iter->m_isGarbage = false; // pointer is reachable.
		}
		iter = iter->m_next;
	}
	LinkedList<StackBoundary> stacks;
	GetThreadStackBoundaries(stacks);
	//scan all threads stacks.
	for (auto stack = stacks.head; stack != nullptr; stack = stack->next) {
		auto ite = g_allocatedPointersHead;
		auto stackSize = stack->data.m_end - stack->data.m_start;
		auto stackBottom = reinterpret_cast<void*>(stack->data.m_start);
		while (ite)
		{
			if (ite->m_isGarbage && IsPatternFound(stackBottom, stackSize, &ite->m_ptr, sizeof(void*))) {
				ite->m_isGarbage = false; // pointer is reachable.
			}
			ite = ite->m_next;
		}
	}

	//scan reachable heap.
	auto ite = g_allocatedPointersHead;
	while (ite)
	{
		bool iteFixed = false;
		if (ite->m_isGarbage)
		{
			auto ite2 = g_allocatedPointersHead;
			while (ite2)
			{
				if (!ite2->m_isGarbage)
				{
					int i = 0;
					if (IsPatternFound(ite2->m_ptr, ite2->m_size, &ite->m_ptr, sizeof(void*)))
					{
						ite->m_isGarbage = false; // pointer is reachable.
						iteFixed = true;
						break;
					}
				}
				ite2 = ite2->m_next;
			}
		}
		if (iteFixed)
		{
			ite = g_allocatedPointersHead;
			continue;
		}
		ite = ite->m_next;
	}

	ite = g_allocatedPointersHead;
	while (ite)
	{
		if (ite->m_isGarbage)
		{
			printf("\n\033[37;41m Memory leak detected of pointer allocated in:");
			printf("\033[0m\n %s", ite->m_stackTrace);
		}
		ite = ite->m_next;
	}
	g_alloc_dealloc_mtx.unlock();
}

unsigned CollectGarbage()
{
	DetectMemoryLeak();
	unsigned count = 0;
	auto ite = g_allocatedPointersHead;
	while (ite)
	{
		if (ite->m_isGarbage)
		{
			count++;
			auto next = ite->m_next;
			delete ite->m_ptr;
			ite = next;
		}
		else
		{
			ite = ite->m_next;
		}
	}
	return count;
}

void ResetAllocationList()
{
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		free(ite->m_ptr);
		ite->m_ptr = nullptr;
		free((void*)ite->m_stackTrace);
		ite->m_stackTrace = nullptr;
		auto temp = ite;
		ite = ite->m_next;
		free(temp);
	}
	g_allocatedPointersHead = nullptr;

	ite = g_deletedPointersHead;
	while (ite)
	{
		if (ite->m_stackTrace) {
			free((void*)ite->m_stackTrace);
		}
		auto temp = ite;
		ite = ite->m_next;
		free(temp);
	}
	g_deletedPointersHead = nullptr;
}
