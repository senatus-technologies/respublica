#pragma once

#include <cassert>
#include <fizzy/fizzy.h>

#include <koinos/vm/error.hpp>
#include <koinos/vm/host_api.hpp>
#include <koinos/vm/module_cache.hpp>
#include <type_traits>
#include <utility>

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

  FizzyExecutionResult wasi_args_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult wasi_args_sizes_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult wasi_fd_seek( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult wasi_fd_write( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult wasi_fd_read( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult wasi_fd_close( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult wasi_fd_fdstat_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult wasi_proc_exit( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

  FizzyExecutionResult koinos_get_caller( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult koinos_get_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult koinos_put_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult koinos_check_authority( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

private:
  host_api* _host_api               = nullptr;
  std::shared_ptr< module > _module = nullptr;
  FizzyInstance* _instance          = nullptr;
  FizzyExecutionContext* _context   = nullptr;
  std::uint64_t _previous_ticks     = 0;
  std::int32_t _exit_code           = 0;

  std::error_code instantiate_module() noexcept;

  template< typename T >
    requires( std::is_pointer_v< T > )
  T native_pointer( std::uint32_t ptr, std::uint32_t size = sizeof( std::remove_pointer_t< T > ) ) const noexcept;

  template<>
  void* native_pointer< void* >( std::uint32_t ptr, std::uint32_t size ) const noexcept;

  result< std::vector< io_vector > > make_iovs( std::uint32_t iovs, std::uint32_t iovs_len ) const noexcept;

  template< typename Lambda >
    requires( !std::is_void_v< std::invoke_result_t< Lambda > > )
  auto with_meter_ticks( const Lambda& lambda ) -> std::expected< decltype( lambda() ), std::int32_t >;

  template< typename Lambda >
    requires( std::is_void_v< std::invoke_result_t< Lambda > > )
  auto with_meter_ticks( const Lambda& lambda ) -> std::int32_t;
};

template< typename T >
  requires( std::is_pointer_v< T > )
T program_context::native_pointer( std::uint32_t ptr, std::uint32_t size ) const noexcept
{
  return static_cast< T >( native_pointer< void* >( ptr, size ) );
}

template< typename Lambda >
  requires( !std::is_void_v< std::invoke_result_t< Lambda > > )
auto program_context::with_meter_ticks( const Lambda& lambda ) -> std::expected< decltype( lambda() ), std::int32_t >
{
  std::int64_t* ticks = fizzy_get_execution_context_ticks( _context );
  assert( ticks );

  if( auto code = _host_api->use_meter_ticks( _previous_ticks - std::bit_cast< std::uint64_t >( *ticks ) ); code )
    return std::unexpected( code );

  auto result = lambda();

  _previous_ticks = _host_api->get_meter_ticks();
  *ticks          = std::bit_cast< std::int64_t >( _previous_ticks );

  return result;
}

template< typename Lambda >
  requires( std::is_void_v< std::invoke_result_t< Lambda > > )
auto program_context::with_meter_ticks( const Lambda& lambda ) -> std::int32_t
{
  std::int64_t* ticks = fizzy_get_execution_context_ticks( _context );
  assert( ticks );

  if( auto code = _host_api->use_meter_ticks( _previous_ticks - std::bit_cast< std::uint64_t >( *ticks ) ); code )
    return code;

  lambda();

  _previous_ticks = _host_api->get_meter_ticks();
  *ticks          = std::bit_cast< std::int64_t >( _previous_ticks );

  return std::to_underlying( virtual_machine_errc::ok );
}

} // namespace koinos::vm
