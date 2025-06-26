#pragma once

#include <fizzy/fizzy.h>

#include <koinos/vm/host_api.hpp>
#include <koinos/vm/module_cache.hpp>

namespace koinos::vm {

class program_context
{
public:
  program_context()                         = delete;
  program_context( const program_context& ) = delete;
  program_context( program_context&& )      = delete;
  program_context( host_api& h, const std::shared_ptr< module >& m ) noexcept;

  ~program_context();

  program_context& operator=( const program_context& ) = delete;
  program_context& operator=( program_context&& )      = delete;

  std::error_code start() noexcept;

  FizzyExecutionResult _wasi_args_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_args_sizes_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_seek( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_write( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_read( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_close( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_fdstat_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_proc_exit( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

  FizzyExecutionResult _koinos_get_caller( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_get_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_put_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_check_authority( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_log( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_exit( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

private:
  host_api* _hapi                   = nullptr;
  std::shared_ptr< module > _module = nullptr;
  FizzyInstance* _instance          = nullptr;
  FizzyExecutionContext* _context   = nullptr;
  std::int32_t _exit_code           = 0;

  std::error_code instantiate_module() noexcept;
};

} // namespace koinos::vm
