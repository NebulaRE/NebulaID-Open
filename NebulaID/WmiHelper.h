#pragma once
#include <string>
#include <optional>

namespace WmiHelper {

	// consulta básica (prop 1 valor)
	std::optional<std::wstring> QuerySingleString(const wchar_t* wql, const wchar_t* prop, long timeoutMs = 3000);

	// fontes específicas
	std::optional<std::string> GetBaseboardSerial();
	std::optional<std::string> GetSystemUUID();
	std::optional<std::string> GetBiosSerial();
	std::optional<std::string> GetDiskSerialWmi();

	// virtualização
	bool IsVirtualMachine();

} // namespace WmiHelper
