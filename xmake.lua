-- 优化后的 xmake.lua 文件，已解决 --target_type 无效选项问题

-- 设置项目名称
set_project("RandomTickOptimizer")

-- 设置项目版本
set_version("1.0.0")

-- 固定 LeviLamina 版本，避免每次拉取最新版导致缓存失效
add_requires("levilamina 1.9.5", {configs = {target_type = "server"}})
add_requires("levibuildscript")

-- 设置运行时库
if not has_config("vs_runtime") then
    set_runtimes("MD")
end

-- 通用编译选项
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

-- 添加头文件目录
add_includedirs("src")

-- 配置目标
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
    add_cxflags("-j4")

-- 添加仓库
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
