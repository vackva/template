cmake_minimum_required(VERSION 3.18)
# The module is meant to run before project(); do exactly that.
include(${CMAKE_CURRENT_LIST_DIR}/../../git-version.cmake)
find_program(GIT git REQUIRED)

# A scratch repository: one commit, then a tag, then a dirty tree, then a
# pre-release tag, commits past it, and a second release line for MATCH.
set(_repo "${CMAKE_BINARY_DIR}/scratch-repo")
file(REMOVE_RECURSE "${_repo}")
file(MAKE_DIRECTORY "${_repo}")
file(WRITE "${_repo}/a.txt" "a\n")
set(_git "${GIT}" -c user.name=t -c user.email=t@t -c commit.gpgsign=false)
execute_process(COMMAND ${_git} init -q "${_repo}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${_git} -C "${_repo}" add a.txt COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${_git} -C "${_repo}" commit -q -m one COMMAND_ERROR_IS_FATAL ANY)

function(expect var regex)
    if(NOT "${${var}}" MATCHES "${regex}")
        message(FATAL_ERROR "${var} = '${${var}}' does not match '${regex}'")
    endif()
endfunction()

tanh_git_version("${_repo}")               # no tag: hash fallback
expect(TANH_VERSION_SHORT "^0\\.0\\.0$")
expect(TANH_VERSION_FULL "^0\\.0\\.0\\+g[0-9a-f]+$")
expect(TANH_VERSION_PRERELEASE "^$")
expect(TANH_VERSION_DISTANCE "^0$")

execute_process(COMMAND ${_git} -C "${_repo}" tag v1.2.3 COMMAND_ERROR_IS_FATAL ANY)
tanh_git_version("${_repo}")               # exactly on a tag
expect(TANH_VERSION_SHORT "^1\\.2\\.3$")
expect(TANH_VERSION_FULL "^1\\.2\\.3$")
expect(TANH_VERSION_PRERELEASE "^$")
expect(TANH_VERSION_DISTANCE "^0$")

file(APPEND "${_repo}/a.txt" "b\n")
tanh_git_version("${_repo}")               # dirty tree
expect(TANH_VERSION_FULL "^1\\.2\\.3-dirty$")
expect(TANH_VERSION_DISTANCE "^0$")

execute_process(COMMAND ${_git} -C "${_repo}" commit -q -a -m two COMMAND_ERROR_IS_FATAL ANY)
tanh_git_version("${_repo}")               # one commit past the tag
expect(TANH_VERSION_SHORT "^1\\.2\\.3$")
expect(TANH_VERSION_FULL "^1\\.2\\.3-1-g[0-9a-f]+$")
expect(TANH_VERSION_DISTANCE "^1$")

execute_process(COMMAND ${_git} -C "${_repo}" tag v1.3.0-alpha.1 COMMAND_ERROR_IS_FATAL ANY)
tanh_git_version("${_repo}")               # exactly on a pre-release tag
expect(TANH_VERSION_SHORT "^1\\.3\\.0$")   # project() gets digits only
expect(TANH_VERSION_FULL "^1\\.3\\.0-alpha\\.1$")
expect(TANH_VERSION_PRERELEASE "^alpha\\.1$")
expect(TANH_VERSION_DISTANCE "^0$")

file(APPEND "${_repo}/a.txt" "c\n")
execute_process(COMMAND ${_git} -C "${_repo}" commit -q -a -m three COMMAND_ERROR_IS_FATAL ANY)
tanh_git_version("${_repo}")               # one commit past the pre-release tag
expect(TANH_VERSION_SHORT "^1\\.3\\.0$")
expect(TANH_VERSION_FULL "^1\\.3\\.0-alpha\\.1-1-g[0-9a-f]+$")
expect(TANH_VERSION_PRERELEASE "^alpha\\.1$")
expect(TANH_VERSION_DISTANCE "^1$")

# A second release line on the same history (an annotated tag, which even a
# --tags-less describe would report): without MATCH the nearest tag wins, with
# MATCH the branch names itself after its own line only.
execute_process(COMMAND ${_git} -C "${_repo}" tag -a v2.0.0 -m v2.0.0 COMMAND_ERROR_IS_FATAL ANY)
tanh_git_version("${_repo}")
expect(TANH_VERSION_SHORT "^2\\.0\\.0$")
expect(TANH_VERSION_DISTANCE "^0$")
tanh_git_version("${_repo}" MATCH "v1*")
expect(TANH_VERSION_SHORT "^1\\.3\\.0$")
expect(TANH_VERSION_FULL "^1\\.3\\.0-alpha\\.1-1-g[0-9a-f]+$")
expect(TANH_VERSION_PRERELEASE "^alpha\\.1$")
expect(TANH_VERSION_DISTANCE "^1$")
tanh_git_version("${_repo}" MATCH "v9*")   # nothing matches: the no-tag fallback
expect(TANH_VERSION_SHORT "^0\\.0\\.0$")
expect(TANH_VERSION_FULL "^0\\.0\\.0\\+g[0-9a-f]+$")
expect(TANH_VERSION_PRERELEASE "^$")
expect(TANH_VERSION_DISTANCE "^0$")

# Not a checkout: the build tree may itself sit inside a git repository (a local
# clone of this one, with tags), so keep git from walking up past the build dir.
set(_plain "${CMAKE_BINARY_DIR}/not-a-repo")
file(MAKE_DIRECTORY "${_plain}")
set(ENV{GIT_CEILING_DIRECTORIES} "${CMAKE_BINARY_DIR}")
tanh_git_version("${_plain}")
unset(ENV{GIT_CEILING_DIRECTORIES})
expect(TANH_VERSION_SHORT "^0\\.0\\.0$")
expect(TANH_VERSION_FULL "^0\\.0\\.0$")
expect(TANH_VERSION_PRERELEASE "^$")
expect(TANH_VERSION_DISTANCE "^0$")

project(tanh_git_version_test VERSION ${TANH_VERSION_SHORT} LANGUAGES NONE)
enable_testing()
add_test(NAME git-version-configured COMMAND ${CMAKE_COMMAND} -E true)
