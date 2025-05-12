#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/secret_key.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/hex.hpp>

#include <koinos/tests/contracts.hpp>
#include <koinos/tests/util.hpp>

#include <koinos/chain/chain.pb.h>

#include <filesystem>
#include <optional>

template< typename T >
concept Operation = std::same_as< koinos::protocol::operation, T >;

template< typename T >
concept Transaction = std::same_as< koinos::protocol::transaction, T >;

namespace koinos::tests {

enum token_entry : uint32_t
{
  name         = 0x82a3537f,
  symbol       = 0xb76a7ca1,
  decimals     = 0xee80fd2f,
  total_supply = 0xb0da3934,
  balance_of   = 0x5c721497,
  transfer     = 0x27f576ca,
  mint         = 0xdc6f17bb,
  burn         = 0x859facc5,
};

struct fixture
{
  fixture( const std::string& name, const std::string& log_level );
  ~fixture();

  void set_block_merkle_roots( protocol::block& block,
                               crypto::multicodec code,
                               crypto::digest_size size = crypto::digest_size( 0 ) );
  void sign_block( protocol::block& block, crypto::secret_key& block_signing_key );
  void set_transaction_merkle_roots( protocol::transaction& transaction,
                                     crypto::multicodec code,
                                     crypto::digest_size size = crypto::digest_size( 0 ) );
  void add_signature( protocol::transaction& transaction, crypto::secret_key& transaction_signing_key );
  void sign_transaction( protocol::transaction& transaction, const crypto::secret_key& transaction_signing_key );

  protocol::operation make_upload_program_operation( const std::string& account, const std::string& bytecode );
  protocol::operation make_mint_operation( const std::string& address, const std::string& to, uint64_t amount );
  protocol::operation make_burn_operation( const std::string& address, const std::string& from, uint64_t amount );
  protocol::operation make_transfer_operation( const std::string& address,
                                               const std::string& from,
                                               const std::string& to,
                                               uint64_t amount );

  template< Operation... Args >
  protocol::transaction
  make_transaction( const crypto::secret_key& signer, uint64_t nonce, uint64_t limit, Args... args )
  {
    protocol::transaction t;
    ( ( *t.add_operations() = std::forward< Args >( args ) ), ... );
    t.mutable_header()->set_rc_limit( limit );
    t.mutable_header()->set_chain_id( _controller->get_chain_id()->chain_id() );
    t.mutable_header()->set_nonce( nonce );
    set_transaction_merkle_roots( t, koinos::crypto::multicodec::sha2_256 );
    sign_transaction( t, signer );
    return t;
  }

  template< Transaction... Args >
  protocol::block make_block( const crypto::secret_key& signer, Args... args )
  {
    auto topology = _controller->get_head_info()->head_topology();
    return make_block( signer,
                       topology.height() + 1,
                       koinos::util::converter::to< crypto::multihash >( topology.id() ),
                       std::forward< Args >( args )... );
  }

  template< Transaction... Args >
  protocol::block
  make_block( const crypto::secret_key& signer, uint64_t height, const crypto::multihash& previous, Args... args )
  {
    protocol::block b;
    ( ( *b.add_transactions() = std::forward< Args >( args ) ), ... );
    b.mutable_header()->set_timestamp(
      std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() )
        .count() );
    b.mutable_header()->set_height( height );
    b.mutable_header()->set_previous( koinos::util::converter::as< std::string >( previous ) );
    b.mutable_header()->set_previous_state_merkle_root( _controller->get_head_info()->head_state_merkle_root() );
    set_block_merkle_roots( b, koinos::crypto::multicodec::sha2_256 );
    b.set_id( koinos::util::converter::as< std::string >(
      *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, b.header() ) ) );
    sign_block( b, *_block_signing_private_key );
    return b;
  }

  enum verification : uint64_t
  {
    none              = 0x00,
    processed         = 0x01,
    head              = 0x02,
    without_reversion = 0x04
  };

  bool verify( std::expected< koinos::rpc::chain::submit_block_response, error::error > response,
               uint64_t flags ) const;
  bool verify( std::expected< koinos::rpc::chain::submit_transaction_response, error::error > response,
               uint64_t flags ) const;

  koinos::rpc::chain::submit_transaction_request tx_req;
  std::unique_ptr< chain::controller > _controller;
  std::filesystem::path _state_dir;
  std::optional< crypto::secret_key > _block_signing_private_key;
  chain::genesis_data _genesis_data;

  std::map< std::string, uint64_t > _thunk_compute{
    {                        "apply_block",  16'465},
    {      "apply_call_contract_operation",     685},
    {    "apply_set_system_call_operation", 136'081},
    {"apply_set_system_contract_operation",   8'692},
    {                  "apply_transaction",  12'542},
    {    "apply_upload_contract_operation",   3'130},
    {                               "call",   3'573},
    {                    "check_authority",  12'653},
    {             "check_system_authority",  12'750},
    {                 "consume_account_rc",     735},
    {            "consume_block_resources",     753},
    {       "deserialize_message_per_byte",       1},
    {         "deserialize_multihash_base",     102},
    {     "deserialize_multihash_per_byte",     404},
    {                              "event",   1'222},
    {                 "event_per_impacted",     101},
    {                               "exit",  11'636},
    {                  "get_account_nonce",     821},
    {                     "get_account_rc",   1'072},
    {                      "get_arguments",     809},
    {                          "get_block",   1'134},
    {                    "get_block_field",   1'417},
    {                         "get_caller",     825},
    {                       "get_chain_id",   1'046},
    {                    "get_contract_id",     778},
    {                      "get_head_info",   2'099},
    {        "get_last_irreversible_block",     772},
    {                    "get_next_object",  11'181},
    {                         "get_object",   1'054},
    {                      "get_operation",   1'081},
    {                    "get_prev_object",  15'445},
    {                "get_resource_limits",   1'227},
    {                    "get_transaction",   1'584},
    {              "get_transaction_field",   1'530},
    {                               "hash",   1'570},
    {                    "keccak_256_base",   1'406},
    {                "keccak_256_per_byte",       1},
    {                                "log",     738},
    {      "object_serialization_per_byte",       1},
    {                "post_block_callback",     741},
    {          "post_transaction_callback",     721},
    {                 "pre_block_callback",     730},
    {           "pre_transaction_callback",     729},
    {            "process_block_signature",   4'499},
    {                         "put_object",   1'057},
    {                 "recover_public_key",  29'630},
    {                      "remove_object",     893},
    {                    "ripemd_160_base",   1'343},
    {                "ripemd_160_per_byte",       1},
    {                  "set_account_nonce",     749},
    {                          "sha1_base",   1'151},
    {                      "sha1_per_byte",       1},
    {                      "sha2_256_base",   1'385},
    {                  "sha2_256_per_byte",       1},
    {                      "sha2_512_base",   1'445},
    {                  "sha2_512_per_byte",       1},
    {               "verify_account_nonce",     822},
    {                 "verify_merkle_root",       1},
    {                   "verify_signature",     762},
    {                   "verify_vrf_proof", 144'067},
  };
};

} // namespace koinos::tests
