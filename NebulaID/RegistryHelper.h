#pragma once
#include <string>
#include <vector>
#include <optional>

namespace reg {

	std::optional<std::wstring> ReadHKLMString(const wchar_t* subKey, const wchar_t* valueName);
	std::vector<std::wstring> ReadHKLMMultiSZ(const wchar_t* subKey, const wchar_t* valueName);

}
