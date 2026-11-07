set(database "${WORKING_DIRECTORY}/system-test.db")
set(setup_input "${WORKING_DIRECTORY}/system-test-setup.txt")
set(restart_input "${WORKING_DIRECTORY}/system-test-restart.txt")

file(REMOVE "${database}" "${setup_input}" "${restart_input}")
file(WRITE "${setup_input}" "admin01\nAdmin12345\nAdmin12345\n0\n")
file(WRITE "${restart_input}" "1\nadmin01\nAdmin12345\nq\n")

execute_process(
    COMMAND "${APPLICATION}" "${database}"
    INPUT_FILE "${setup_input}"
    OUTPUT_VARIABLE setup_output
    ERROR_VARIABLE setup_error
    RESULT_VARIABLE setup_result
)
if(NOT setup_result EQUAL 0)
    message(FATAL_ERROR
        "Initial production executable run failed (${setup_result}): ${setup_error}\n"
        "${setup_output}")
endif()
if(NOT setup_output MATCHES "Goodbye\\.")
    message(FATAL_ERROR "Initial production executable run did not exit cleanly")
endif()

execute_process(
    COMMAND "${APPLICATION}" "${database}"
    INPUT_FILE "${restart_input}"
    OUTPUT_VARIABLE restart_output
    ERROR_VARIABLE restart_error
    RESULT_VARIABLE restart_result
)
if(NOT restart_result EQUAL 0)
    message(FATAL_ERROR
        "Restarted production executable run failed (${restart_result}): ${restart_error}\n"
        "${restart_output}")
endif()
if(NOT restart_output MATCHES "Welcome, admin01 \\(Admin\\)\\.")
    message(FATAL_ERROR "Administrator was not persisted across application restart")
endif()
if(NOT restart_output MATCHES "Goodbye\\.")
    message(FATAL_ERROR "Restarted production executable run did not exit cleanly")
endif()

file(REMOVE "${database}" "${setup_input}" "${restart_input}")
