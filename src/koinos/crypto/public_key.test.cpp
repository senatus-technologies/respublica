#include <gtest/gtest.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/public_key.hpp>
#include <koinos/crypto/secret_key.hpp>
#include <koinos/util/base58.hpp>

using namespace std::string_literals;

TEST( public_key, verify )
{
  auto alice_hash = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "alice"s );
  EXPECT_TRUE( alice_hash.has_value() );

  auto skey = koinos::crypto::secret_key::create( *alice_hash );
  EXPECT_TRUE( skey.has_value() );

  auto pkey = skey->public_key();

  auto data = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "carpe diem"s );
  EXPECT_TRUE( data.has_value() );

  auto signature_data = koinos::util::from_base58< koinos::crypto::signature >(
    "CcqEY1RLVPUW7GvALLNdJXJnNFJ4uXJ2dXZhX5QmEsQKP1sNKi3pwCA85dp29q4pmcTvoWHfMXuwFQL3a2MuNpm" );

  EXPECT_TRUE( pkey.verify( signature_data, *data ) );
}

TEST( public_key, comparison )
{
  auto alice_hash = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "alice"s );
  EXPECT_TRUE( alice_hash.has_value() );
  auto bob_hash = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "bob"s );
  EXPECT_TRUE( bob_hash.has_value() );

  auto pkey1 = koinos::crypto::secret_key::create( *alice_hash )->public_key();
  auto pkey2 = koinos::crypto::secret_key::create( *bob_hash )->public_key();

  EXPECT_NE( pkey1, pkey2 );
  EXPECT_EQ( pkey1, pkey1 );

  auto pkey1_bytes_a = pkey1.bytes();
  auto pkey1_bytes_b = pkey1.bytes();
  auto pkey2_bytes   = pkey2.bytes();

  EXPECT_NE( pkey1_bytes_a, pkey2_bytes );
  EXPECT_EQ( pkey1_bytes_a, pkey1_bytes_b );
}

TEST( public_key, serialization )
{
  auto skey = koinos::crypto::secret_key::create();
  EXPECT_TRUE( skey.has_value() );

  auto pkey = skey->public_key();

  std::stringstream ss;
  koinos::to_binary( ss, pkey );
  koinos::crypto::public_key pkey2;
  koinos::from_binary( ss, pkey2 );
  EXPECT_EQ( pkey, pkey2 );
}
