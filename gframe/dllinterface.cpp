#ifdef YGOPRO_BUILD_DLL
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#ifdef __ANDROID__
#include "Android/porting_android.h"
#endif
#include <dlfcn.h>
#endif
#include "config.h"
#include "dllinterface.h"
#include "utils.h"

#if defined(_WIN32)
#define CORENAME EPRO_TEXT("ocgcore.dll")
#elif defined(EDOPRO_MACOS)
#define CORENAME EPRO_TEXT("libocgcore.dylib")
#elif defined(__ANDROID__)
#include <fcntl.h> //open()
#include <unistd.h> //close()
struct AndroidCore {
	void* library;
	int fd;
};
#if defined(__arm__)
#define CORENAME EPRO_TEXT("libocgcorev7.so")
#elif defined(__i386__)
#define CORENAME EPRO_TEXT("libocgcorex86.so")
#elif defined(__aarch64__)
#define CORENAME EPRO_TEXT("libocgcorev8.so")
#elif defined(__x86_64__)
#define CORENAME EPRO_TEXT("libocgcorex64.so")
#endif //__arm__
#elif defined(__linux__)
#define CORENAME EPRO_TEXT("libocgcore.so")
#endif //_WIN32

#define X(type,name,...) type(*name)(__VA_ARGS__) = nullptr;
#include "ocgcore_functions.inl"
#undef X

#define CREATE_CLONE(x) static auto x##_copy = x;
#define STORE_CLONE(x) x##_copy = x;
#define CLEAR_CLONE(x) x##_copy = nullptr;

#define X(type,name,...) CREATE_CLONE(name)
#include "ocgcore_functions.inl"
#undef X
#undef CREATE_CLONE

#ifdef _WIN32
static inline void* OpenLibrary(epro::path_stringview path) {
	return LoadLibrary(fmt::format("{}" CORENAME, path).data());
}
#define CloseLibrary(core) FreeLibrary((HMODULE)core)

#define GetFunction(core, x) (decltype(x))GetProcAddress((HMODULE)core, #x)

#elif defined(__ANDROID__)

static void* OpenLibrary(epro::path_stringview path) {
	void* lib = nullptr;
	auto dest_path = porting::internal_storage + "/libocgcoreXXXXXX.so";
	auto output = mkstemps(&dest_path[0], 3);
	if(output == -1)
		return nullptr;
	auto input = open(fmt::format("{}" CORENAME, path).data(), O_RDONLY);
	if(input == -1) {
		unlink(dest_path.data());
		close(output);
		return nullptr;
	}
	ygo::Utils::FileCopyFD(input, output);
	lib = dlopen(dest_path.data(), RTLD_NOW);
	unlink(dest_path.data());
	if(!lib) {
		close(output);
		close(input);
		return nullptr;
	}
	close(input);
	auto core = new AndroidCore;
	core->library = lib;
	core->fd = output;
	return core;
}

static inline void CloseLibrary(void* core) {
	AndroidCore* acore = static_cast<AndroidCore*>(core);
	dlclose(acore->library);
	close(acore->fd);
	delete acore;
}

#define GetFunction(core, x) (decltype(x))dlsym(static_cast<AndroidCore*>(core)->library, #x)

#else

static inline void* OpenLibrary(epro::path_stringview path) {
	return dlopen(fmt::format("{}" CORENAME, path).data(), RTLD_NOW);
}

#define CloseLibrary(core) dlclose(core)

#define GetFunction(core, x) (decltype(x))dlsym(core, #x)

#endif

#define RESTORE_CLONE(x) if(x##_copy) { x = x##_copy; x##_copy = nullptr; }

void RestoreFromCopies() {
#define X(type,name,...) RESTORE_CLONE(name)
#include "ocgcore_functions.inl"
#undef X
}

#undef RESTORE_CLONE

void ClearCopies() {
#define X(type,name,...) CLEAR_CLONE(name)
#include "ocgcore_functions.inl"
#undef X
}

bool check_api_version() {
	int min = 0;
	int max = 0;
	OCG_GetVersion(&max, &min);
	return (max == OCG_VERSION_MAJOR) && (min == OCG_VERSION_MINOR);
}

#define LOAD_FUNCTION(x) x = GetFunction(newcore, x);\
		if(!x){ UnloadCore(newcore); return nullptr; }

void* LoadOCGcore(epro::path_stringview path) {
	void* newcore = OpenLibrary(path);
	if(!newcore)
		return nullptr;
	ClearCopies();
#define X(type,name,...) LOAD_FUNCTION(name)
#include "ocgcore_functions.inl"
#undef X
	if(!check_api_version()) {
		UnloadCore(newcore);
		return nullptr;
	}
	return newcore;
}
#undef LOAD_FUNCTION

#define LOAD_WITH_COPY_CHECK(x) STORE_CLONE(x)\
		x = GetFunction(handle, x);\
		if(!x) {\
			RestoreFromCopies();\
			return false;\
		}

bool ReloadCore(void* handle) {
	if(!handle)
		return false;
#define X(type,name,...) LOAD_WITH_COPY_CHECK(name)
#include "ocgcore_functions.inl"
#undef X
	if(!check_api_version()) {
		RestoreFromCopies();
		return false;
	}
	ClearCopies();
	return true;
}

#undef LOAD_WITH_COPY_CHECK

#define CLEAR_FUNCTION(x) x = nullptr;

void UnloadCore(void* handle) {
	CloseLibrary(handle);
#define X(type,name,...) CLEAR_FUNCTION(name)
#include "ocgcore_functions.inl"
#undef X
}

#undef CLEAR_FUNCTION

#define CHANGE_WITH_COPY_CHECK(x) STORE_CLONE(x)\
		x = GetFunction(newcore, x);\
		if(!x) {\
			CloseLibrary(newcore);\
			RestoreFromCopies();\
			return nullptr;\
		}

void* ChangeOCGcore(epro::path_stringview path, void* handle) {
	void* newcore = OpenLibrary(path);
	if(!newcore)
		return nullptr;
#define X(type,name,...) CHANGE_WITH_COPY_CHECK(name)
#include "ocgcore_functions.inl"
#undef X
	if(!check_api_version()) {
		CloseLibrary(newcore);
		RestoreFromCopies();
		return nullptr;
	}
	ClearCopies();
	if(handle)
		CloseLibrary(handle);
	return newcore;
}
#undef CHANGE_WITH_COPY_CHECK
#undef STORE_CLONE
#undef CLEAR_CLONE

#endif //YGOPRO_BUILD_DLL
