/* Native Botan allocation controls and independently generated Base64 vectors.
 * Global replacement new is armed only during the actual public helper call.
 * C++ is required for allocator interception, not a C++ consumer API promise. */
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "deucessh.h"
#include "deucessh-crypto.h"

static thread_local bool allocation_armed;
static thread_local bool allocation_refused;
static thread_local unsigned allocation_calls;

void *operator new(std::size_t size)
{
	if (allocation_armed) {
		if (allocation_calls < UINT_MAX)
			allocation_calls++;
		if (allocation_refused)
			throw std::bad_alloc();
	}
	void *result = std::malloc(size == 0 ? 1 : size);
	if (result == nullptr)
		throw std::bad_alloc();
	return result;
}
void operator delete(void *pointer) noexcept
{
	std::free(pointer);
}
void operator delete(void *pointer, std::size_t) noexcept
{
	std::free(pointer);
}

static const struct {
	std::size_t length;
	unsigned pattern;
	const char *encoded;
} vectors[] = {
	{0, 0, ""},
	{1, 0, "AA=="},
	{2, 0, "AAE="},
	{3, 0, "AAEC"},
	{15, 0, "AAECAwQFBgcICQoLDA0O"},
	{16, 0, "AAECAwQFBgcICQoLDA0ODw=="},
	{31, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHg=="},
	{32, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="},
	{33, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8g"},
	{63, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+"},
	{64, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+Pw=="},
	{65, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0A="},
	{127, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fg=="},
	{128, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn8="},
	{129, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+A"},
	{254, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P0="},
	{255, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+"},
	{256, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w=="},
	{0, 1, ""},
	{1, 1, "AA=="},
	{2, 1, "AAA="},
	{3, 1, "AAAA"},
	{15, 1, "AAAAAAAAAAAAAAAAAAAA"},
	{16, 1, "AAAAAAAAAAAAAAAAAAAAAA=="},
	{31, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="},
	{32, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
	{33, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
	{63, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
	{64, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="},
	{65, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
	{127, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="},
	{128, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
	{129, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
	{254, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
	{255, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
	{256, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="},
	{0, 2, ""},
	{1, 2, "/w=="},
	{2, 2, "//8="},
	{3, 2, "////"},
	{15, 2, "////////////////////"},
	{16, 2, "/////////////////////w=="},
	{31, 2, "/////////////////////////////////////////w=="},
	{32, 2, "//////////////////////////////////////////8="},
	{33, 2, "////////////////////////////////////////////"},
	{63, 2, "////////////////////////////////////////////////////////////////////////////////////"},
	{64, 2, "/////////////////////////////////////////////////////////////////////////////////////w=="},
	{65, 2, "//////////////////////////////////////////////////////////////////////////////////////8="},
	{127, 2, "/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////w=="},
	{128, 2, "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////8="},
	{129, 2, "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"},
	{254, 2, "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////8="},
	{255, 2, "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"},
	{256, 2, "/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////w=="},
	{0, 3, ""},
	{1, 3, "KQ=="},
	{2, 3, "KXI="},
	{3, 3, "KXK7"},
	{15, 3, "KXK7BE2W3yhxugNMld4n"},
	{16, 3, "KXK7BE2W3yhxugNMld4ncA=="},
	{31, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutw=="},
	{32, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwA="},
	{33, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJ"},
	{63, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7X"},
	{64, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIA=="},
	{65, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGk="},
	{127, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOFw=="},
	{128, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2A="},
	{129, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp"},
	{254, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg6TJ7xA1Wn+gxesMMVZ7nMHnCC1Sd5i94wQpTnOUud8AJUpvkLXa/CFGa4yx1vgdQmeIrdL0GT5jhKnO8BU4="},
	{255, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg6TJ7xA1Wn+gxesMMVZ7nMHnCC1Sd5i94wQpTnOUud8AJUpvkLXa/CFGa4yx1vgdQmeIrdL0GT5jhKnO8BU6X"},
	{256, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg6TJ7xA1Wn+gxesMMVZ7nMHnCC1Sd5i94wQpTnOUud8AJUpvkLXa/CFGa4yx1vgdQmeIrdL0GT5jhKnO8BU6X4A=="},
	{191, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb4="},
	{192, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/"},
	{193, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wA=="},
	{383, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX4="},
	{384, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX5/"},
	{385, 0, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX5/gA=="},
	{191, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
	{192, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
	{193, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="},
	{383, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},
	{384, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
	{385, 1, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="},
	{191, 2, "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////8="},
	{192, 2, "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"},
	{193, 2, "/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////w=="},
	{383, 2, "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////8="},
	{384, 2, "////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"},
	{385, 2, "/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////w=="},
	{191, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDlc="},
	{192, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg"},
	{193, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg6Q=="},
	{383, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg6TJ7xA1Wn+gxesMMVZ7nMHnCC1Sd5i94wQpTnOUud8AJUpvkLXa/CFGa4yx1vgdQmeIrdL0GT5jhKnO8BU6X4ClyuwRNlt8ocboDTJXeJ3C5AkuU3SZvuAFKk9wlbrcASZLbJG22/0iR2iNstf5HkNkia7T9Ro/YIWqz/EWO1yBpsvtEjdYfaLH6Q4zVHmew+UKL1B1mr/hBitMcZa73QInSG2St9j+I0RpjrPU+h9AZYqv0PYbPGGGq8zyFzhc="},
	{384, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg6TJ7xA1Wn+gxesMMVZ7nMHnCC1Sd5i94wQpTnOUud8AJUpvkLXa/CFGa4yx1vgdQmeIrdL0GT5jhKnO8BU6X4ClyuwRNlt8ocboDTJXeJ3C5AkuU3SZvuAFKk9wlbrcASZLbJG22/0iR2iNstf5HkNkia7T9Ro/YIWqz/EWO1yBpsvtEjdYfaLH6Q4zVHmew+UKL1B1mr/hBitMcZa73QInSG2St9j+I0RpjrPU+h9AZYqv0PYbPGGGq8zyFzhdg"},
	{385, 3, "KXK7BE2W3yhxugNMld4ncLkCS5TdJm+4AUqT3CVutwBJktskbbb/SJHaI2y1/keQ2SJrtP1Gj9gharP8RY7XIGmy+0SN1h9osfpDjNUeZ7D5QovUHWav+EGK0xxlrvdAidIbZK32P4jRGmOs9T6H0Bliq/Q9hs8YYarzPIXOF2Cp8juEzRZfqPE6g8wVXqfwOYLLFF2m7ziByhNcpe43gMkSW6TtNn/IEVqj7DV+xxBZous0fcYPWKHqM3zFDleg6TJ7xA1Wn+gxesMMVZ7nMHnCC1Sd5i94wQpTnOUud8AJUpvkLXa/CFGa4yx1vgdQmeIrdL0GT5jhKnO8BU6X4ClyuwRNlt8ocboDTJXeJ3C5AkuU3SZvuAFKk9wlbrcASZLbJG22/0iR2iNstf5HkNkia7T9Ro/YIWqz/EWO1yBpsvtEjdYfaLH6Q4zVHmew+UKL1B1mr/hBitMcZa73QInSG2St9j+I0RpjrPU+h9AZYqv0PYbPGGGq8zyFzhdgqQ=="},
};

int main(void)
{
	const int offsets[] = {-256, -128, -4, -3, -2, -1, 0, 1, 2, 3, 4, 128, 256};
	unsigned cases = 0, failures = 0, allocations = 0, escapes = 0;
	for (const auto& vector : vectors) {
		if (vector.length > 385 || vector.pattern > 3)
			return 2;
		std::size_t encoded_length = std::strlen(vector.encoded);
		if (encoded_length > INT_MAX || encoded_length > 516)
			return 2;
		int encoded_result = static_cast<int>(encoded_length);
		for (int offset : offsets) {
			int position = 512 + offset;
			if (position < 0 || position > 768)
				return 2;
			std::size_t output_at = static_cast<std::size_t>(position);
			for (unsigned mode = 0; mode < 3; mode++) {
				std::array<uint8_t, 1536> frame;
				frame.fill(0xa5);
				for (std::size_t i = 0; i < vector.length; i++) {
					unsigned index = static_cast<unsigned>(i); /* i <= 384 */
					unsigned value = vector.pattern == 0 ? index & 255u : vector.pattern == 1 ? 0u
					    : vector.pattern == 2 ? 255u : (73u * index + 41u) & 255u;
					uint8_t byte = static_cast<uint8_t>(value); /* 0..255 */
					frame[512 + i] = byte;
				}
				auto expected = frame;
				if (mode != 2)
					std::memcpy(expected.data() + output_at, vector.encoded, encoded_length + 1);
				char *output = reinterpret_cast<char *>(frame.data() + output_at);
				int result = DSSH_ERROR_INIT;
				bool escaped = false;
				allocation_calls = 0;
				allocation_refused = mode == 1;
				allocation_armed = true;
				try {
					result = dssh_base64_encode(frame.data() + 512, vector.length, output,
					    mode == 2 ? encoded_length : encoded_length + 1);
				}
				catch (...) {
					escaped = true;
				}
				allocation_armed = false;
				allocation_refused = false;
				int wanted = mode == 2 ? DSSH_ERROR_TOOLONG : encoded_result;
				bool valid = !escaped && result == wanted && allocation_calls == 0 && frame == expected;
				if (!valid)
					failures++;
				if (escaped)
					escapes++;
				if (allocation_calls > UINT_MAX - allocations)
					allocations = UINT_MAX;
				else
					allocations += allocation_calls;
				cases++;
			}
		}
	}
	std::printf("base64_storage cases=%u vectors=96 offsets=13 failures=%u allocation_calls=%u escapes=%u\n",
	    cases, failures, allocations, escapes);
	return failures == 0 ? 0 : 1;
}
