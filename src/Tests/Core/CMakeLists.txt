add_subdirectory(Archive)
add_subdirectory(ProcessingObject)
add_subdirectory(CommandOptions)

# The Python devices only exist when the interpreter is built in.
if(WITH_PYTHON)
	add_subdirectory(Python)
endif()
