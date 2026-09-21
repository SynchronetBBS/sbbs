# Standalone unit tests for syncdrive's pure door modules: no engine, no
# termgfx library link (it drags in C++/libADLMIDI). -UNDEBUG because these
# tests ARE their assert()s.

function(syncdrive_test name)
	add_executable(${name} ${ARGN})
	target_include_directories(${name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../door
		${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx)
	set_target_properties(${name} PROPERTIES C_STANDARD 11)
	if(MSVC)
		target_compile_options(${name} PRIVATE -UNDEBUG /W3)
		target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_options(${name} PRIVATE -UNDEBUG -Wall -Wextra)
	endif()
	add_test(NAME ${name} COMMAND ${name})
endfunction()

syncdrive_test(test_frame
	${CMAKE_CURRENT_SOURCE_DIR}/../door/frame.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_frame.c)

syncdrive_test(test_keymap
	${CMAKE_CURRENT_SOURCE_DIR}/../door/keymap.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_keymap.c)

syncdrive_test(test_keyscript
	${CMAKE_CURRENT_SOURCE_DIR}/../door/keyscript.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_keyscript.c)

syncdrive_test(test_speaker
	${CMAKE_CURRENT_SOURCE_DIR}/../door/speaker.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_speaker.c)
if(NOT WIN32)
	target_link_libraries(test_speaker m)   # libm: no separate lib on MSVC
endif()

syncdrive_test(test_alias
	${CMAKE_CURRENT_SOURCE_DIR}/../door/alias.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_alias.c)

# The lock test is the one non-pure test here: it needs a second process
# contending for the same lock, and does it with fork() + mkdtemp(). *nix only,
# so the Windows build does not cover scores_lock.c (nor xpdev's LockFileEx
# path under it) -- the lock itself is exercised on every *nix build.
if(NOT WIN32)
	syncdrive_test(test_scores_lock
		${CMAKE_CURRENT_SOURCE_DIR}/../door/scores_lock.c
		${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev/filewrap.c
		${CMAKE_CURRENT_SOURCE_DIR}/test_scores_lock.c)
	target_include_directories(test_scores_lock PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev)
endif()
