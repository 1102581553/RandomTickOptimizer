-- 项目名称和版本
set_project("RandomTickOptimizer")
set_version("1.0.0")

-- 先添加自定义仓库，确保后续依赖能正确找到
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- 固定 LeviLamina 的 target_type 为 "server"（直接写死，不再需要命令行参数）
add_requires("levilamina 1.9.5", {configs = {target_type = "server"}})
add_requires("levibuildscript")

-- 运行时库设置（如果用户未通过命令行指定）
if not has_config("vs_runtime") then
    set_runtimes("MD")
end

-- 编译器选项：按平台分别设置，避免跨平台冲突
if is_plat("windows") then
    -- Windows 专用选项
    add_cxflags(
        "/EHa",        -- 启用 SEH 异常（如果不需要可移除，此处保留以兼容原有设置）
        "/utf-8",      -- 设置源文件编码为 UTF-8
        "/W4",         -- 警告等级 4
        "/w44265",     -- 禁用特定警告（根据项目需要可调整）
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204"
    )
else
    -- 其他平台（如 Linux）选项
    add_cxflags(
        "-O2",
        "-march=native",
        "-flto=auto"
    )
end

-- 通用定义和设置
add_defines("NOMINMAX", "UNICODE")
set_languages("c++20")
set_optimize("fast")
set_symbols("none")
set_exceptions("none")   -- 禁用 C++ 异常（与 /EHa 同时使用时需注意：/EHa 会启用 SEH 和 C++ 异常）
                         -- 若不需要 SEH，建议移除 /EHa；若需要 SEH，可改为 set_exceptions("seh") 并移除 /EHa

add_includedirs("src")   -- 全局包含路径（若后续新增 target 且不需要此路径，建议移入 target 内）

-- 主目标
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
    set_runtimes("MD")   -- 确保 target 使用 MD 运行时（与全局设置一致）
