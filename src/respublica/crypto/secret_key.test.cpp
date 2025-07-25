// NOLINTBEGIN

#include <gtest/gtest.h>
#include <respublica/crypto/hash.hpp>
#include <respublica/crypto/secret_key.hpp>
#include <respublica/encode.hpp>

using namespace std::string_literals;

TEST( secret_key, sign )
{
  auto alice_hash = respublica::crypto::hash( "alice" );

  auto skey = respublica::crypto::secret_key::create( alice_hash );

  auto data = respublica::crypto::hash( "carpe diem" );

  auto signed_data = skey.sign( data );

#ifdef FAST_CRYPTO
  auto signature_data = *respublica::encode::from_base58(
    "4ec2BUf5BvE12UnjVCXoycPSsYuhwonYLpYPv46ntUHckAmqxgugDsHCneWqu1pEqfM4jTLV8B8eN1DD3FVUkRhh" );
#else
  auto signature_data = *respublica::encode::from_base58(
    "3vn9RyuDw9CRhr82sFKyrNkpFk7SM519AGB1iJYLatCyVc2k6rJ6K6cumCyrqm7WkcTbbJYNhJxuRSF3fUJoHGcx" );
#endif

  EXPECT_TRUE( std::ranges::equal( signature_data, signed_data ) );
}

TEST( secret_key, comparison )
{
  auto alice_hash = respublica::crypto::hash( "alice" );
  auto bob_hash   = respublica::crypto::hash( "bob" );

  auto skey1 = respublica::crypto::secret_key::create( alice_hash );
  auto skey2 = respublica::crypto::secret_key::create( bob_hash );

  EXPECT_NE( skey1, skey2 );
  EXPECT_EQ( skey1, skey1 );

  auto skey1_bytes_a = skey1.bytes();
  auto skey1_bytes_b = skey1.bytes();
  auto skey2_bytes   = skey2.bytes();

  EXPECT_NE( skey1_bytes_a, skey2_bytes );
  EXPECT_EQ( skey1_bytes_a, skey1_bytes_b );
}

TEST( secret_key, determinism )
{
  auto alice_hash1 = respublica::crypto::hash( "alice" );
  auto alice_hash2 = respublica::crypto::hash( "alice" );
  auto bob_hash    = respublica::crypto::hash( "bob" );

  auto skey1 = respublica::crypto::secret_key::create( alice_hash1 );
  auto skey2 = respublica::crypto::secret_key::create( alice_hash2 );
  auto skey3 = respublica::crypto::secret_key::create( bob_hash );
  EXPECT_EQ( skey1, skey2 );
  EXPECT_NE( skey1, skey3 );

  EXPECT_EQ( skey1.public_key(), skey2.public_key() );
  EXPECT_NE( skey1.public_key(), skey3.public_key() );
}

TEST( private_key, nondeterminism )
{
  auto skey1 = respublica::crypto::secret_key::create();

  auto data1 = respublica::crypto::hash( "carpe diem" );

  auto signed_data1 = skey1.sign( data1 );

  auto pkey1 = skey1.public_key();
  EXPECT_TRUE( pkey1.verify( signed_data1, data1 ) );

  auto skey2 = respublica::crypto::secret_key::create();

  auto data2 = respublica::crypto::hash( "carpe diem" );

  auto signed_data2 = skey2.sign( data2 );

  auto pkey2 = skey2.public_key();
  EXPECT_TRUE( pkey2.verify( signed_data2, data2 ) );

  EXPECT_NE( skey1, skey2 );
}

// NOLINTEND
