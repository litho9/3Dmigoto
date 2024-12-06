#pragma once

#include <ctype.h>
#include <wchar.h>
#include <string>
#include <vector>
#include <map>

#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <D3Dcompiler.h>
#include <d3d9.h>
#include <DirectXMath.h>

#include "version.h"
#include "log.h"
#include "crc32c.h"
#include "util_min.h"

#include "D3D_Shaders\stdafx.h"
#include "DirectX11\HookedDevice.h"
#include "DirectX11\HookedContext.h"
const int INI_PARAMS_SIZE_WARNING = 256;

extern CRITICAL_SECTION resource_creation_mode_lock;

// Use the pretty lock debugging version if lock.h is included first, otherwise
// use the regular EnterCriticalSection:
#ifdef EnterCriticalSectionPretty
#define LockResourceCreationMode() \
	EnterCriticalSectionPretty(&resource_creation_mode_lock)
#else
#define LockResourceCreationMode() \
	EnterCriticalSection(&resource_creation_mode_lock)
#endif

#define UnlockResourceCreationMode() \
	LeaveCriticalSection(&resource_creation_mode_lock)

// Create hash code for textures or buffers.
static uint32_t crc32c_hw(const uint32_t seed, const void *buffer, const size_t length) {
	try {
		return crc32c_append(seed, static_cast<const uint8_t *>(buffer), length);
	} catch (...) {
		LogInfo("   ******* Exception caught while calculating crc32c_hw hash ******\n");
		return 0; // Fatal error, but catch it and return null for hash.
	}
}

// Primary hash calculation for all shader file names.

#define FNV_64_PRIME ((UINT64)0x100000001b3ULL) // 64 bit magic FNV-0 and FNV-1 prime
static UINT64 fnv_64_buf(const void *buf, const size_t len) {
	UINT64 hval = 0;
	auto *bp = static_cast<unsigned const char *>(buf);	/* start of buffer */
	unsigned const char *be = bp + len;		/* beyond end of buffer */

	// FNV-1 hash each octet of the buffer
	while (bp < be) {
		hval *= FNV_64_PRIME; // multiply by the 64 bit FNV magic prime mod 2^64 */
		hval ^= static_cast<UINT64>(*bp++); // xor the bottom with the current octet
	}
	return hval;
}

// Strip spaces from the right of a string.
// Returns a pointer to the last non-NULL character of the truncated string.
static char *RightStripA(char *buf) {
	char *end = buf + strlen(buf) - 1;
	while (end > buf && isspace(*end))
		end--;
	*(end + 1) = 0;
	return end;
}
static wchar_t *RightStripW(wchar_t *buf) {
	wchar_t *end = buf + wcslen(buf) - 1;
	while (end > buf && iswspace(*end))
		end--;
	*(end + 1) = 0;
	return end;
}

static char *readStringParameter(const wchar_t *val) {
	static char buf[MAX_PATH];
	wcstombs(buf, val, MAX_PATH);
	RightStripA(buf);
	char *start = buf; while (isspace(*start)) start++;
	return start;
}

static void BeepSuccess() {
	Beep(1800, 400); // High beep for success
}

static void BeepShort() {
	Beep(1800, 100); // Short High beep
}

static void BeepFailure() {
	Beep(200, 150); // Bonk sound for failure.
}

static void BeepFailure2() {
	Beep(300, 200); Beep(200, 150); // Brnk, dunk sound for failure.
}

static void BeepProfileFail(){
	// Brnk, du-du-dunk sound to signify the profile failed to install.
	// This is more likely to hit than the others for an end user (e.g. if
	// they denied admin privileges), so use a unique tone to make it
	// easier to identify.
	Beep(300, 300);
	Beep(200, 100);
	Beep(200, 100);
	Beep(200, 200);
}

static DECLSPEC_NORETURN void DoubleBeepExit() {
	// Fatal error somewhere, known to crash, might as well exit cleanly with some notification.
	BeepFailure2();
	Sleep(500);
	BeepFailure2();
	Sleep(200);
	if (LogFile) {
		// Make sure the log is written out so we see the failure message
		fclose(LogFile);
		LogFile = nullptr;
	}
	ExitProcess(0xc0000135);
}

static int _autoicmp(const wchar_t *s1, const wchar_t *s2) { return _wcsicmp(s1, s2); }
static int _autoicmp(const char *s1, const char *s2) { return _stricmp(s1, s2); }

// To use this function be sure to terminate an EnumName_t list with {NULL, 0}
// as it cannot use ArraySize on passed in arrays.
template <class T1, class T2>
static T2 lookup_enum_val(struct EnumName_t<T1, T2> *enum_names, T1 name, T2 default, bool *found= nullptr) {
	for (; enum_names->name; enum_names++) {
		if (!_autoicmp(name, enum_names->name)) {
			if (found)
				*found = true;
			return enum_names->val;
		}
	}

	if (found)
		*found = false;

	return default;
}
template <class T1, class T2>
static T2 lookup_enum_val(struct EnumName_t<T1, T2> *enum_names, T1 name, size_t len, T2 default, bool *found= nullptr)
{
	for (; enum_names->name; enum_names++) {
		if (!_wcsnicmp(name, enum_names->name, len)) {
			if (found)
				*found = true;
			return enum_names->val;
		}
	}

	if (found)
		*found = false;

	return default;
}
template <class T1, class T2>
static T1 lookup_enum_name(struct EnumName_t<T1, T2> *enum_names, T2 val) {
	for (; enum_names->name; enum_names++)
		if (val == enum_names->val)
			return enum_names->name;
	return NULL;
}

template <class T2>
static wstring lookup_enum_bit_names(struct EnumName_t<const wchar_t*, T2> *enum_names, T2 val) {
	wstring ret;
	T2 remaining = val;

	for (; enum_names->name; enum_names++) {
		if (static_cast<T2>(val & enum_names->val) == enum_names->val) {
			if (!ret.empty())
				ret += L' ';
			ret += enum_names->name;
			remaining = static_cast<T2>(remaining & (T2) ~enum_names->val);
		}
	}

	if (remaining != static_cast<T2>(0)) {
		wchar_t buf[20];
		wsprintf(buf, L"%x", remaining);
		if (!ret.empty())
			ret += L' ';
		ret += L"unknown:0x";
		ret += buf;
	}

	return ret;
}

// Parses an option string of names given by enum_names. The enum used with
// this function should have an INVALID entry, other flags declared as powers
// of two, and the SENSIBLE_ENUM macro used to enable the bitwise and logical
// operators. As above, the EnumName_t list must be terminated with {NULL, 0}
//
// If you wish to parse an option string that contains exactly one unrecognised
// argument, provide a pointer to a pointer in the 'unrecognised' field and the
// unrecognised option will be returned. Multiple unrecognised options are
// still considered errors.
template <class T1, class T2, class T3>
static T2 parse_enum_option_string(struct EnumName_t<T1, T2> *enum_names, T3 option_string, T1 *unrecognised)
{
	T3 ptr = option_string, cur;
	T2 ret = static_cast<T2>(0);
	T2 tmp = T2::INVALID;

	if (unrecognised)
		*unrecognised = NULL;

	while (*ptr) {
		// Skip over whitespace:
		for (; *ptr == L' '; ptr++) {}

		// Mark start of current entry:
		cur = ptr;

		// Scan until the next whitespace or end of string:
		for (; *ptr && *ptr != L' '; ptr++) {}

		if (*ptr) {
			// NULL terminate the current entry and advance pointer:
			*ptr = L'\0';
			ptr++;
		}

		// Lookup the value of the current entry:
		tmp = lookup_enum_val<T1, T2> (enum_names, cur, T2::INVALID);
		if (tmp != T2::INVALID) {
			ret |= tmp;
		} else {
			if (unrecognised && !(*unrecognised)) {
				*unrecognised = cur;
			} else {
				LogOverlayW(LOG_WARNING, L"WARNING: Unknown option: %s\n", cur);
				ret |= T2::INVALID;
			}
		}
	}
	return ret;
}

// Two template argument version is the typical case for now. We probably want
// to start adding the 'const' modifier in a bunch of places as we work towards
// migrating to C++ strings, since .c_str() always returns a const string.
// Since the parse_enum_option_string currently modified one of its inputs, it
// cannot use const, so the three argument template version above is to allow
// both const and non-const types passed in.
template <class T1, class T2>
static T2 parse_enum_option_string(struct EnumName_t<T1, T2> *enum_names, T1 option_string, T1 *unrecognised)
{
	return parse_enum_option_string<T1, T2, T1>(enum_names, option_string, unrecognised);
}

// This is similar to the above, but stops parsing when it hits an unrecognised
// keyword and returns the position without throwing any errors. It also
// doesn't modify the option_string, allowing it to be used with C++ strings.
template <class T1, class T2>
static T2 parse_enum_option_string_prefix(struct EnumName_t<T1, T2> *enum_names, T1 option_string, T1 *unrecognised)
{
	T1 ptr = option_string, cur;
	T2 ret = static_cast<T2>(0);
	T2 tmp = T2::INVALID;

	if (unrecognised)
		*unrecognised = NULL;

	while (*ptr) {
		// Skip over whitespace:
		for (; *ptr == L' '; ptr++) {}

		// Mark start of current entry:
		cur = ptr;

		// Scan until the next whitespace or end of string:
		for (; *ptr && *ptr != L' '; ptr++) {}

		// Note word length:
		size_t len = ptr - cur;

		// Advance pointer if not at end of string:
		if (*ptr)
			ptr++;

		// Lookup the value of the current entry:
		tmp = lookup_enum_val<T1, T2> (enum_names, cur, len, T2::INVALID);
		if (tmp != T2::INVALID) {
			ret |= tmp;
		} else {
			if (unrecognised)
				*unrecognised = cur;
			return ret;
		}
	}
	return ret;
}

// http://msdn.microsoft.com/en-us/library/windows/desktop/bb173059(v=vs.85).aspx
static const char *DXGIFormats[] = {
	"UNKNOWN",
	"R32G32B32A32_TYPELESS", "R32G32B32A32_FLOAT", "R32G32B32A32_UINT", "R32G32B32A32_SINT",
	"R32G32B32_TYPELESS", "R32G32B32_FLOAT", "R32G32B32_UINT", "R32G32B32_SINT",
	"R16G16B16A16_TYPELESS", "R16G16B16A16_FLOAT", "R16G16B16A16_UNORM", "R16G16B16A16_UINT", "R16G16B16A16_SNORM", "R16G16B16A16_SINT",
	"R32G32_TYPELESS", "R32G32_FLOAT", "R32G32_UINT", "R32G32_SINT", "R32G8X24_TYPELESS",
	"D32_FLOAT_S8X24_UINT", "R32_FLOAT_X8X24_TYPELESS", "X32_TYPELESS_G8X24_UINT",
	"R10G10B10A2_TYPELESS", "R10G10B10A2_UNORM", "R10G10B10A2_UINT", "R11G11B10_FLOAT",
	"R8G8B8A8_TYPELESS", "R8G8B8A8_UNORM", "R8G8B8A8_UNORM_SRGB", "R8G8B8A8_UINT", "R8G8B8A8_SNORM", "R8G8B8A8_SINT",
	"R16G16_TYPELESS", "R16G16_FLOAT", "R16G16_UNORM", "R16G16_UINT", "R16G16_SNORM", "R16G16_SINT",
	"R32_TYPELESS", "D32_FLOAT", "R32_FLOAT", "R32_UINT", "R32_SINT", "R24G8_TYPELESS",
	"D24_UNORM_S8_UINT", "R24_UNORM_X8_TYPELESS", "X24_TYPELESS_G8_UINT",
	"R8G8_TYPELESS", "R8G8_UNORM", "R8G8_UINT", "R8G8_SNORM", "R8G8_SINT",
	"R16_TYPELESS", "R16_FLOAT", "D16_UNORM", "R16_UNORM", "R16_UINT", "R16_SNORM", "R16_SINT", "R8_TYPELESS",
	"R8_UNORM", "R8_UINT", "R8_SNORM", "R8_SINT", "A8_UNORM", "R1_UNORM",
	"R9G9B9E5_SHAREDEXP", "R8G8_B8G8_UNORM", "G8R8_G8B8_UNORM",
	"BC1_TYPELESS", "BC1_UNORM", "BC1_UNORM_SRGB",
	"BC2_TYPELESS", "BC2_UNORM", "BC2_UNORM_SRGB",
	"BC3_TYPELESS", "BC3_UNORM", "BC3_UNORM_SRGB",
	"BC4_TYPELESS", "BC4_UNORM", "BC4_SNORM",
	"BC5_TYPELESS", "BC5_UNORM", "BC5_SNORM",
	"B5G6R5_UNORM", "B5G5R5A1_UNORM", "B8G8R8A8_UNORM", "B8G8R8X8_UNORM",
	"R10G10B10_XR_BIAS_A2_UNORM", "B8G8R8A8_TYPELESS", "B8G8R8A8_UNORM_SRGB", "B8G8R8X8_TYPELESS", "B8G8R8X8_UNORM_SRGB",
	"BC6H_TYPELESS", "BC6H_UF16", "BC6H_SF16", "BC7_TYPELESS", "BC7_UNORM", "BC7_UNORM_SRGB", "AYUV", "Y410", "Y416",
	"NV12", "P010", "P016", "420_OPAQUE", "YUY2", "Y210", "Y216", "NV11", "AI44", "IA44", "P8", "A8P8", "B4G4R4A4_UNORM"
};

static char *TexFormatStr(unsigned int format)
{
	if (format < sizeof(DXGIFormats) / sizeof(DXGIFormats[0]))
		return DXGIFormats[format];
	return "UNKNOWN";
}

static DXGI_FORMAT ParseFormatString(const char *fmt, bool allow_numeric_format)
{
	size_t num_formats = sizeof(DXGIFormats) / sizeof(DXGIFormats[0]);
	unsigned format;
	int end;

	if (allow_numeric_format) {
		// Try parsing format string as decimal:
		int nargs = sscanf_s(fmt, "%u%n", &format, &end);
		if (nargs == 1 && end == strlen(fmt))
			return static_cast<DXGI_FORMAT>(format);
	}

	if (!_strnicmp(fmt, "DXGI_FORMAT_", 12))
		fmt += 12;

	// Look up format string:
	for (format = 0; format < num_formats; format++) {
		if (!_strnicmp(fmt, DXGIFormats[format], 30))
			return static_cast<DXGI_FORMAT>(format);
	}

	// UNKNOWN/0 is a valid format (e.g. for structured buffers), so return
	// -1 cast to a DXGI_FORMAT to signify an error:
	return static_cast<DXGI_FORMAT>(-1);
}

static DXGI_FORMAT ParseFormatString(const wchar_t *wfmt, bool allow_numeric_format)
{
	char afmt[42];

	wcstombs(afmt, wfmt, 42);
	afmt[41] = '\0';

	return ParseFormatString(afmt, allow_numeric_format);
}

// From DirectXTK with extra formats added
static DXGI_FORMAT EnsureNotTypeless(const DXGI_FORMAT fmt)
{
    // Assumes UNORM or FLOAT; doesn't use UINT or SINT
    switch( fmt )
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R32G32B32_TYPELESS:       return DXGI_FORMAT_R32G32B32_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:    return DXGI_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_R32G32_TYPELESS:          return DXGI_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:     return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R16G16_TYPELESS:          return DXGI_FORMAT_R16G16_UNORM;
    case DXGI_FORMAT_R32_TYPELESS:             return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R8G8_TYPELESS:            return DXGI_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R16_TYPELESS:             return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R8_TYPELESS:              return DXGI_FORMAT_R8_UNORM;
    case DXGI_FORMAT_BC1_TYPELESS:             return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:             return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:             return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:             return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:             return DXGI_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:        return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_BC7_TYPELESS:             return DXGI_FORMAT_BC7_UNORM;
// Extra depth/stencil buffer formats not covered in DirectXTK (discards
// stencil buffer to allow binding to a shader resource, alternatively we could
// discard the depth buffer if we ever needed the stencil buffer):
    case DXGI_FORMAT_R32G8X24_TYPELESS:        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    case DXGI_FORMAT_R24G8_TYPELESS:           return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    default:                                   return fmt;
    }
}

// Is there already a utility function that does this?
static UINT dxgi_format_size(const DXGI_FORMAT format)
{
	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return 16;
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			return 12;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return 8;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return 4;
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return 2;
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_A8_UNORM:
			return 1;
		default:
			return 0;
	}
}


static const char* type_name(IUnknown *object)
{
	if (lookup_hooked_device(dynamic_cast<ID3D11Device1 *>(object)))
		return "Hooked_ID3D11Device";
	if (lookup_hooked_context(dynamic_cast<ID3D11DeviceContext1 *>(object)))
		return "Hooked_ID3D11DeviceContext";

	try {
		return typeid(*object).name();
	} catch (__non_rtti_object) {
		return "<NO_RTTI>";
	} catch(bad_typeid) {
		return "<NULL>";
	}
}

// Common routine to handle disassembling binary shaders to asm text.
// This is used whenever we need the Asm text.


// New version using Flugan's wrapper around D3DDisassemble to replace the
// problematic %f floating point values with %.9e, which is enough that a 32bit
// floating point value will be reproduced exactly:
static string BinaryToAsmText(const void *pShaderBytecode, const size_t BytecodeLength,
		const bool patch_cb_offsets,
		const bool disassemble_undecipherable_data = true,
		const int hexdump = 0, const bool d3dcompiler_46_compat = true)
{
	vector<byte> byteCode(BytecodeLength);
	vector<byte> disassembly;

	string comments = "//   using 3Dmigoto v" + string(VER_FILE_VERSION_STR) + " on " + LogTime() + "//\n";
	memcpy(byteCode.data(), pShaderBytecode, BytecodeLength);

	HRESULT r = disassembler(&byteCode, &disassembly, comments.c_str(), hexdump,
	                         d3dcompiler_46_compat, disassemble_undecipherable_data, patch_cb_offsets);
	if (FAILED(r)) {
		LogInfo("  disassembly failed. Error: %x\n", r);
		return "";
	}

	return string(disassembly.begin(), disassembly.end());
}

static string GetShaderModel(const void *pShaderBytecode, const size_t bytecodeLength)
{
	string asmText = BinaryToAsmText(pShaderBytecode, bytecodeLength, false);
	if (asmText.empty())
		return "";

	// Read shader model. This is the first not commented line.
	auto pos = (char *)asmText.data();
	const char *end = pos + asmText.size();
	while ((pos[0] == '/' || pos[0] == '\n') && pos < end)
	{
		while (pos[0] != 0x0a && pos < end) pos++;
		pos++;
	}
	// Extract model.
	char *eol = pos;
	while (eol[0] != 0x0a && pos < end) eol++;
	string shaderModel(pos, eol);

	return shaderModel;
}

// Create a text file containing text for the string specified.  Can be Asm or HLSL.
static HRESULT CreateTextFile(wchar_t *fullPath, const string *asmText, const bool overwrite) {
	FILE *f;

	if (!overwrite) {
		_wfopen_s(&f, fullPath, L"rb");
		if (f) {
			fclose(f);
			LogInfoW(L"    CreateTextFile error: file already exists %s\n", fullPath);
			return ERROR_FILE_EXISTS;
		}
	}

	_wfopen_s(&f, fullPath, L"wb");
	if (f) {
		fwrite(asmText->data(), 1, asmText->size(), f);
		fclose(f);
	}

	return S_OK;
}

// Get shader type from asm, first non-commented line.  CS, PS, VS.
// Not sure this works on weird Unity variant with embedded types.


// Specific variant to name files consistently, so we know they are Asm text.

static HRESULT CreateAsmTextFile(wchar_t* fileDirectory, const UINT64 hash, const wchar_t* shaderType,
	const void *pShaderBytecode, const size_t bytecodeLength, const bool patch_cb_offsets)
{
	string asmText = BinaryToAsmText(pShaderBytecode, bytecodeLength, patch_cb_offsets);
	if (asmText.empty())
	{
		return E_OUTOFMEMORY;
	}

	wchar_t fullPath[MAX_PATH];
	swprintf_s(fullPath, MAX_PATH, L"%ls\\%016llx-%ls.txt", fileDirectory, hash, shaderType);

	const HRESULT hr = CreateTextFile(fullPath, &asmText, false);

	if (SUCCEEDED(hr))
		LogInfoW(L"    storing disassembly to %s\n", fullPath);
	else
		LogInfoW(L"    error: %x, storing disassembly to %s\n", hr, fullPath);

	return hr;
}

// Specific variant to name files, so we know they are HLSL text.
static HRESULT CreateHLSLTextFile(UINT64 hash, string hlslText) { }

// Parses the name of one of the IniParam constants: x, y, z, w, x1, y1, ..., z7, w7
static bool ParseIniParamName(const wchar_t *name, int *idx, float DirectX::XMFLOAT4::**component) {
	int len1, len2;
	wchar_t component_chr;
	const size_t length = wcslen(name);
	const int ret = swscanf_s(name, L"%lc%n%u%n", &component_chr, 1, &len1, idx, &len2);

	// May or may not have matched index. Make sure entire string was
	// matched either way and check index is valid if it was matched:
	if (ret == 1 && len1 == length) {
		*idx = 0;
	} else if (ret == 2 && len2 == length) {
	} else {
		return false;
	}

	switch (towlower(component_chr)) {
		case L'x':
			*component = &DirectX::XMFLOAT4::x;
			return true;
		case L'y':
			*component = &DirectX::XMFLOAT4::y;
			return true;
		case L'z':
			*component = &DirectX::XMFLOAT4::z;
			return true;
		case L'w':
			*component = &DirectX::XMFLOAT4::w;
			return true;
	}

	return false;
}

// -----------------------------------------------------------------------------------------------

BOOL CreateDirectoryEnsuringAccess(LPCWSTR path);
errno_t wfopen_ensuring_access(FILE** pFile, const wchar_t *filename, const wchar_t *mode);
void set_file_last_write_time(wchar_t *path, FILETIME *ftWrite, DWORD flags=0);
void touch_file(wchar_t *path, DWORD flags=0);
#define touch_dir(path) touch_file(path, FILE_FLAG_BACKUP_SEMANTICS)

bool check_interface_supported(IUnknown *unknown, REFIID riid);
void analyse_iunknown(IUnknown *unknown);

// For the time being, since we are not setup to use the Win10 SDK, we'll add
// these manually. Some games under Win10 are requesting these.

struct _declspec(uuid("9d06dffa-d1e5-4d07-83a8-1bb123f2f841")) ID3D11Device2;
struct _declspec(uuid("420d5b32-b90c-4da4-bef0-359f6a24a83a")) ID3D11DeviceContext2;
struct _declspec(uuid("A8BE2AC4-199F-4946-B331-79599FB98DE7")) IDXGISwapChain2;
struct _declspec(uuid("94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1")) IDXGISwapChain3;
struct _declspec(uuid("3D585D5A-BD4A-489E-B1F4-3DBCB6452FFB")) IDXGISwapChain4;

std::string NameFromIID(IID id);

void WarnIfConflictingShaderExists(wchar_t *orig_path, const char *message = "");
static auto end_user_conflicting_shader_msg =
	"Conflicting shaders present - please use uninstall.bat and reinstall the fix.\n";

struct OMState {
	UINT NumRTVs;
	ID3D11RenderTargetView *rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *dsv;
	UINT UAVStartSlot;
	UINT NumUAVs;
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
void save_om_state(ID3D11DeviceContext *context, struct OMState *state);
void restore_om_state(ID3D11DeviceContext *context, struct OMState *state);

extern IDXGISwapChain *last_fullscreen_swap_chain;
void install_crash_handler(int level);
float get_effective_dpi();