#-----------------------------------------------------------------
# build-mk manifest reader
#
# build-mk/ holds the authoritative source-ownership manifests. CMake cannot
# include make syntax, so this module evaluates the small subset those files
# use:
#
#   NAME := value        NAME = value        NAME ?= value        NAME += value
#   include path
#   backslash-newline continuations
#   $(NAME) references (recursively expanded)
#
# The manifests assign plain, whitespace-separated path lists. Make functions
# such as $(shell ...) or $(wildcard ...) are left unexpanded on purpose: no
# consumed list uses them, and guessing their meaning would be worse than
# leaving the value alone.
#
# These are macros, so all variables land in the caller's directory scope. A
# caller harvests the names it needs and then calls coduomp_manifest_reset().
#-----------------------------------------------------------------

set_property(GLOBAL PROPERTY CODUOMP_MANIFEST_VARS "")

macro(_coduomp_manifest_expand out_var text)
	set(${out_var} "${text}")
	set(_coduomp_expand_passes 0)
	while(_coduomp_expand_passes LESS 32 AND
		"${${out_var}}" MATCHES "\\$\\(([A-Za-z_][A-Za-z0-9_]*)\\)")
		math(EXPR _coduomp_expand_passes "${_coduomp_expand_passes} + 1")
		set(_coduomp_ref_name "${CMAKE_MATCH_1}")
		set(_coduomp_ref_text "${CMAKE_MATCH_0}")
		if(DEFINED ${_coduomp_ref_name})
			set(_coduomp_ref_value "${${_coduomp_ref_name}}")
		else()
			set(_coduomp_ref_value "")
		endif()
		string(REPLACE "${_coduomp_ref_text}" "${_coduomp_ref_value}"
			${out_var} "${${out_var}}")
	endwhile()
endmacro()

# coduomp_manifest_read(<file>)
#
# <file> may be absolute or relative to the repository root.
macro(coduomp_manifest_read file)
	set(_coduomp_file "${file}")
	if(NOT IS_ABSOLUTE "${_coduomp_file}")
		set(_coduomp_file "${CMAKE_SOURCE_DIR}/${_coduomp_file}")
	endif()
	if(NOT EXISTS "${_coduomp_file}")
		message(FATAL_ERROR
			"CoDUOMP: build manifest does not exist: ${_coduomp_file}")
	endif()

	file(READ "${_coduomp_file}" _coduomp_text)
	string(REPLACE "\r\n" "\n" _coduomp_text "${_coduomp_text}")
	string(REPLACE "\r" "\n" _coduomp_text "${_coduomp_text}")
	string(REPLACE "\\\n" " " _coduomp_text "${_coduomp_text}")
	string(REPLACE "\n" ";" _coduomp_lines "${_coduomp_text}")

	foreach(_coduomp_line IN LISTS _coduomp_lines)
		string(STRIP "${_coduomp_line}" _coduomp_line)
		if(_coduomp_line STREQUAL "")
			continue()
		endif()

		if(_coduomp_line MATCHES "^[-]?include[ \t]+([^ \t$]+)[ \t]*$")
			# Only plain paths are followed; $(...)-derived includes belong to
			# target rule files this module does not evaluate.
			coduomp_manifest_read("${CMAKE_MATCH_1}")
			continue()
		endif()

		if(NOT _coduomp_line MATCHES
			"^(override[ \t]+)?([A-Za-z_][A-Za-z0-9_]*)[ \t]*([+][=]|:=|[?][=]|=)[ \t]*(.*)$")
			# Rule lines, conditionals, function bodies and comments are not
			# variable assignments.
			continue()
		endif()

		set(_coduomp_name "${CMAKE_MATCH_2}")
		set(_coduomp_op "${CMAKE_MATCH_3}")
		_coduomp_manifest_expand(_coduomp_value "${CMAKE_MATCH_4}")
		string(STRIP "${_coduomp_value}" _coduomp_value)
		string(REGEX REPLACE "[ \t]+" ";" _coduomp_value "${_coduomp_value}")
		list(REMOVE_ITEM _coduomp_value "")

		if(_coduomp_op STREQUAL "?=")
			if(NOT DEFINED ${_coduomp_name})
				set(${_coduomp_name} "${_coduomp_value}")
			endif()
		elseif(_coduomp_op STREQUAL "+=")
			set(${_coduomp_name} "${${_coduomp_name}};${_coduomp_value}")
			list(REMOVE_ITEM ${_coduomp_name} "")
		else()
			set(${_coduomp_name} "${_coduomp_value}")
		endif()

		set_property(GLOBAL APPEND PROPERTY CODUOMP_MANIFEST_VARS
			"${_coduomp_name}")
	endforeach()
endmacro()

# Drop every variable created by coduomp_manifest_read() so make names such as
# CC, CFLAGS, TARGET or SRCS cannot leak into the CMake project scope.
function(coduomp_manifest_reset)
	get_property(_coduomp_vars GLOBAL PROPERTY CODUOMP_MANIFEST_VARS)
	foreach(_coduomp_name IN LISTS _coduomp_vars)
		unset(${_coduomp_name})
		unset(${_coduomp_name} PARENT_SCOPE)
	endforeach()
	set_property(GLOBAL PROPERTY CODUOMP_MANIFEST_VARS "")
endfunction()
