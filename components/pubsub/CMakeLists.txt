idf_component_register(
    SRCS "src/pubsub.c"
    INCLUDE_DIRS "src"
)

set_source_files_properties(pubsub.c
    PROPERTIES COMPILE_FLAGS
    -DPS_FREE_RTOS
)
