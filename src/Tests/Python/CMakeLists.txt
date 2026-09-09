# The Python side of the shared memory protocol is checked by reading the module
# rather than by running it: the interpreter is not always built in, and the
# properties that matter are structural.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_test(NAME CheckThermavipProtocol
		 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_thermavip_protocol.py)
endif()
