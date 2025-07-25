// NOLINTBEGIN

#include <gtest/gtest.h>
#include <respublica/crypto/hash.hpp>
#include <respublica/crypto/public_key.hpp>
#include <respublica/crypto/secret_key.hpp>
#include <respublica/encode.hpp>

using namespace std::string_literals;

TEST( public_key, verify )
{
  auto alice_hash = respublica::crypto::hash( "alice" );

  auto skey = respublica::crypto::secret_key::create( alice_hash );

  auto pkey = skey.public_key();

  auto data = respublica::crypto::hash( "carpe diem" );

#ifdef FAST_CRYPTO
  auto signature_data = *respublica::encode::from_base58(
    "4ec2BUf5BvE12UnjVCXoycPSsYuhwonYLpYPv46ntUHckAmqxgugDsHCneWqu1pEqfM4jTLV8B8eN1DD3FVUkRhh" );
#else
  auto signature_data = *respublica::encode::from_base58(
    "3vn9RyuDw9CRhr82sFKyrNkpFk7SM519AGB1iJYLatCyVc2k6rJ6K6cumCyrqm7WkcTbbJYNhJxuRSF3fUJoHGcx" );
#endif

  respublica::crypto::signature signature;
  std::ranges::copy( signature_data, signature.begin() );

  EXPECT_TRUE( pkey.verify( signature, data ) );
}

TEST( public_key, comparison )
{
  auto skey1 = respublica::crypto::secret_key::create( respublica::crypto::hash( "alice" ) );
  auto skey2 = respublica::crypto::secret_key::create( respublica::crypto::hash( "bob" ) );

  auto pkey1 = skey1.public_key();
  auto pkey2 = skey2.public_key();

  EXPECT_NE( pkey1, pkey2 );
  EXPECT_EQ( pkey1, pkey1 );

  auto pkey1_bytes_a = pkey1.bytes();
  auto pkey1_bytes_b = pkey1.bytes();
  auto pkey2_bytes   = pkey2.bytes();

  EXPECT_FALSE( std::ranges::equal( pkey1_bytes_a, pkey2_bytes ) );
  EXPECT_TRUE( std::ranges::equal( pkey1_bytes_a, pkey1_bytes_b ) );
}

// NOLINTEND
