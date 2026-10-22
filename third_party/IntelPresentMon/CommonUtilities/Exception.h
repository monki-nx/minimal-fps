// Shim de minimal-fps: versión mínima de la excepción base de PresentMon.
#pragma once
#include "str/String.h"
#include <exception>
#include <string>
#include <utility>

namespace pmon::util
{
	class Exception : public std::exception
	{
	public:
		Exception() noexcept = default;
		Exception(std::string msg) noexcept : note_{ std::move(msg) } {}
		const char* what() const noexcept override { return note_.c_str(); }
		const std::string& GetNote() const noexcept { return note_; }
	private:
		std::string note_;
	};

	template<class E = Exception, typename...R>
	auto Except(R&&...args)
	{
		return E{ std::forward<R>(args)... };
	}

	std::string ReportException(std::string note = {}) noexcept;
}

#define PM_DEFINE_EX_FROM(base, name) class name : public base { public: using base::base; }
#define PM_DEFINE_EX(name) class name : public ::pmon::util::Exception { public: using Exception::Exception; }
#define pmquell(stat) try { stat; } catch (...) {}
