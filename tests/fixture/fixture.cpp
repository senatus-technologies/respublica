// NOLINTBEGIN

#include <test/fixture.hpp>

#include <boost/endian.hpp>

#include <respublica/controller.hpp>
#include <respublica/crypto.hpp>
#include <respublica/encode.hpp>
#include <respublica/log.hpp>
#include <respublica/protocol.hpp>

namespace test {

fixture::fixture( const std::string& name, const std::string& log_level )
{
  respublica::log::initialize();

  _controller               = std::make_unique< respublica::controller::controller >( 10'000'000 );
  _block_signing_secret_key = respublica::crypto::secret_key::create( respublica::crypto::hash( "genesis" ) );

  _state_dir = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
  LOG_INFO( respublica::log::instance(), "Using temporary directory: {}", _state_dir.string() );
  std::filesystem::create_directory( _state_dir );

  auto genesis_pub_key = _block_signing_secret_key.public_key();
  _genesis_data.emplace_back(
    respublica::controller::state::space::metadata(),
    std::vector< std::byte >( respublica::controller::state::key::genesis_key().begin(),
                              respublica::controller::state::key::genesis_key().end() ),
    std::vector< std::byte >( genesis_pub_key.bytes().begin(), genesis_pub_key.bytes().end() ) );

  _controller->open( _state_dir, _genesis_data, respublica::state_db::fork_resolution_algorithm::fifo, false );
}

fixture::~fixture()
{
  std::filesystem::remove_all( _state_dir );
}

respublica::protocol::operation fixture::make_upload_program_operation( const respublica::protocol::account& account,
                                                                        const std::vector< std::byte >& bytecode )
{
  respublica::protocol::upload_program op;
  op.id       = account;
  op.bytecode = bytecode;
  return op;
}

respublica::protocol::operation fixture::make_mint_operation( const respublica::protocol::account& id,
                                                              const respublica::protocol::account& to,
                                                              std::uint64_t amount )
{
  respublica::protocol::call_program op;
  op.id          = id;
  op.input.stdin = make_stdin( test::token::instruction::mint, to, amount );
  return op;
}

respublica::protocol::operation fixture::make_burn_operation( const respublica::protocol::account& id,
                                                              const respublica::protocol::account& from,
                                                              std::uint64_t amount )
{
  respublica::protocol::call_program op;
  op.id          = id;
  op.input.stdin = make_stdin( test::token::instruction::burn, from, amount );
  return op;
}

respublica::protocol::operation fixture::make_transfer_operation( const respublica::protocol::account& id,
                                                                  const respublica::protocol::account& from,
                                                                  const respublica::protocol::account& to,
                                                                  std::uint64_t amount )
{
  respublica::protocol::call_program op;
  op.id          = id;
  op.input.stdin = make_stdin( test::token::instruction::transfer, from, to, amount );
  return op;
}

bool fixture::verify( respublica::controller::result< respublica::protocol::block_receipt > receipt,
                      std::uint64_t flags ) const
{
  if( flags == verification::none )
    return true;

  if( !receipt.has_value() )
  {
    LOG_ERROR( respublica::log::instance(), "Block submission has failed with: {}", receipt.error().message() );
    return false;
  }

  if( flags & verification::head )
  {
    auto head = _controller->head();
    if( receipt->id != head.id )
    {
      LOG_ERROR( respublica::log::instance(),
                 "Block ID {} does not match head {}",
                 respublica::log::hex{ receipt->id.data(), receipt->id.size() },
                 respublica::log::hex{ head.id.data(), head.id.size() } );
      return false;
    }
  }

  if( flags & verification::without_reversion )
  {
    for( const auto& tx_receipt: receipt->transaction_receipts )
    {
      if( tx_receipt.reverted )
      {
        LOG_ERROR( respublica::log::instance(),
                   "Transaction ID {} was reverted",
                   respublica::log::hex{ tx_receipt.id.data(), tx_receipt.id.size() } );
        return false;
      }
    }
  }

  return true;
}

bool fixture::verify( respublica::controller::result< respublica::protocol::transaction_receipt > receipt,
                      std::uint64_t flags ) const
{
  if( flags == verification::none )
    return true;

  if( !receipt.has_value() )
  {
    LOG_ERROR( respublica::log::instance(), "Transaction submission has failed with: {}", receipt.error().message() );
    return false;
  }

  if( flags & verification::without_reversion )
  {
    if( receipt->reverted )
    {
      LOG_ERROR( respublica::log::instance(),
                 "Transaction ID {} was reverted",
                 respublica::log::hex{ receipt->id.data(), receipt->id.size() } );
      return false;
    }
  }

  return true;
}

respublica::protocol::program_input fixture::make_input( std::vector< std::byte >&& stdin,
                                                         std::vector< std::string >&& arguments ) const noexcept
{
  respublica::protocol::program_input input;
  input.stdin     = std::move( stdin );
  input.arguments = std::move( arguments );
  return input;
}

} // namespace test

// NOLINTEND
