idf_component_register(
    SRCS "main.c" "camera.c" "piccture.c" "com.c"
    INCLUDE_DIRS "."
    REQUIRES camera_driver image_processing connectivity
)
