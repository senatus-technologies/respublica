// NOLINTBEGIN

#include <test/fixture.hpp>

#include <boost/endian.hpp>

#include <koinos/chain/state.hpp>
#include <koinos/crypto/crypto.hpp>
#include <koinos/encode/encode.hpp>
#include <koinos/log/log.hpp>
#include <koinos/protocol/transaction.hpp>

namespace test {

fixture::fixture( const std::string& name, const std::string& log_level )
{
  koinos::log::initialize( name, log_level );
  LOG( info ) << "Initializing fixture";

  _controller               = std::make_unique< koinos::chain::controller >( 10'000'000 );
  _block_signing_secret_key = koinos::crypto::secret_key::create( koinos::crypto::hash( "genesis" ) );

  _state_dir = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
  LOG( info ) << "Using temporary directory: " << _state_dir.string();
  std::filesystem::create_directory( _state_dir );

  auto genesis_pub_key = _block_signing_secret_key.public_key();
  _genesis_data.emplace_back(
    koinos::chain::state::space::metadata(),
    std::vector< std::byte >( koinos::chain::state::key::genesis_key().begin(),
                              koinos::chain::state::key::genesis_key().end() ),
    std::vector< std::byte >( genesis_pub_key.bytes().begin(), genesis_pub_key.bytes().end() ) );

  LOG( info ) << "Opening controller";
  _controller->open( _state_dir, _genesis_data, koinos::state_db::fork_resolution_algorithm::fifo, false );
}

fixture::~fixture()
{
  boost::log::core::get()->remove_all_sinks();
  std::filesystem::remove_all( _state_dir );
}

koinos::protocol::operation fixture::make_upload_program_operation( const koinos::protocol::account& account,
                                                                    const std::vector< std::byte >& bytecode )
{
  koinos::protocol::upload_program op;
  op.id       = account;
  op.bytecode = bytecode;
  return op;
}

koinos::protocol::operation fixture::make_mint_operation( const koinos::protocol::account& id,
                                                          const koinos::protocol::account& to,
                                                          uint64_t amount )
{
  koinos::protocol::call_program op;
  op.id          = id;
  op.entry_point = test::token_entry::mint;
  op.arguments.emplace_back( to.begin(), to.end() );
  boost::endian::native_to_little_inplace( amount );
  auto amount_span = std::as_bytes( std::span( &amount, 1 ) );
  op.arguments.emplace_back( amount_span.begin(), amount_span.end() );
  return op;
}

koinos::protocol::operation fixture::make_burn_operation( const koinos::protocol::account& id,
                                                          const koinos::protocol::account& from,
                                                          uint64_t amount )
{
  koinos::protocol::call_program op;
  op.id          = id;
  op.entry_point = test::token_entry::burn;
  op.arguments.emplace_back( from.begin(), from.end() );
  boost::endian::native_to_little_inplace( amount );
  auto amount_span = std::as_bytes( std::span( &amount, 1 ) );
  op.arguments.emplace_back( amount_span.begin(), amount_span.end() );
  return op;
}

koinos::protocol::operation fixture::make_transfer_operation( const koinos::protocol::account& id,
                                                              const koinos::protocol::account& from,
                                                              const koinos::protocol::account& to,
                                                              uint64_t amount )
{
  koinos::protocol::call_program op;
  op.id          = id;
  op.entry_point = test::token_entry::transfer;
  op.arguments.emplace_back( from.begin(), from.end() );
  op.arguments.emplace_back( to.begin(), to.end() );
  boost::endian::native_to_little_inplace( amount );
  auto amount_span = std::as_bytes( std::span( &amount, 1 ) );
  op.arguments.emplace_back( amount_span.begin(), amount_span.end() );
  return op;
}

bool fixture::verify( std::expected< koinos::protocol::block_receipt, koinos::error::error > receipt,
                      uint64_t flags ) const
{
  if( flags == verification::none )
    return true;

  if( !receipt.has_value() )
  {
    LOG( error ) << "Block submission has failed with: " << receipt.error().message();
    return false;
  }

  if( flags & verification::head )
  {
    if( receipt->id != _controller->head().id )
    {
      LOG( error ) << "Block submission ID " << koinos::encode::to_hex( receipt->id ) << " does not match head "
                   << koinos::encode::to_hex( _controller->head().id );
      return false;
    }
  }

  if( flags & verification::without_reversion )
  {
    for( const auto& tx_receipt: receipt->transaction_receipts )
    {
      if( tx_receipt.reverted )
      {
        LOG( error ) << "Transaction with ID " << koinos::encode::to_hex( tx_receipt.id ) << " was reverted";
        for( const auto& message: tx_receipt.logs )
          LOG( error ) << message;
        return false;
      }
    }
  }

  return true;
}

bool fixture::verify( std::expected< koinos::protocol::transaction_receipt, koinos::error::error > receipt,
                      uint64_t flags ) const
{
  if( flags == verification::none )
    return true;

  if( !receipt.has_value() )
  {
    LOG( error ) << "Transaction submission has failed with: " << receipt.error().message();
    return false;
  }

  if( flags & verification::without_reversion )
  {
    if( receipt->reverted )
    {
      LOG( error ) << "Transaction with ID " << koinos::encode::to_hex( receipt->id ) << " was reverted";
      for( const auto& message: receipt->logs )
        LOG( error ) << message;

      return false;
    }
  }

  return true;
}

} // namespace test

// NOLINTEND
