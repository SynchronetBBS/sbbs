/* Ordinary C++ new failure injection, linked only to the native Botan unit.
 * It replaces the actual calling-thread allocator, without guessing a native
 * object layout or fabricating a crypto-object failure result. */
#include <cstdlib>
#include <limits>
#include <new>
#include "key_file_cpp_alloc.h"

static thread_local bool active;
static thread_local bool failed;
static thread_local bool failed_nothrow;
static thread_local size_t fail_at;
static thread_local size_t attempts;
static thread_local size_t failed_size;
static thread_local bool nothrow_only, in_nothrow;
static thread_local size_t nothrow_size;

void
dssh_test_cpp_new_fail_after(size_t index)
{
	active = true;
	nothrow_only = false;
	failed = false;
	failed_nothrow = false;
	failed_size = 0;
	fail_at = index;
	attempts = 0;
}

void
dssh_test_cpp_new_fail_nothrow_size(size_t size)
{
	dssh_test_cpp_new_fail_after(0);
	nothrow_only = true;
	nothrow_size = size;
}

void
dssh_test_cpp_new_disable(void)
{
	active = false;
}

size_t
dssh_test_cpp_new_count(void)
{
	return attempts;
}

bool
dssh_test_cpp_new_failed(void)
{
	return failed;
}

bool
dssh_test_cpp_new_failed_nothrow(void)
{
	return failed_nothrow;
}

size_t
dssh_test_cpp_new_failed_size(void)
{
	return failed_size;
}

void *
operator new(std::size_t size)
{
	if (active && (!nothrow_only || (in_nothrow && size == nothrow_size))) {
		size_t index = attempts;
		if (attempts < std::numeric_limits<size_t>::max())
			attempts++;
		if (!failed && index == fail_at) {
			failed = true;
			failed_nothrow = in_nothrow;
			failed_size = size;
			throw std::bad_alloc();
		}
	}
	if (size == 0)
		size = 1;
	for (;;) {
		void *ptr = std::malloc(size);
		if (ptr != nullptr)
			return ptr;
		std::new_handler handler = std::get_new_handler();
		if (handler == nullptr)
			throw std::bad_alloc();
		handler();
	}
}

void *
operator new(std::size_t size, const std::nothrow_t &) noexcept
{
	bool previous = in_nothrow;
	in_nothrow = true;
	try {
		void *ptr = ::operator new(size);
		in_nothrow = previous;
		return ptr;
	}
	catch (...) {
		in_nothrow = previous;
		return nullptr;
	}
}

void
operator delete(void *ptr, const std::nothrow_t &) noexcept
{
	std::free(ptr);
}

void *
operator new[](std::size_t size)
{
	return ::operator new(size);
}

void
operator delete(void *ptr) noexcept
{
	std::free(ptr);
}

void
operator delete[](void *ptr) noexcept
{
	std::free(ptr);
}

void
operator delete(void *ptr, std::size_t) noexcept
{
	std::free(ptr);
}

void
operator delete[](void *ptr, std::size_t) noexcept
{
	std::free(ptr);
}
