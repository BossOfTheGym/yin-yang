add_executable(yin_yang
    main.cpp dt_timer.hpp glfw.hpp glfw.cpp
    imgui/imgui_impl_glfw.cpp imgui/imgui_impl_glfw.h imgui/imgui_impl_opengl3_loader.h imgui/imgui_impl_opengl3.cpp imgui/imgui_impl_opengl3.h
    imgui/imconfig.h imgui/imgui_demo.cpp imgui/imgui_draw.cpp
    imgui/imgui_internal.h imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui.cpp imgui/imgui.h
    imgui/imstb_rectpack.h imgui/imstb_textedit.h imgui/imstb_truetype.h)
target_link_libraries(yin_yang ${ALL_DEPS})
target_include_directories(yin_yang PUBLIC .)

populate_filters()