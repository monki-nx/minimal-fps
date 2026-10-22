// Shim de minimal-fps: utilidades QPC que usa PresentData.
#pragma once
#include <cstdint>

namespace pmon::util
{
	int64_t GetCurrentTimestamp() noexcept;
	double GetTimestampFrequencyDouble() noexcept;
	uint64_t GetTimestampFrequencyUint64() noexcept;
	double GetTimestampPeriodSeconds() noexcept;
	void SpinWaitUntilTimestamp(int64_t timestamp) noexcept;
	double TimestampDeltaToSeconds(int64_t start, int64_t end, double period) noexcept;
	double TimestampDeltaToMilliSeconds(uint64_t duration, uint64_t qpcFrequency) noexcept;
	double TimestampDeltaToMilliSeconds(uint64_t start, uint64_t end, uint64_t qpcFrequency) noexcept;

	class QpcTimer
	{
	public:
		QpcTimer() noexcept;
		double Mark() noexcept;
		double Peek() const noexcept;
		int64_t GetStartTimestamp() const noexcept { return startTimestamp_; }
		double GetPerformanceCounterPeriod() const noexcept { return performanceCounterPeriod_; }
		double SpinWaitUntil(double seconds) const noexcept;
		int64_t TimeToTimestamp(double seconds) const noexcept;
	private:
		double performanceCounterPeriod_;
		int64_t startTimestamp_ = 0;
	};
}
