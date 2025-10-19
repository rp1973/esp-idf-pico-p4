idf_component_register(
    SRCS "main_app.c"
    INCLUDE_DIRS "."
    REQUIRES camera_driver image_processing connectivity
)
