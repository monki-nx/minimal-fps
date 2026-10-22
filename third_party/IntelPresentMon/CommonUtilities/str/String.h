// Shim de minimal-fps.
#pragma once
#include <string>

namespace pmon::util::str
{
	std::wstring ToWide(const std::string& narrow) noexcept;
	std::string ToNarrow(const std::wstring& wide) noexcept;
}
