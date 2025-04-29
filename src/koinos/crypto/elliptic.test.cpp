#include <gtest/gtest.h>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/hex.hpp>

using namespace std::string_literals;

TEST( private_key, sign )
{
  auto skey = koinos::crypto::private_key::create();
  EXPECT_TRUE( skey.has_value() );

  auto data = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "thing"s );
  EXPECT_TRUE( data.has_value() );

  auto signed_data = skey->sign( *data );
  EXPECT_TRUE( signed_data.has_value() );

  auto pkey = skey->public_key();
  EXPECT_TRUE( pkey.verify( *signed_data, *data ) );
}

#if 0
TEST( elliptic, serialization )
{
  auto priv = koinos::crypto::private_key::regenerate(
    *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string{ "seed" } ) );
  ASSERT_TRUE( priv );

  auto pub = priv->get_public_key();
  ASSERT_TRUE( pub );

  /*auto compressed = pub->serialize();*/
  /*ASSERT_TRUE( compressed );*/

  auto pub2 = koinos::crypto::public_key::deserialize( *compressed );
  ASSERT_TRUE( pub2 );
  EXPECT_EQ( pub->to_address_bytes(), pub2->to_address_bytes() );

  std::stringstream ss;
  koinos::to_binary( ss, *pub );
  koinos::crypto::public_key pub3;
  koinos::from_binary( ss, pub3 );
  EXPECT_EQ( pub->to_address_bytes(), pub3.to_address_bytes() );
}

TEST( elliptic, ecc )
{
  koinos::crypto::private_key nullkey;
  std::string pass = "foobar";

  for( uint32_t i = 0; i < 100; ++i )
  {
    koinos::crypto::multihash h = *hash( koinos::crypto::multicodec::sha2_256, pass.c_str(), pass.size() );
    auto priv                   = koinos::crypto::private_key::regenerate( h );
    if( !priv )
    {
      FAIL();
      continue;
    }
    EXPECT_NE( nullkey, priv );

    auto pub = priv->get_public_key();
    if( !pub )
    {
      FAIL();
      continue;
    }

    pass    += "1";
    auto h2  = *hash( koinos::crypto::multicodec::sha2_256, pass.c_str(), pass.size() );
    EXPECT_TRUE( pub->add( h2 ) );
    EXPECT_TRUE( koinos::crypto::private_key::generate_from_seed( h, h2 ) );

    auto sig = priv->sign( h );
    if( !sig )
    {
      FAIL();
      continue;
    }

    auto recover = koinos::crypto::public_key::recover( *sig, h );
    if( !recover )
      FAIL();
    else
      EXPECT_EQ( *recover, *pub );
  }
}

TEST( elliptic, wif )
{
  std::string secret         = "foobar";
  std::string wif            = "5KJTiKfLEzvFuowRMJqDZnSExxxwspVni1G4RcggoPtDqP5XgM1";
  std::string compressed_wif = "L3n4uPNBvne4p6BCUdhpThYQe21wDJe4jz9U7eWAfn15e9tj2jAF";
  auto key1                  = koinos::crypto::private_key::regenerate(
    *hash( koinos::crypto::multicodec::sha2_256, secret.c_str(), secret.size() ) );
  ASSERT_TRUE( key1 );
  EXPECT_EQ( key1->to_wif(), compressed_wif );

  auto key2 = koinos::crypto::private_key::from_wif( wif );
  ASSERT_TRUE( key2 );
  EXPECT_EQ( *key1, *key2 );

  // Encoding:
  // Prefix Secret                                                           Checksum
  // 80     C3AB8FF13720E8AD9047DD39466B3C8974E592C2FA383D4A3960714CAEF0C4F2 C957BEB4

  // Wrong checksum, change last octal (4->3)
  wif       = "5KJTiKfLEzvFuowRMJqDZnSExxxwspVni1G4RcggoPtDqP5XgLz";
  auto priv = koinos::crypto::private_key::from_wif( wif );
  EXPECT_FALSE( priv );
  EXPECT_EQ( priv.error().value(), koinos::error::error_code::reversion );

  // Wrong seed, change first octal of secret (C->D)
  wif  = "5KRWQqW5riLTcB39nLw6K7iv2HWBMYvbP7Ch4kUgRd8kEvLH5jH";
  priv = koinos::crypto::private_key::from_wif( wif );
  EXPECT_FALSE( priv );
  EXPECT_EQ( priv.error().value(), koinos::error::error_code::reversion );

  // Wrong prefix, change first octal of prefix (8->7)
  wif  = "4nCYtcUpcC6dkge8r2uEJeqrK97TUZ1n7n8LXDgLtun1wRyxU2P";
  priv = koinos::crypto::private_key::from_wif( wif );
  EXPECT_FALSE( priv );
  EXPECT_EQ( priv.error().value(), koinos::error::error_code::reversion );
}

TEST( elliptic, address )
{
  std::string private_wif = "5J1F7GHadZG3sCCKHCwg8Jvys9xUbFsjLnGec4H125Ny1V9nR6V";
  auto priv_key           = koinos::crypto::private_key::from_wif( private_wif );
  ASSERT_TRUE( priv_key );

  auto pub_key = priv_key->get_public_key();
  ASSERT_TRUE( pub_key );

  auto address = pub_key->to_address_bytes();

  const unsigned char bytes[] = { 0x00, 0xf5, 0x4a, 0x58, 0x51, 0xe9, 0x37, 0x2b, 0x87, 0x81, 0x0a, 0x8e, 0x60,
                                  0xcd, 0xd2, 0xe7, 0xcf, 0xd8, 0x0b, 0x6e, 0x31, 0xc7, 0xf1, 0x8f, 0xe8 };
  std::span< const std::byte > address_bytes( reinterpret_cast< const std::byte* >( bytes ), sizeof( bytes ) );

  EXPECT_TRUE( std::equal( address.begin(), address.end(), address_bytes.begin(), address_bytes.end() ) );
}

TEST( elliptic, compressed_key )
{
  std::string uncompressed_wif = "5JtU2c2MHKb8xSeNvsZJpxZRXeRg6iq6uwc6EUtDA9zsWM6B4c5";
  auto priv_key                = koinos::crypto::private_key::from_wif( uncompressed_wif );
  ASSERT_TRUE( priv_key );

  std::string expected_wif = "L1xAJ5axX33g7iBynn9bggE7GGBuaFdK6g1t6W52fQiRvQi73evQ";
  EXPECT_EQ( priv_key->to_wif(), expected_wif );

  std::string expected_address = "13Sqw4TrwdZ8RZ9UVfqqA2i3mrbeumcWba";
  EXPECT_EQ( koinos::util::to_base58( priv_key->get_public_key()->to_address_bytes() ), expected_address );

  priv_key = koinos::crypto::private_key::from_wif( expected_wif );
  ASSERT_TRUE( priv_key );
  EXPECT_EQ( priv_key->to_wif(), expected_wif );
  EXPECT_EQ( koinos::util::to_base58( priv_key->get_public_key()->to_address_bytes() ), expected_address );
}

TEST( elliptic, vrf )
{
  std::string msg = "sample";
  auto expected_proof =
    "0x031f4dbca087a1972d04a07a779b7df1caa99e0f5db2aa21f3aecc4f9e10e85d08748c9fbe6b95d17359707bfb8e8ab0c93ba0c515333adcb8b64f372c535e115ccf66ebf5abe6fadb01b5efb37c0a0ec9";
  auto priv_key = koinos::crypto::private_key::regenerate(
    koinos::crypto::multihash( koinos::crypto::multicodec::sha2_256,
                               koinos::util::from_hex< koinos::crypto::digest_type >( std::string(
                                 "0xc9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721" ) ) ) );
  ASSERT_TRUE( priv_key );
  auto pub_key = priv_key->get_public_key();
  ASSERT_TRUE( pub_key );

  auto random_proof = priv_key->generate_random_proof( msg );
  ASSERT_TRUE( random_proof );

  auto& proof = random_proof->first;
  EXPECT_EQ( koinos::util::to_hex( proof ), expected_proof );

  for( int i = 0; i < proof.size(); i++ )
  {
    auto temp = proof.data()[ i ];
    if( temp == 0x00 )
      const_cast< char* >( proof.data() )[ i ] = 0x01;
    else
      const_cast< char* >( proof.data() )[ i ] = 0x00;

    auto mh = pub_key->verify_random_proof( msg, proof );
    if( mh )
      FAIL();
    else
      EXPECT_EQ( mh.error().value(), koinos::error::error_code::reversion );

    const_cast< char* >( proof.data() )[ i ] = temp;

    EXPECT_TRUE( pub_key->verify_random_proof( msg, proof ) );
  }

  for( int i = 0; i < 10; i++ )
  {
    auto priv_key = koinos::crypto::private_key::regenerate( *hash( koinos::crypto::multicodec::sha2_256, i ) );
    if( !priv_key )
    {
      FAIL();
      continue;
    }

    auto random_proof = priv_key->generate_random_proof( msg );
    if( !random_proof )
    {
      FAIL();
      continue;
    }

    auto& proof = random_proof->first;
    auto& hash1 = random_proof->second;
    auto hash2  = priv_key->get_public_key()->verify_random_proof( msg, proof );
    if( !hash2 )
      FAIL();
    else
      EXPECT_EQ( hash1, *hash2 );
  }

  for( int i = 0; i < 10; i++ )
  {
    auto msg          = "message" + std::to_string( i );
    auto random_proof = priv_key->generate_random_proof( msg );
    if( !random_proof )
    {
      FAIL();
      continue;
    }

    auto& proof = random_proof->first;
    auto& hash1 = random_proof->second;
    auto hash2  = priv_key->get_public_key()->verify_random_proof( msg, proof );
    if( !hash2 )
      FAIL();
    else
      EXPECT_EQ( hash1, *hash2 );
  }

  auto expected_hash = "0x612065e309e937ef46c2ef04d5886b9c6efd2991ac484ec64a9b014366fc5d81";

  pub_key = koinos::crypto::public_key::deserialize( koinos::util::from_hex< koinos::crypto::compressed_public_key >(
    "0x032c8c31fc9f990c6b55e3865a184a4ce50e09481f2eaeb3e60ec1cea13a6ae645" ) );
  ASSERT_TRUE( pub_key );

  proof = koinos::util::from_hex< std::string >(
    "0x031f4dbca087a1972d04a07a779b7df1caa99e0f5db2aa21f3aecc4f9e10e85d0814faa89697b482daa377fb6b4a8b0191a65d34a6d90a8a2461e5db9205d4cf0bb4b2c31b5ef6997a585a9f1a72517b6f" );
  auto hash = pub_key->verify_random_proof( msg, proof );
  if( !hash )
    FAIL();
  else
    EXPECT_EQ( koinos::util::to_hex( hash->digest() ), expected_hash );

  proof = koinos::util::from_hex< std::string >(
    "0x031f4dbca087a1972d04a07a779b7df1caa99e0f5db2aa21f3aecc4f9e10e85d0814faa89697b482daa377fb6b4a8b0191a65d34a6d90a8a2461e5db9205d4cf0bb4b2c31b5ef6997a585a9f1a72517b" );
  hash = pub_key->verify_random_proof( msg, proof );
  if( hash )
    FAIL();
  else
    EXPECT_EQ( hash.error().value(), koinos::error::error_code::reversion );

  proof = koinos::util::from_hex< std::string >(
    "0x031f4dbca087a1972d04a07a779b7df1caa99e0f5db2aa21f3aecc4f9e10e85d0814faa89697b482daa377fb6b4a8b0191a65d34a6d90a8a2461e5db9205d4cf0bb4b2c31b5ef6997a585a9f1a72517b6f00" );
  hash = pub_key->verify_random_proof( msg, proof );
  if( hash )
    FAIL();
  else
    EXPECT_EQ( hash.error().value(), koinos::error::error_code::reversion );
}

#endif
