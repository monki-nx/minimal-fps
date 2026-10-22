// Shim de minimal-fps: RAII mínimo para HANDLE.
#pragma once
#include "WinAPI.h"

namespace pmon::util::win
{
	class Handle
	{
	public:
		Handle() noexcept = default;
		explicit Handle(HANDLE h) noexcept : h_{ h } {}
		~Handle() { Clear(); }
		Handle(const Handle&) = delete;
		Handle& operator=(const Handle&) = delete;
		Handle(Handle&& o) noexcept : h_{ o.h_ } { o.h_ = nullptr; }
		Handle& operator=(Handle&& o) noexcept { if (this != &o) { Clear(); h_ = o.h_; o.h_ = nullptr; } return *this; }
		operator HANDLE() const noexcept { return h_; }
		HANDLE Get() const noexcept { return h_; }
		void Clear() noexcept { if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); h_ = nullptr; }
		explicit operator bool() const noexcept { return h_ && h_ != INVALID_HANDLE_VALUE; }
	private:
		HANDLE h_ = nullptr;
	};
}
