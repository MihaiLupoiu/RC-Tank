idf_component_register(
    SRCS "main.c" 
    INCLUDE_DIRS "."
    REQUIRES pubsub mod_random mod_motor_drv8833)