#pragma once
#include <string>
#include <vector>

namespace util {

	std::string ToUpper(std::string s);
	void Trim(std::string& s);

	std::string W2U8(const std::wstring& ws);
	std::wstring U8ToW(const std::string& s);

	bool IsAllZerosOrOnes(const std::string& s);
	bool ContainsAny(const std::string& hay, const std::vector<std::string>& needles);
	bool IsGenericJunk(const std::string& v);          // "To be filled by O.E.M.", "00000000", etc.

	std::string Sanitize(const std::string& in);       // trim + remove CR/LF/TAB

	std::string Hex(const std::vector<uint8_t>& bytes, bool upper = true);
	std::string Grouped(const std::string& hex, size_t group = 4);

} // namespace util
