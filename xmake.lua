-- 修复后的 xmake.lua 文件，已完全移除 target_type 选项

set_project("RandomTickOptimizer")
set_version("1.0.0")

-- 固定 LeviLamina 的 target_type 为 "server"（直接写死，不再需要命令行参数）
add_requires("levilamina 1.9.5", {configs = {target_type = "server"}})
add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

add_cxflags(
    "-O2", 
    "-march=native", 
    "-flto=auto", 
    "/EHa", 
    "/utf-8", 
    "/W4", 
    "/w44265", 
    "/w44289", 
    "/w44296", 
    "/w45263", 
    "/w44738", 
    "/w45204"
)
add_defines("NOMINMAX", "UNICODE")
set_languages("c++20")
set_optimize("fast")
set_symbols("none")
set_exceptions("none")

add_includedirs("src")

target("RandomTickOptimizer")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    set_kind("shared")
    set_languages("c++20")
    add_headerfiles("src/*.h")
    add_files("src/*.cpp")
    add_packages("levilamina")
    add_syslinks("shlwapi", "advapi32")
    set_targetdir("bin")
    set_runtimes("MD")
    set_pcxxflags("-fPIC")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
