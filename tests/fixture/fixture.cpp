#include "koinos/protocol/transaction.hpp"
#include <test/fixture.hpp>

#include <koinos/chain/state.hpp>
#include <koinos/crypto/crypto.hpp>
#include <koinos/log/log.hpp>

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

  std::string genesis_key( reinterpret_cast< const char* >( _block_signing_secret_key.public_key().bytes().data() ),
                           _block_signing_secret_key.public_key().bytes().size() );
  _genesis_data.emplace_back( koinos::chain::state::space::metadata(),
                              koinos::chain::state::key::genesis_key,
                              genesis_key );

  LOG( info ) << "Opening controller";
  _controller->open( _state_dir, _genesis_data, koinos::chain::fork_resolution_algorithm::fifo, false );
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
  op.arguments.emplace_back(
    koinos::util::converter::as< std::vector< std::byte > >( boost::endian::native_to_little( uint64_t( amount ) ) ) );
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
  op.arguments.emplace_back(
    koinos::util::converter::as< std::vector< std::byte > >( boost::endian::native_to_little( uint64_t( amount ) ) ) );
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
  op.arguments.emplace_back(
    koinos::util::converter::as< std::vector< std::byte > >( boost::endian::native_to_little( uint64_t( amount ) ) ) );
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
      LOG( error ) << "Block submission ID " << koinos::util::to_hex( receipt->id ) << " does not match head "
                   << koinos::util::to_hex( _controller->head().id );
      return false;
    }
  }

  if( flags & verification::without_reversion )
  {
    for( const auto& tx_receipt: receipt->transaction_receipts )
    {
      if( tx_receipt.reverted )
      {
        LOG( error ) << "Transaction with ID " << koinos::util::to_hex( tx_receipt.id ) << " was reverted";
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
      LOG( error ) << "Transaction with ID " << koinos::util::to_hex( receipt->id ) << " was reverted";
      for( const auto& message: receipt->logs )
        LOG( error ) << message;

      return false;
    }
  }

  return true;
}

} // namespace test
