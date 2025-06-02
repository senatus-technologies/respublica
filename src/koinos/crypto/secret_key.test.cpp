#include <gtest/gtest.h>
#include <koinos/crypto/hash.hpp>
#include <koinos/crypto/secret_key.hpp>
#include <koinos/util/base58.hpp>

using namespace std::string_literals;

TEST( secret_key, sign )
{
  auto alice_hash = koinos::crypto::hash( "alice" );

  auto skey = koinos::crypto::secret_key::create( alice_hash );
  EXPECT_TRUE( skey.has_value() );

  auto data = koinos::crypto::hash( "carpe diem" );

  auto signed_data = skey->sign( data );
  EXPECT_TRUE( signed_data.has_value() );

  auto signature_data = koinos::util::from_base58< koinos::crypto::signature >(
    "CcqEY1RLVPUW7GvALLNdJXJnNFJ4uXJ2dXZhX5QmEsQKP1sNKi3pwCA85dp29q4pmcTvoWHfMXuwFQL3a2MuNpm" );

  EXPECT_EQ( signature_data, *signed_data );
}

TEST( secret_key, comparison )
{
  auto alice_hash = koinos::crypto::hash( "alice" );
  auto bob_hash   = koinos::crypto::hash( "bob" );

  auto skey1 = koinos::crypto::secret_key::create( alice_hash );
  EXPECT_TRUE( skey1.has_value() );
  auto skey2 = koinos::crypto::secret_key::create( bob_hash );
  EXPECT_TRUE( skey2.has_value() );

  EXPECT_NE( *skey1, *skey2 );
  EXPECT_EQ( *skey1, *skey1 );

  auto skey1_bytes_a = skey1->bytes();
  auto skey1_bytes_b = skey1->bytes();
  auto skey2_bytes   = skey2->bytes();

  EXPECT_NE( skey1_bytes_a, skey2_bytes );
  EXPECT_EQ( skey1_bytes_a, skey1_bytes_b );
}

TEST( secret_key, determinism )
{
  auto alice_hash1 = koinos::crypto::hash( "alice" );
  auto alice_hash2 = koinos::crypto::hash( "alice" );
  auto bob_hash    = koinos::crypto::hash( "bob" );

  auto skey1 = koinos::crypto::secret_key::create( alice_hash1 );
  auto skey2 = koinos::crypto::secret_key::create( alice_hash2 );
  auto skey3 = koinos::crypto::secret_key::create( bob_hash );
  EXPECT_EQ( *skey1, *skey2 );
  EXPECT_NE( *skey1, *skey3 );

  EXPECT_EQ( skey1->public_key(), skey2->public_key() );
  EXPECT_NE( skey1->public_key(), skey3->public_key() );
}

TEST( private_key, nondeterminism )
{
  auto skey1 = koinos::crypto::secret_key::create();
  EXPECT_TRUE( skey1.has_value() );

  auto data1 = koinos::crypto::hash( "carpe diem" );

  auto signed_data1 = skey1->sign( data1 );
  EXPECT_TRUE( signed_data1.has_value() );

  auto pkey1 = skey1->public_key();
  EXPECT_TRUE( pkey1.verify( *signed_data1, data1 ) );

  auto skey2 = koinos::crypto::secret_key::create();
  EXPECT_TRUE( skey2.has_value() );

  auto data2 = koinos::crypto::hash( "carpe diem" );

  auto signed_data2 = skey2->sign( data2 );
  EXPECT_TRUE( signed_data2.has_value() );

  auto pkey2 = skey2->public_key();
  EXPECT_TRUE( pkey2.verify( *signed_data2, data2 ) );

  EXPECT_NE( skey1, skey2 );
}
