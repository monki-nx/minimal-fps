// Shim de minimal-fps: reemplaza el sistema de logging de PresentMon por
// no-ops, de modo que PresentData compile sin el resto de CommonUtilities.
#pragma once
#include <utility>

namespace pmon::util::log
{
	enum class Level { Fatal, Error, Warning, Info, Debug, Verbose, Verbose2, Performance };

	struct NullEntry
	{
		template<class...A> NullEntry& note(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& hr(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& code(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& every(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& watch(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& diag(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& subsys(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& to(A&&...) noexcept { return *this; }
		template<class...A> NullEntry& mark(A&&...) noexcept { return *this; }
		operator bool() const noexcept { return true; }
	};

	struct GlobalPolicy
	{
		static GlobalPolicy& Get() noexcept { static GlobalPolicy p; return p; }
		Level GetLogLevel() const noexcept { return Level::Error; }
		template<class T> static bool VCheck(T&&) noexcept { return false; }
	};
}

#define pmlog_(lvl)        ::pmon::util::log::NullEntry{}
#define pmlog_fatal        pmlog_(0).note
#define pmlog_error        pmlog_(0).note
#define pmlog_warn         pmlog_(0).note
#define pmlog_info         pmlog_(0).note
#define pmlog_dbg          pmlog_(0).note
#define pmlog_verb(vtag)   pmlog_(0).note
#define pmlog_verb2(v2tag) pmlog_(0).note
#define pmlog_perf(ptag)   pmlog_(0).note
#define pmwatch(expr)      watch(#expr, (expr))
