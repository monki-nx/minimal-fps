// Shim de minimal-fps: implementaciones mínimas de las utilidades de
// PresentMon que PresentData necesita (QPC, hash, strings, excepciones).
#include "win/WinAPI.h"
#include "Qpc.h"
#include "Hash.h"
#include "str/String.h"
#include "Exception.h"
#include <guiddef.h>
#include <functional>

namespace pmon::util
{
	namespace
	{
		int64_t QpcFrequency_() noexcept
		{
			static const int64_t f = [] { LARGE_INTEGER li; QueryPerformanceFrequency(&li); return li.QuadPart; }();
			return f;
		}
	}

	int64_t GetCurrentTimestamp() noexcept
	{
		LARGE_INTEGER li; QueryPerformanceCounter(&li); return li.QuadPart;
	}
	double GetTimestampFrequencyDouble() noexcept { return double(QpcFrequency_()); }
	uint64_t GetTimestampFrequencyUint64() noexcept { return uint64_t(QpcFrequency_()); }
	double GetTimestampPeriodSeconds() noexcept { return 1.0 / double(QpcFrequency_()); }
	void SpinWaitUntilTimestamp(int64_t timestamp) noexcept
	{
		while (GetCurrentTimestamp() < timestamp) { YieldProcessor(); }
	}
	double TimestampDeltaToSeconds(int64_t start, int64_t end, double period) noexcept
	{
		return double(end - start) * period;
	}
	double TimestampDeltaToMilliSeconds(uint64_t duration, uint64_t qpcFrequency) noexcept
	{
		return qpcFrequency ? 1000.0 * double(duration) / double(qpcFrequency) : 0.0;
	}
	double TimestampDeltaToMilliSeconds(uint64_t start, uint64_t end, uint64_t qpcFrequency) noexcept
	{
		return (end > start) ? TimestampDeltaToMilliSeconds(end - start, qpcFrequency) : 0.0;
	}

	QpcTimer::QpcTimer() noexcept : performanceCounterPeriod_{ GetTimestampPeriodSeconds() } { Mark(); }
	double QpcTimer::Mark() noexcept
	{
		const auto elapsed = Peek();
		startTimestamp_ = GetCurrentTimestamp();
		return elapsed;
	}
	double QpcTimer::Peek() const noexcept
	{
		return TimestampDeltaToSeconds(startTimestamp_, GetCurrentTimestamp(), performanceCounterPeriod_);
	}
	double QpcTimer::SpinWaitUntil(double seconds) const noexcept
	{
		SpinWaitUntilTimestamp(TimeToTimestamp(seconds));
		return Peek();
	}
	int64_t QpcTimer::TimeToTimestamp(double seconds) const noexcept
	{
		return startTimestamp_ + int64_t(seconds / performanceCounterPeriod_);
	}

	namespace hash
	{
		size_t HashCombine(size_t lhs, size_t rhs) noexcept
		{
			return lhs ^ (rhs + 0x9e3779b97f4a7c15ull + (lhs << 6) + (lhs >> 2));
		}
		size_t HashGuid(const _GUID& g) noexcept
		{
			const auto* p = reinterpret_cast<const uint64_t*>(&g);
			return HashCombine(std::hash<uint64_t>{}(p[0]), std::hash<uint64_t>{}(p[1]));
		}
	}

	namespace str
	{
		std::wstring ToWide(const std::string& narrow) noexcept
		{
			if (narrow.empty()) return {};
			const int n = MultiByteToWideChar(CP_UTF8, 0, narrow.data(), int(narrow.size()), nullptr, 0);
			std::wstring out(size_t(n), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, narrow.data(), int(narrow.size()), out.data(), n);
			return out;
		}
		std::string ToNarrow(const std::wstring& wide) noexcept
		{
			if (wide.empty()) return {};
			const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
			std::string out(size_t(n), '\0');
			WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), out.data(), n, nullptr, nullptr);
			return out;
		}
	}

	std::string ReportException(std::string note) noexcept
	{
		try {
			if (auto p = std::current_exception()) std::rethrow_exception(p);
		}
		catch (const std::exception& e) { return note + e.what(); }
		catch (...) { return note + "unknown exception"; }
		return note;
	}
}
