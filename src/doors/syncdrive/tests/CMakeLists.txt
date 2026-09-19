# Standalone unit tests for syncdrive's pure door modules: no engine, no
# termgfx library link (it drags in C++/libADLMIDI). -UNDEBUG because these
# tests ARE their assert()s.

function(syncdrive_test name)
	add_executable(${name} ${ARGN})
	target_include_directories(${name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../door
		${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx)
	set_target_properties(${name} PROPERTIES C_STANDARD 11)
	target_compile_options(${name} PRIVATE -UNDEBUG -Wall -Wextra)
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
target_link_libraries(test_speaker m)

syncdrive_test(test_alias
	${CMAKE_CURRENT_SOURCE_DIR}/../door/alias.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_alias.c)

syncdrive_test(test_scores_lock
	${CMAKE_CURRENT_SOURCE_DIR}/../door/scores_lock.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev/filewrap.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_scores_lock.c)
target_include_directories(test_scores_lock PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev)
