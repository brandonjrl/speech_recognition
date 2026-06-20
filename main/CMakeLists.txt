set(srcs
    main.c
    )

set(requires
    aura_sr
    )

idf_component_register(SRCS ${srcs}
                       REQUIRES ${requires})
