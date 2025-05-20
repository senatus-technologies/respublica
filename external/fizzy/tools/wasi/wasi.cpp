// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasi.hpp"
#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include "uvwasi.hpp"
#include <uvwasi.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <ostream>

namespace fizzy::wasi
{
namespace
{
ExecutionResult return_enosys(std::any&, Instance&, const Value*, ExecutionContext&) noexcept
{
    return Value{uint32_t{UVWASI_ENOSYS}};
}

ExecutionResult proc_exit(
    std::any& host_context, Instance&, const Value* args, ExecutionContext&) noexcept
{
    auto* uvwasi = *std::any_cast<UVWASI*>(&host_context);

    uvwasi->proc_exit(static_cast<uvwasi_exitcode_t>(args[0].as<uint32_t>()));
    // Should not reach this.
    return Trap;
}

ExecutionResult fd_write(
    std::any& host_context, Instance& instance, const Value* args, ExecutionContext&) noexcept
{
    try
    {
        auto* uvwasi = *std::any_cast<UVWASI*>(&host_context);

        const auto fd = args[0].as<uint32_t>();
        const auto iov_ptr = args[1].as<uint32_t>();
        const auto iov_cnt = args[2].as<uint32_t>();
        const auto nwritten_ptr = args[3].as<uint32_t>();

        std::vector<uvwasi_ciovec_t> iovs(iov_cnt);
        // TODO: not sure what to pass as end, passing memory size...
        uvwasi_errno_t ret = uvwasi_serdes_readv_ciovec_t(
            instance.memory->data(), instance.memory->size(), iov_ptr, iovs.data(), iov_cnt);
        if (ret != UVWASI_ESUCCESS)
            return Value{uint32_t{ret}};

        uvwasi_size_t nwritten;
        ret = uvwasi->fd_write(static_cast<uvwasi_fd_t>(fd), iovs.data(), iov_cnt, &nwritten);
        uvwasi_serdes_write_uint32_t(instance.memory->data(), nwritten_ptr, nwritten);

        return Value{uint32_t{ret}};
    }
    catch (...)
    {
        return Value{uint32_t{UVWASI_EINVAL}};
    }
}

ExecutionResult fd_read(
    std::any& host_context, Instance& instance, const Value* args, ExecutionContext&) noexcept
{
    try
    {
        auto* uvwasi = *std::any_cast<UVWASI*>(&host_context);

        const auto fd = args[0].as<uint32_t>();
        const auto iov_ptr = args[1].as<uint32_t>();
        const auto iov_cnt = args[2].as<uint32_t>();
        const auto nread_ptr = args[3].as<uint32_t>();

        std::vector<uvwasi_iovec_t> iovs(iov_cnt);
        // TODO: not sure what to pass as end, passing memory size...
        uvwasi_errno_t ret = uvwasi_serdes_readv_iovec_t(
            instance.memory->data(), instance.memory->size(), iov_ptr, iovs.data(), iov_cnt);
        if (ret != UVWASI_ESUCCESS)
            return Value{uint32_t{ret}};

        uvwasi_size_t nread;
        ret = uvwasi->fd_read(static_cast<uvwasi_fd_t>(fd), iovs.data(), iov_cnt, &nread);
        uvwasi_serdes_write_uint32_t(instance.memory->data(), nread_ptr, nread);

        return Value{uint32_t{ret}};
    }
    catch (...)
    {
        return Value{uint32_t{UVWASI_EINVAL}};
    }
}

ExecutionResult fd_prestat_get(
    std::any& host_context, Instance& instance, const Value* args, ExecutionContext&) noexcept
{
    auto* uvwasi = *std::any_cast<UVWASI*>(&host_context);

    const auto fd = args[0].as<uint32_t>();
    const auto prestat_ptr = args[1].as<uint32_t>();

    uvwasi_prestat_t buf;
    uvwasi_errno_t ret = uvwasi->fd_prestat_get(fd, &buf);

    uvwasi_serdes_write_prestat_t(instance.memory->data(), prestat_ptr, &buf);

    return Value{uint32_t{ret}};
}

ExecutionResult environ_sizes_get(
    std::any&, Instance& instance, const Value* args, ExecutionContext&) noexcept
{
    const auto environc = args[0].as<uint32_t>();
    const auto environ_buf_size = args[1].as<uint32_t>();
    // TODO: implement properly (only returns 0 now)
    uvwasi_serdes_write_uint32_t(instance.memory->data(), environc, 0);
    uvwasi_serdes_write_uint32_t(instance.memory->data(), environ_buf_size, 0);
    return Value{uint32_t{UVWASI_ESUCCESS}};
}
}  // namespace

std::optional<bytes> load_file(std::string_view file, std::ostream& err) noexcept
{
    try
    {
        std::filesystem::path path{file};

        if (!std::filesystem::exists(path))
        {
            err << "File does not exist: " << path << "\n";
            return std::nullopt;
        }
        if (!std::filesystem::is_regular_file(path))
        {
            err << "Not a file: " << path << "\n";
            return std::nullopt;
        }

        std::ifstream wasm_file{path};
        if (!wasm_file)
        {
            err << "Failed to open file: " << path << "\n";
            return std::nullopt;
        }
        return bytes(std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});
    }
    catch (...)
    {
        // Generic error message due to other exceptions.
        err << "Failed to load: " << file << "\n";
        return std::nullopt;
    }
}

std::unique_ptr<Instance> instantiate(UVWASI& uvwasi, bytes_view wasm_binary)
{
    auto host_context = std::make_any<UVWASI*>(&uvwasi);

    constexpr auto ns = "wasi_snapshot_preview1";
    const std::vector<ImportedFunction> wasi_functions = {
        {ns, "proc_exit", {ValType::i32}, std::nullopt, {proc_exit, host_context}},
        {ns, "fd_read", {ValType::i32, ValType::i32, ValType::i32, ValType::i32}, ValType::i32,
            {fd_read, host_context}},
        {ns, "fd_write", {ValType::i32, ValType::i32, ValType::i32, ValType::i32}, ValType::i32,
            {fd_write, host_context}},
        {ns, "fd_prestat_get", {ValType::i32, ValType::i32}, ValType::i32,
            {fd_prestat_get, host_context}},
        {ns, "fd_prestat_dir_name", {ValType::i32, ValType::i32, ValType::i32}, ValType::i32,
            {return_enosys, host_context}},
        {ns, "environ_sizes_get", {ValType::i32, ValType::i32}, ValType::i32,
            {environ_sizes_get, host_context}},
        {ns, "environ_get", {ValType::i32, ValType::i32}, ValType::i32,
            {return_enosys, host_context}},
    };

    auto module = parse(wasm_binary);
    auto imports = resolve_imported_functions(*module, wasi_functions);
    return instantiate(std::move(module), std::move(imports), {}, {}, {}, MaxMemoryPagesLimit);
}

bool run(UVWASI& uvwasi, Instance& instance, int argc, const char* argv[], std::ostream& err)
{
    const uvwasi_errno_t uvwasi_err = uvwasi.init(static_cast<uvwasi_size_t>(argc), argv);
    if (uvwasi_err != UVWASI_ESUCCESS)
    {
        err << "Failed to initialise UVWASI: " << uvwasi_embedder_err_code_to_string(uvwasi_err)
            << "\n";
        return false;
    }

    const auto start_function = find_exported_function_index(*instance.module, "_start");
    if (!start_function.has_value())
    {
        err << "File is not WASI compatible (_start not found)\n";
        return false;
    }

    // Manually validate type signature here
    // TODO: do this in find_exported_function_index
    if (instance.module->get_function_type(*start_function) != FuncType{})
    {
        err << "File is not WASI compatible (_start has invalid signature)\n";
        return false;
    }

    if (!find_exported_memory(instance, "memory").has_value())
    {
        err << "File is not WASI compatible (no memory exported)\n";
        return false;
    }

    const auto result = execute(instance, *start_function, {});
    if (result.trapped)
    {
        err << "Execution aborted with WebAssembly trap\n";
        return false;
    }
    assert(!result.has_value);

    return true;
}

bool run(bytes_view wasm_binary, int argc, const char* argv[], std::ostream& err)
{
    auto uvwasi = create_uvwasi();
    auto instance = instantiate(*uvwasi, wasm_binary);
    assert(instance != nullptr);

    return run(*uvwasi, *instance, argc, argv, err);
}

bool load_and_run(int argc, const char** argv, std::ostream& err)
{
    const auto wasm_binary = load_file(argv[0], err);
    if (!wasm_binary)
        return false;

    return run(*wasm_binary, argc, argv, err);
}
}  // namespace fizzy::wasi
