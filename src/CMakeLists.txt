add_library(yin_yang_lib STATIC
    dt_timer.hpp glfw.hpp glfw.cpp ecs.hpp utils.hpp lofi.hpp sparse_cell.hpp thread_pool.hpp simd.hpp
    imgui/imgui_impl_glfw.cpp imgui/imgui_impl_glfw.h imgui/imgui_impl_opengl3_loader.h imgui/imgui_impl_opengl3.cpp imgui/imgui_impl_opengl3.h
    imgui/imconfig.h imgui/imgui_demo.cpp imgui/imgui_draw.cpp
    imgui/imgui_internal.h imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui.cpp imgui/imgui.h
    imgui/imstb_rectpack.h imgui/imstb_textedit.h imgui/imstb_truetype.h
    implot/implot_demo.cpp implot/implot_internal.h implot/implot_items.cpp implot/implot.cpp implot/implot.h)
target_link_libraries(yin_yang_lib PUBLIC ${ALL_DEPS} yin_yang_interface)
target_include_directories(yin_yang_lib PUBLIC .)

populate_filters()