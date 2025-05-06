#include <gtest/gtest.h>
#include <koinos/log/log.hpp>
#include <koinos/tests/fixture.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>

class integration: public ::testing::Test
{
protected:
  std::unique_ptr< koinos::tests::fixture > fixture;
  std::string token_address;
  std::optional< koinos::crypto::secret_key > alice_private_key;
  std::optional< koinos::crypto::secret_key > bob_private_key;

  virtual void SetUp()
  {
    fixture = std::make_unique< koinos::tests::fixture >( "integration", "trace" );

    auto rc_limit1 = 10'000'000;
    auto rc_limit2 = 9'000'000;

    LOG( info ) << "Creating accounts";
    auto contract_private_key = *koinos::crypto::secret_key::create(
      *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "contract" ) ) );
    alice_private_key = *koinos::crypto::secret_key::create(
      *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "alice" ) ) );
    bob_private_key = *koinos::crypto::secret_key::create(
      *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "bob" ) ) );

    uint64_t nonce = 1;

    LOG( info ) << "Uploading token contract";
    koinos::protocol::transaction trx1;

    token_address = koinos::util::converter::as< std::string >( contract_private_key.public_key().bytes() );
    auto op1      = trx1.add_operations()->mutable_upload_contract();
    op1->set_contract_id( token_address );
    op1->set_bytecode( get_koin_wasm() );
    trx1.mutable_header()->set_rc_limit( rc_limit1 );
    trx1.mutable_header()->set_chain_id( fixture->_controller->get_chain_id()->chain_id() );
    trx1.mutable_header()->set_nonce( nonce );
    fixture->set_transaction_merkle_roots( trx1, koinos::crypto::multicodec::sha2_256 );
    fixture->sign_transaction( trx1, contract_private_key );
    fixture->add_signature( trx1, *fixture->_block_signing_private_key );

    LOG( info ) << "Minting tokens";
    koinos::protocol::transaction trx2;

    auto op2 = trx2.add_operations()->mutable_call_contract();
    op2->set_contract_id( op1->contract_id() );
    op2->set_entry_point( koinos::tests::token_entry::mint );
    // to
    *op2->add_args() = koinos::util::converter::as< std::string >( alice_private_key->public_key().bytes() );
    // amount
    *op2->add_args() = koinos::util::converter::as< std::string >( 100 );

    trx2.mutable_header()->set_rc_limit( rc_limit2 );
    trx2.mutable_header()->set_chain_id( fixture->_controller->get_chain_id()->chain_id() );
    trx2.mutable_header()->set_nonce( nonce );
    fixture->set_transaction_merkle_roots( trx2, koinos::crypto::multicodec::sha2_256 );
    fixture->sign_transaction( trx2, *alice_private_key );

    koinos::rpc::chain::submit_block_request block_req;

    auto duration = std::chrono::system_clock::now().time_since_epoch();
    block_req.mutable_block()->mutable_header()->set_timestamp(
      std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
    block_req.mutable_block()->mutable_header()->set_height( 1 );
    block_req.mutable_block()->mutable_header()->set_previous( koinos::util::converter::as< std::string >(
      *koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 ) ) );
    block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root(
      fixture->_controller->get_head_info()->head_state_merkle_root() );
    *block_req.mutable_block()->add_transactions() = trx1;
    *block_req.mutable_block()->add_transactions() = trx2;

    fixture->set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
    block_req.mutable_block()->set_id( koinos::util::converter::as< std::string >(
      *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
    fixture->sign_block( *block_req.mutable_block(), *fixture->_block_signing_private_key );

    auto response = fixture->_controller->submit_block( block_req );
    if( !response.has_value() )
      LOG( error ) << response.error().message();
  }

  virtual void TearDown()
  {
    fixture = nullptr;
  }

  koinos::rpc::chain::submit_transaction_request transfer_request()
  {
    LOG( info ) << "Building transfer request";
    koinos::rpc::chain::submit_transaction_request req;

    auto rc_limit = 8'000'000;

    koinos::protocol::transaction trx;

    auto op3 = trx.add_operations()->mutable_call_contract();
    op3->set_contract_id( token_address );
    op3->set_entry_point( koinos::tests::token_entry::transfer );
    // from
    *op3->add_args() = koinos::util::converter::as< std::string >( alice_private_key->public_key().bytes() );
    // to
    *op3->add_args() = koinos::util::converter::as< std::string >( bob_private_key->public_key().bytes() );
    // value
    *op3->add_args() = koinos::util::converter::as< std::string >( 0 );

    trx.mutable_header()->set_rc_limit( rc_limit );
    trx.mutable_header()->set_nonce( 2 );
    trx.mutable_header()->set_chain_id( fixture->_controller->get_chain_id()->chain_id() );
    fixture->set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
    fixture->sign_transaction( trx, *alice_private_key );

    *req.mutable_transaction() = trx;
    return req;
  }
};

TEST_F( integration, transfer )
{
  auto tx_req   = transfer_request();
  auto response = fixture->_controller->submit_transaction( tx_req );
  EXPECT_TRUE( response.has_value() );
  EXPECT_TRUE( !response.value().receipt().reverted() );
}
