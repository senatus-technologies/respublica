#include <gtest/gtest.h>
#include <koinos/crypto/hash.hpp>
#include <koinos/crypto/public_key.hpp>
#include <koinos/crypto/secret_key.hpp>
#include <koinos/util/base58.hpp>

using namespace std::string_literals;

TEST( public_key, verify )
{
  auto alice_hash = koinos::crypto::hash( "alice" );

  auto skey = koinos::crypto::secret_key::create( alice_hash );

  auto pkey = skey.public_key();

  auto data = koinos::crypto::hash( "carpe diem" );

#ifdef FAST_CRYPTO
  auto signature_data = koinos::util::from_base58< koinos::crypto::signature >(
    "4ec2BUf5BvE12UnjVCXoycPSsYuhwonYLpYPv46ntUHckAmqxgugDsHCneWqu1pEqfM4jTLV8B8eN1DD3FVUkRhh" );
#else
  auto signature_data = koinos::util::from_base58< koinos::crypto::signature >(
    "3vn9RyuDw9CRhr82sFKyrNkpFk7SM519AGB1iJYLatCyVc2k6rJ6K6cumCyrqm7WkcTbbJYNhJxuRSF3fUJoHGcx" );
#endif

  EXPECT_TRUE( pkey.verify( signature_data, data ) );
}

TEST( public_key, comparison )
{
  auto alice_hash = koinos::crypto::hash( "alice" );
  auto bob_hash   = koinos::crypto::hash( "bob" );

  auto pkey1 = koinos::crypto::secret_key::create( alice_hash ).public_key();
  auto pkey2 = koinos::crypto::secret_key::create( bob_hash ).public_key();

  EXPECT_NE( pkey1, pkey2 );
  EXPECT_EQ( pkey1, pkey1 );

  auto pkey1_bytes_a = pkey1.bytes();
  auto pkey1_bytes_b = pkey1.bytes();
  auto pkey2_bytes   = pkey2.bytes();

  EXPECT_NE( pkey1_bytes_a, pkey2_bytes );
  EXPECT_EQ( pkey1_bytes_a, pkey1_bytes_b );
}
