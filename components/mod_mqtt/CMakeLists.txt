idf_component_register(
    SRCS "mod_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES pubsub parseutils mqtt
)
