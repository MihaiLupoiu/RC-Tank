idf_component_register(
    SRCS "mod_wifi.c"
    INCLUDE_DIRS "include" "."
    REQUIRES pubsub nvs_flash
)
