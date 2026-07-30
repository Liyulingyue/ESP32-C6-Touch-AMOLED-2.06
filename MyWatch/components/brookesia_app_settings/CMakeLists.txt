idf_component_register(
    SRCS "app_settings.cpp" "page_navigator.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES main
    WHOLE_ARCHIVE
)
