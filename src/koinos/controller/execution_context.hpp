#pragma once

#include <koinos/controller/call_stack.hpp>
#include <koinos/controller/chronicler.hpp>
#include <koinos/controller/error.hpp>
#include <koinos/controller/resource_meter.hpp>
#include <koinos/controller/session.hpp>
#include <koinos/controller/state.hpp>
#include <koinos/crypto.hpp>
#include <koinos/program.hpp>
#include <koinos/state_db.hpp>
#include <koinos/vm.hpp>

#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace koinos::controller {

using program_registry_map = std::map< 
  protocol::account_view,
  std::unique_ptr< program::program >, 
  decltype( []( protocol::account_view lhs, protocol::account_view rhs )
  {
    return std::ranges::lexicographical_compare( lhs, rhs );
  } ) >;

enum class tolerance : std::uint8_t
{
  relaxed,
  strict
};

enum class intent : std::uint8_t
{
  read_only,
  block_application,
  transaction_application,
  block_proposal
};

class execution_context final: public program::system_interface

{
public:
  execution_context()                           = delete;
  execution_context( const execution_context& ) = delete;
  execution_context( execution_context&& )      = delete;
  execution_context( const std::shared_ptr< vm::virtual_machine >&, intent i = intent::read_only );

  ~execution_context() final = default;

  execution_context& operator=( const execution_context& ) = delete;
  execution_context& operator=( execution_context&& )      = delete;

  void set_state_node( const state_db::state_node_ptr& );
  void clear_state_node();

  class resource_meter& resource_meter();
  class chronicler& chronicler();

  result< protocol::block_receipt > apply( const protocol::block& );
  result< protocol::transaction_receipt > apply( const protocol::transaction& );

  std::span< const std::string > arguments() final;

  std::error_code write( program::file_descriptor fd, std::span< const std::byte > bytes ) final;
  std::error_code read( program::file_descriptor fd, std::span< std::byte > buffer ) final;

  std::span< const std::byte > get_object( std::uint32_t id, std::span< const std::byte > key ) final;

  std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_next_object( std::uint32_t id, std::span< const std::byte > key ) final;

  std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_prev_object( std::uint32_t id, std::span< const std::byte > key ) final;

  std::error_code
  put_object( std::uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value ) final;

  std::error_code remove_object( std::uint32_t id, std::span< const std::byte > key ) final;

  void log( std::span< const std::byte > message ) final;

  std::error_code event( std::span< const std::byte > name,
                         std::span< const std::byte > data,
                         const std::vector< std::span< const std::byte > >& impacted ) final;

  result< bool > check_authority( protocol::account_view account ) final;

  std::span< const std::byte > get_caller() final;

  result< protocol::program_output > call_program( protocol::account_view account,
                                                   std::span< const std::byte > stdin,
                                                   std::span< const std::string > arguments = {} ) final;

  std::uint64_t account_resources( protocol::account_view ) const;
  std::uint64_t account_nonce( protocol::account_view ) const;

  const crypto::digest& network_id() const noexcept;
  state::head head() const;
  const state::resource_limits& resource_limits() const;

  template< tolerance T >
  result< protocol::program_output > run_program( protocol::account_view account,
                                                  std::span< const std::byte > stdin,
                                                  std::span< const std::string > arguments = {} )
  {
    assert( _state_node );

    if( auto error = _stack.push_frame( { .program_id = account, .arguments = arguments, .stdin = stdin } ); error )
      return std::unexpected( error );

    frame_guard guard( _stack );

    std::error_code code;

    switch( account.type() )
    {
      case protocol::account_type::program:
        code = execute_user_program( account );

        if constexpr( T == tolerance::relaxed )
          if( code && code.category() != vm::program_category() )
            return std::unexpected( code );

        break;
      case protocol::account_type::native_program:
        code = execute_native_program( account );

        if constexpr( T == tolerance::relaxed )
          if( code && code.category() != program::program_category() )
            return std::unexpected( code );

        break;
      default:
        return std::unexpected( controller_errc::invalid_program );
    }

    if constexpr( T == tolerance::strict )
      if( code )
        return std::unexpected( code );

    auto frame = _stack.peek_frame();

    return protocol::program_output{ .code   = code.value(),
                                     .stdout = std::move( frame.stdout ),
                                     .stderr = std::move( frame.stderr ) };
  }

private:
  std::error_code apply( const protocol::upload_program& );
  std::error_code apply( const protocol::call_program& );
  std::error_code consume_account_resources( protocol::account_view account, std::uint64_t resources );
  std::error_code set_account_nonce( protocol::account_view account, std::uint64_t nonce );

  state_db::object_space create_object_space( std::uint32_t id );

  std::shared_ptr< session > make_session( std::uint64_t );

  std::error_code execute_user_program( protocol::account_view account ) noexcept;
  std::error_code execute_native_program( protocol::account_view account ) noexcept;

  std::shared_ptr< vm::virtual_machine > _vm;
  state_db::state_node_ptr _state_node;
  call_stack _stack;

  const protocol::block* _block             = nullptr;
  const protocol::transaction* _transaction = nullptr;
  const protocol::operation* _operation     = nullptr;

  class resource_meter _resource_meter;
  class chronicler _chronicler;
  intent _intent;

  std::vector< protocol::account_view > _verified_signatures;

  static const program_registry_map program_registry;
};

} // namespace koinos::controller
