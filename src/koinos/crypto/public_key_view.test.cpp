// NOLINTBEGIN

#include <gtest/gtest.h>
#include <koinos/crypto/hash.hpp>
#include <koinos/crypto/public_key.hpp>
#include <koinos/crypto/public_key_view.hpp>
#include <koinos/crypto/secret_key.hpp>
#include <koinos/encode.hpp>

using namespace std::string_literals;

TEST( public_key_view, verify )
{
  auto alice_hash = koinos::crypto::hash( "alice" );
  auto skey = koinos::crypto::secret_key::create( alice_hash );
  auto pkey = skey.public_key();
  auto pkey_view = koinos::crypto::public_key_view( std::span( pkey.bytes() ) );

  auto data = koinos::crypto::hash( "carpe diem" );

#ifdef FAST_CRYPTO
  auto signature_data = *koinos::encode::from_base58(
    "4ec2BUf5BvE12UnjVCXoycPSsYuhwonYLpYPv46ntUHckAmqxgugDsHCneWqu1pEqfM4jTLV8B8eN1DD3FVUkRhh" );
#else
  auto signature_data = *koinos::encode::from_base58(
    "3vn9RyuDw9CRhr82sFKyrNkpFk7SM519AGB1iJYLatCyVc2k6rJ6K6cumCyrqm7WkcTbbJYNhJxuRSF3fUJoHGcx" );
#endif

  koinos::crypto::signature signature;
  std::ranges::copy( signature_data, signature.begin() );

  EXPECT_TRUE( pkey_view.verify( signature, data ) );
}

TEST( public_key_view, comparison )
{
  auto alice_hash = koinos::crypto::hash( "alice" );
  auto bob_hash   = koinos::crypto::hash( "bob" );

  auto pkey1 = koinos::crypto::secret_key::create( alice_hash ).public_key();
  auto pkey2 = koinos::crypto::secret_key::create( bob_hash ).public_key();

  auto pkey1_view = koinos::crypto::public_key_view( pkey1.bytes() );
  auto pkey2_view = koinos::crypto::public_key_view( pkey2.bytes() );

  EXPECT_NE( pkey1_view, pkey2 );
  EXPECT_NE( pkey1_view, pkey2_view );
  EXPECT_EQ( pkey1_view, pkey1 );
  EXPECT_EQ( pkey1_view, pkey1_view );

  auto pkey1_bytes_a = pkey1.bytes();
  auto pkey1_bytes_b = pkey1.bytes();
  auto pkey2_bytes   = pkey2.bytes();

  auto pkey1_view_bytes_a = pkey1_view.bytes();
  auto pkey1_view_bytes_b = pkey1_view.bytes();
  auto pkey2_view_bytes   = pkey2_view.bytes();

  EXPECT_FALSE( std::ranges::equal( pkey1_view_bytes_a, pkey2_bytes ) );
  EXPECT_FALSE( std::ranges::equal( pkey1_view_bytes_a, pkey2_view_bytes ) );
  EXPECT_TRUE( std::ranges::equal( pkey1_view_bytes_a, pkey1_bytes_a ) );
  EXPECT_TRUE( std::ranges::equal( pkey1_view_bytes_a, pkey1_bytes_b ) );
  EXPECT_TRUE( std::ranges::equal( pkey1_view_bytes_a, pkey1_view_bytes_b ) );
}

// NOLINTEND
