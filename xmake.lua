add_rules("mode.debug", "mode.release")

if is_mode("debug") then
    set_runtimes("MDd")
else
    set_runtimes("MD")
end

target("Demangler")
    set_kind("static")
    set_languages("c++17")
    add_headerfiles("include/(**.h)", "include/(**.def)")
    add_includedirs("./include")
    add_cxflags("/utf-8", "/permissive-")
    add_files("src/**.cpp")
