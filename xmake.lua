-- 优化后的 xmake.lua 文件，针对构建速度进行了全面优化

-- 设置项目名称
set_project("RandomTickOptimizer")

-- 设置项目版本
set_version("1.0.0")

-- 选项：目标类型 (server 或 client)
option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- 固定 LeviLamina 版本，避免每次拉取最新版导致缓存失效
add_requires("levilamina 1.9.5", {configs = {target_type = get_config("target_type")}})
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
set_optimize("fast")  -- 启用快速优化
set_symbols("none")   -- 减少调试符号，加快链接速度
set_exceptions("none") -- 禁用异常处理，提高性能

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
    set_pcxxflags("-fPIC") -- 用于共享库
    add_cxflags("-j4")    -- 启用并行编译，使用4个线程

-- 添加仓库
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- 修复：确保使用正确的字符串字面量，避免 Logger 问题
-- 这个修复在代码中已经处理，xmake.lua 不需要修改
