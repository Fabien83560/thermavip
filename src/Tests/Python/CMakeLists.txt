# The Python side of the shared memory protocol is checked by reading the module
# rather than by running it: the interpreter is not always built in, and the
# properties that matter are structural.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_test(NAME CheckThermavipProtocol
		 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_thermavip_protocol.py)
endif()

# The decay time estimation needs numpy and scipy; the check skips itself when
# they are not there rather than failing the suite.
if(Python3_Interpreter_FOUND)
	execute_process(COMMAND ${Python3_EXECUTABLE} -c "import numpy, scipy" RESULT_VARIABLE _vip_have_scipy OUTPUT_QUIET ERROR_QUIET)
	if(_vip_have_scipy EQUAL 0)
		add_test(NAME CheckEstimTau
			 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_estim_tau.py)
	endif()
endif()
