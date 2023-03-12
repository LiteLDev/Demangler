add_rules("mode.debug", "mode.release")

target("Demangler")
    set_kind("static")
    set_languages("c++17")
    add_headerfiles("include/(**.h)", "include/(**.def)")
    add_includedirs("./include")
    add_cxxflags("/UTF-8", "/GL", "/permissive-")
    if is_mode("debug") then
        add_defines("DEBUG")
        add_cxxflags("/MDd")
        add_ldflags("/DEBUG")
    else
        add_defines("NDEBUG")
        add_cxxflags("/MD")
        add_ldflags("/OPT:REF", "/OPT:ICF")
    end
    add_ldflags("/LTCG")
    add_files("src/**.cpp")
