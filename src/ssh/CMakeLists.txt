cmake_minimum_required(VERSION 3.21)
project(deuce-ssh VERSION 0.0 LANGUAGES C)

find_package(OpenSSL REQUIRED)

set(DEUCE_SSH_SOURCES
	ssh-arch.c ssh-trans.c ssh.c ssh-auth.c ssh-conn.c ssh-chan.c
	kex/curve25519-sha256.c
	kex/dh-gex-sha256.c
	key_algo/ssh-ed25519.c
	key_algo/rsa-sha2-256.c
	enc/aes256-ctr.c
	enc/none.c
	mac/hmac-sha2-256.c
	mac/none.c
	comp/none.c
)

set(DEUCE_SSH_HEADERS
	deucessh.h ssh-arch.h ssh-trans.h ssh-auth.h ssh-conn.h ssh-chan.h portable.h
	kex/curve25519-sha256.h kex/dh-gex-sha256.h
	key_algo/ssh-ed25519.h key_algo/rsa-sha2-256.h
	enc/aes256-ctr.h enc/none.h
	mac/hmac-sha2-256.h mac/none.h
	comp/none.h
)

set(DEUCE_SSH_COMPILE_OPTIONS
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
		-Wall -Wpedantic -pedantic -Werror>
	$<$<C_COMPILER_ID:MSVC>:
		/W4>)

# --- Static library (no visibility annotations) ---

add_library(deuce-ssh_static STATIC ${DEUCE_SSH_SOURCES})
set_property(TARGET deuce-ssh_static PROPERTY C_STANDARD 17)
set_property(TARGET deuce-ssh_static PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET deuce-ssh_static PROPERTY OUTPUT_NAME deuce-ssh)
target_sources(deuce-ssh_static INTERFACE FILE_SET HEADERS FILES ${DEUCE_SSH_HEADERS})
target_include_directories(deuce-ssh_static PUBLIC .)
target_include_directories(deuce-ssh_static PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(deuce-ssh_static INTERFACE ${OPENSSL_CRYPTO_LIBRARIES})
target_link_libraries(deuce-ssh_static INTERFACE stdthreads)
target_compile_definitions(deuce-ssh_static PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(deuce-ssh_static PRIVATE ${DEUCE_SSH_COMPILE_OPTIONS})

# --- Shared library (visibility annotations + hidden default) ---

add_library(deuce-ssh_shared SHARED ${DEUCE_SSH_SOURCES})
set_property(TARGET deuce-ssh_shared PROPERTY C_STANDARD 17)
set_property(TARGET deuce-ssh_shared PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET deuce-ssh_shared PROPERTY OUTPUT_NAME deuce-ssh)
set_property(TARGET deuce-ssh_shared PROPERTY POSITION_INDEPENDENT_CODE 1)
target_sources(deuce-ssh_shared INTERFACE FILE_SET HEADERS FILES ${DEUCE_SSH_HEADERS})
target_include_directories(deuce-ssh_shared PUBLIC .)
target_include_directories(deuce-ssh_shared PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(deuce-ssh_shared PRIVATE ${OPENSSL_CRYPTO_LIBRARIES})
target_link_libraries(deuce-ssh_shared PRIVATE stdthreads)
target_compile_definitions(deuce-ssh_shared
	PRIVATE
		DEUCE_SSH_SHARED
		$<$<CONFIG:Release>:NDEBUG>)
target_compile_options(deuce-ssh_shared PRIVATE
	${DEUCE_SSH_COMPILE_OPTIONS}
	$<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
		-fvisibility=hidden>)

# --- Test executables ---

add_executable(client EXCLUDE_FROM_ALL client.c)
target_link_libraries(client PRIVATE deuce-ssh_static)

add_executable(server EXCLUDE_FROM_ALL server.c)
target_link_libraries(server PRIVATE deuce-ssh_static)

install(TARGETS deuce-ssh_static deuce-ssh_shared)
