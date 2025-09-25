#include "MemoryManager.h"
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

struct StackBoundary {
	unsigned long m_tid;
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

static std::vector<StackBoundary> GetThreadStackBoundaries() {
	std::vector<StackBoundary> stacks;

	auto pNtQueryInformationThread = reinterpret_cast<NtQueryInformationThread_t>(
		GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread")
		);
	if (!pNtQueryInformationThread) {
		return stacks;
	}

	DWORD pid = GetCurrentProcessId();
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return stacks;

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
					stacks.push_back({ te.th32ThreadID,
									  reinterpret_cast<uintptr_t>(tib->StackLimit),
									  reinterpret_cast<uintptr_t>(tib->StackBase) });
				}
				CloseHandle(hThread);
			}
		} while (Thread32Next(snapshot, &te));
	}

	CloseHandle(snapshot);
	return stacks;
}

#else
// ------------------- Linux -------------------
#include <fstream>
#include <sstream>
#include <string>
#include <dirent.h>
#include <unistd.h>

static std::vector<StackBoundary> GetThreadStackBoundaries() {
	std::vector<StackBoundary> stacks;
	pid_t pid = getpid();
	std::string taskDir = "/proc/" + std::to_string(pid) + "/task";
	DIR* dir = opendir(taskDir.c_str());
	if (!dir) return stacks;

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
					stacks.push_back({ (unsigned long)std::stoul(tidStr), start, end });
				}
			}
		}
	}
	closedir(dir);
	return stacks;
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
	char const* m_file;
	int m_line;
	std::size_t m_size;
	Element* m_next;
	Element() : m_ptr(nullptr),
		m_next(nullptr),
		m_isGarbage(true),
		m_file(nullptr),
		m_line(0),
		m_size(0) {}
};

Element* g_allocatedPointersHead = nullptr;
Element* g_allocatedPointersTail = nullptr;
Element* g_deletedPointersHead = nullptr;
Element* g_deletedPointersTail = nullptr;

void* g_stackTop = nullptr;

const char* g_newOperatorCallingFile = nullptr;
int g_newOperatorCallingLine = 0;

static unsigned GetAllocatedPointersCount()
{
	int counter = 0;
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
			ptrElement->m_file = trace;
			ptrElement->m_line = g_newOperatorCallingLine;
			ptrElement->m_size = size;
			ptrElement->m_next = nullptr;
			if (g_allocatedPointersHead == nullptr)
			{
				g_allocatedPointersHead = ptrElement;
				g_allocatedPointersTail = ptrElement;
			}
			else
			{
				g_allocatedPointersTail->m_next = ptrElement;
				g_allocatedPointersTail = ptrElement;
			}
			return ptr;
		}
	}
	throw std::bad_alloc{};
}


#undef new

void* operator new(std::size_t size)
{
	return MyNew(size);
}

void* operator new[](std::size_t size)
{
	return MyNew(size);
}

void operator delete(void* p)
{
	auto ite1 = g_allocatedPointersHead;
	auto ite2 = g_allocatedPointersHead;
	if (ite1 != nullptr && ite1->m_ptr == p)
	{
		if (g_allocatedPointersHead == g_allocatedPointersTail)
		{
			g_allocatedPointersTail = g_allocatedPointersTail->m_next;
		}
		g_allocatedPointersHead = g_allocatedPointersHead->m_next;
	}
	else
	{
		while (ite1 != nullptr)
		{
			ite1 = ite1->m_next;
			if (ite1 != nullptr && ite1->m_ptr == p)
			{
				ite2->m_next = ite1->m_next;
				if (g_allocatedPointersTail == ite1) {
					g_allocatedPointersTail = ite2;
				}
				break;
			}
			ite2 = ite2->m_next;
		}
	}

	if (ite1 != nullptr) { // move deleted pointer element from allocated to deleted list.
		ite1->m_next = nullptr;
		if (g_deletedPointersHead == nullptr) {
			g_deletedPointersHead = g_deletedPointersTail = ite1;
		}
		else if (g_deletedPointersHead->m_next == nullptr) {
			g_deletedPointersHead->m_next = g_deletedPointersTail = ite1;
		}
		else {
			g_deletedPointersTail->m_next = ite1;
			g_deletedPointersTail = ite1;
		}
	}

	free(p);
}

void operator delete[](void* p)
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

	for (size_t i = 0; i <= dataSize - patternSize; i++) {
		if (std::memcmp(reinterpret_cast<const uint8_t*>(data) + i, element, patternSize) == 0) {
			return true;
		}
	}
	return false;
}

void DetectDanglingPointers() {
	auto stacks = GetThreadStackBoundaries();

	//scan all threads stacks.
	for (auto const& stack : stacks) {
		auto ite = g_deletedPointersHead;
		auto stackSize = stack.m_end - stack.m_start;
		while (ite != nullptr)
		{
			if (IsPatternFound(reinterpret_cast<void*>(stack.m_start), stackSize, &ite->m_ptr, sizeof(void*)))
			{
				printf("\n\033[37;41m Dangling pointer of the deleted pointer allocated in:");
				printf("\033[0m\n %s", ite->m_file);
			}
			ite = ite->m_next;
		}
	}

	//scan reachable heap.
	auto ite = g_deletedPointersHead;
	while (ite != nullptr)
	{
		auto ite2 = g_allocatedPointersHead;
		while (ite2 != nullptr)
		{

			if (IsPatternFound(ite2->m_ptr, ite2->m_size, &ite->m_ptr, sizeof(void*)))
			{
				printf("\n\033[37;41m Dangling pointer of the deleted pointer allocated in:");
				printf("\033[0m\n %s", ite->m_file);
			}
			ite2 = ite2->m_next;
		}
		ite = ite->m_next;
	}
}

void DetectMemoryLeak()
{
	ResetAllocatedPointers();

	auto stacks = GetThreadStackBoundaries();
	//scan all threads stacks.
	for (auto const& stack : stacks) {
		auto ite = g_allocatedPointersHead;
		int dummy = 0;
		dummy++;
		void* stackBottom = &dummy;
		auto stackSize = stack.m_end - reinterpret_cast<uintptr_t>(stackBottom);
		if (stackSize > 40000) {
			stackSize = stack.m_end - stack.m_start;
			stackBottom = reinterpret_cast<void*>(stack.m_start);
		}
		while (ite != nullptr)
		{
			if (IsPatternFound(stackBottom, stackSize, &ite->m_ptr, sizeof(void*))) {
				ite->m_isGarbage = false; // pointer is reachable.
			}
			ite = ite->m_next;
		}
	}

	//scan reachable heap.
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		bool iteFixed = false;
		if (ite->m_isGarbage)
		{
			auto ite2 = g_allocatedPointersHead;
			while (ite2 != nullptr)
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
	while (ite != nullptr)
	{
		if (ite->m_isGarbage)
		{
			printf("\n\033[37;41m Memory leak detected of pointer allocated in:");
			printf("\033[0m\n %s", ite->m_file);
		}
		ite = ite->m_next;
	}
}

unsigned CollectGarbage()
{
	DetectMemoryLeak();
	unsigned count = 0;
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
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
		free((void*)ite->m_file);
		ite->m_file = nullptr;
		auto temp = ite;
		ite = ite->m_next;
		free(temp);
	}
	g_allocatedPointersHead = nullptr;
	g_allocatedPointersTail = nullptr;

	ite = g_deletedPointersHead;
	while (ite != nullptr)
	{
		if (ite->m_file) {
			free((void*)ite->m_file);
		}
		auto temp = ite;
		ite = ite->m_next;
		free(temp);
	}
	g_deletedPointersHead = nullptr;
	g_deletedPointersTail = nullptr;
}
