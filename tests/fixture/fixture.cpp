#include <koinos/tests/fixture.hpp>

#include <ranges>

#include <koinos/chain/state.hpp>
#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/log/log.hpp>

using namespace std::string_literals;
using namespace std::chrono_literals;

namespace koinos::tests {

fixture::fixture( const std::string& name, const std::string& log_level )
{
  koinos::log::initialize( name, log_level );
  LOG( info ) << "Initializing fixture";

  _controller = std::make_unique< chain::controller >( 10'000'000 );
  auto seed   = "genesis"s;
  _block_signing_secret_key =
    *crypto::secret_key::create( *crypto::hash( koinos::crypto::multicodec::sha2_256, seed.c_str(), seed.size() ) );

  _state_dir = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
  LOG( info ) << "Using temporary directory: " << _state_dir.string();
  std::filesystem::create_directory( _state_dir );

  std::string genesis_key( reinterpret_cast< char* >( _block_signing_secret_key->public_key().bytes().data() ),
                           _block_signing_secret_key->public_key().bytes().size() );
  _genesis_data.emplace_back( chain::state::space::metadata(), chain::state::key::genesis_key, genesis_key );

  LOG( info ) << "Opening controller";
  _controller->open( _state_dir, _genesis_data, chain::fork_resolution_algorithm::fifo, false );
}

fixture::~fixture()
{
  boost::log::core::get()->remove_all_sinks();
  std::filesystem::remove_all( _state_dir );
}

void fixture::set_block_merkle_roots( protocol::block& block, crypto::multicodec code, crypto::digest_size size )
{
  std::vector< crypto::multihash > hashes;
  hashes.reserve( block.transactions.size() * 2 );

  for( const auto& trx: block.transactions )
  {
    hashes.emplace_back( *crypto::hash( code, trx.header, size ) );
    std::stringstream ss;
    for( const auto& sig: trx.signatures )
      ss.write( reinterpret_cast< const char* >( sig.signature.data() ), sig.signature.size() );

    if( auto hash = crypto::hash( crypto::multicodec::sha2_256, ss.str() ); hash )
      hashes.emplace_back( std::move( *hash ) );
  }

  auto transaction_merkle_tree = *crypto::merkle_tree< crypto::multihash >::create( code, hashes );

  assert( block.header.transaction_merkle_root.size() == transaction_merkle_tree.root()->hash().digest().size() );
  std::copy( transaction_merkle_tree.root()->hash().digest().begin(),
             transaction_merkle_tree.root()->hash().digest().end(),
             block.header.transaction_merkle_root.begin() );
}

void fixture::sign_block( protocol::block& block, crypto::secret_key& block_signing_key )
{
  block.header.signer = block_signing_key.public_key().bytes();
  auto id             = *crypto::hash( crypto::multicodec::sha2_256, block.header );

  assert( id.digest().size() == block.id.size() );
  std::copy( id.digest().begin(), id.digest().end(), block.id.begin() );

  block.signature = *block_signing_key.sign( id );
}

void fixture::set_transaction_merkle_roots( protocol::transaction& transaction,
                                            crypto::multicodec code,
                                            crypto::digest_size size )
{
  std::vector< crypto::multihash > operations;
  operations.reserve( transaction.operations.size() );

  for( const auto& op: transaction.operations )
    operations.emplace_back( *crypto::hash( code, op, size ) );

  auto operation_merkle_tree = *crypto::merkle_tree< crypto::multihash >::create( code, operations );

  assert( transaction.header.operation_merkle_root.size() == operation_merkle_tree.root()->hash().digest().size() );
  std::copy( operation_merkle_tree.root()->hash().digest().begin(),
             operation_merkle_tree.root()->hash().digest().end(),
             transaction.header.operation_merkle_root.begin() );
}

void fixture::add_signature( protocol::transaction& transaction, crypto::secret_key& transaction_signing_key )
{
  auto id_mh =
    crypto::multihash( crypto::multicodec::sha2_256, std::vector( transaction.id.begin(), transaction.id.end() ) );

  koinos::protocol::transaction_signature sig;
  sig.signer    = transaction_signing_key.public_key().bytes();
  sig.signature = *transaction_signing_key.sign( id_mh );

  transaction.signatures.emplace_back( std::move( sig ) );
}

void fixture::sign_transaction( protocol::transaction& transaction, const crypto::secret_key& transaction_signing_key )
{
  auto account             = transaction_signing_key.public_key().bytes();
  transaction.header.payer = account;
  auto id_mh               = *crypto::hash( crypto::multicodec::sha2_256, transaction.header );

  assert( id_mh.digest().size() == transaction.id.size() );
  std::copy( id_mh.digest().begin(), id_mh.digest().end(), transaction.id.begin() );

  transaction.signatures.clear();
  koinos::protocol::transaction_signature sig;
  sig.signer    = transaction_signing_key.public_key().bytes();
  sig.signature = *transaction_signing_key.sign( id_mh );

  transaction.signatures.emplace_back( std::move( sig ) );
}

protocol::upload_program fixture::make_upload_program_operation( const protocol::account& account,
                                                                 const std::vector< std::byte >& bytecode )
{
  protocol::upload_program op;
  op.id       = account;
  op.bytecode = bytecode;
  return op;
}

protocol::call_program
fixture::make_mint_operation( const protocol::account& id, const protocol::account& to, uint64_t amount )
{
  protocol::call_program op;
  op.id          = id;
  op.entry_point = koinos::tests::token_entry::mint;
  op.arguments.emplace_back( std::ranges::to< std::vector< std::byte > >( to ) );
  op.arguments.emplace_back(
    koinos::util::converter::as< std::vector< std::byte > >( boost::endian::native_to_little( uint64_t( amount ) ) ) );
  return op;
}

protocol::call_program
fixture::make_burn_operation( const protocol::account& id, const protocol::account& from, uint64_t amount )
{
  protocol::call_program op;
  op.id          = id;
  op.entry_point = koinos::tests::token_entry::burn;
  op.arguments.emplace_back( std::ranges::to< std::vector< std::byte > >( from ) );
  op.arguments.emplace_back(
    koinos::util::converter::as< std::vector< std::byte > >( boost::endian::native_to_little( uint64_t( amount ) ) ) );
  return op;
}

protocol::call_program fixture::make_transfer_operation( const protocol::account& id,
                                                         const protocol::account& from,
                                                         const protocol::account& to,
                                                         uint64_t amount )
{
  protocol::call_program op;
  op.id          = id;
  op.entry_point = koinos::tests::token_entry::transfer;
  op.arguments.emplace_back( std::ranges::to< std::vector< std::byte > >( from ) );
  op.arguments.emplace_back( std::ranges::to< std::vector< std::byte > >( to ) );
  op.arguments.emplace_back(
    koinos::util::converter::as< std::vector< std::byte > >( boost::endian::native_to_little( uint64_t( amount ) ) ) );
  return op;
}

bool fixture::verify( std::expected< protocol::block_receipt, error::error > receipt, uint64_t flags ) const
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
        return false;
      }
    }
  }

  return true;
}

bool fixture::verify( std::expected< protocol::transaction_receipt, error::error > receipt, uint64_t flags ) const
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
      return false;
    }
  }

  return true;
}

} // namespace koinos::tests
