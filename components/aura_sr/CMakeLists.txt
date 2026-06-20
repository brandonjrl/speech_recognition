if(IDF_TARGET STREQUAL "esp32s3")
    idf_component_register(SRCS "aura_sr.c"
                           INCLUDE_DIRS "include"
                           REQUIRES driver espressif__esp-sr esp_timer)
else()
    idf_component_register(SRCS "aura_sr.c"
                           INCLUDE_DIRS "include"
                           REQUIRES driver)
endif()
