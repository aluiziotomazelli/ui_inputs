idf_component_register(
    SRCS 
        "main.cpp"
        "test_button.cpp"
        "test_rotary_encoder.cpp"
    
    INCLUDE_DIRS
        "."
        "../../../external/idf_hals/mocks"
        
    REQUIRES 
        ui_inputs
        idf_hals
        gtest
        WHOLE_ARCHIVE
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()