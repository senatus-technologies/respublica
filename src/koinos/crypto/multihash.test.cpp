#include <gtest/gtest.h>
#include <koinos/bigint.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/hex.hpp>

template< typename Blob >
std::string hex_string( const Blob& b )
{
  std::stringstream ss;
  ss << std::hex;

  for( int i = 0; i < b.size(); ++i )
    ss << std::setw( 2 ) << std::setfill( '0' ) << (int)b[ i ];

  return ss.str();
}

TEST( multihash, keccak_256_test )
{
  auto eh = koinos::crypto::multihash::empty( koinos::crypto::multicodec::keccak_256 );
  EXPECT_EQ( koinos::util::to_hex( eh.digest() ),
             "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470" );

  auto h1 = koinos::crypto::hash(
    koinos::crypto::multicodec::keccak_256,
    koinos::util::from_hex< std::string >(
      "0x20ff454369a5d05b81a78f3db05819fea9b08c2384f75cb0ab6aa115dd690da3131874a1ca8f708ad1519ea952c1e249cb540d196392c79e87755424fee7c890808c562722359eea52e8a12fbbb969dd7961d2ba52037493755a5fa04f0d50a1aa26c9b44148c0d3b94d1c4a59a31aca15ae8bd44acb7833d8e91c4b86fa3135a423387b8151b4133ed23f6d7187b50ec2204ad901ad74d396e44274e0ecafaae17b3b9085e22260b35ca53b15cc52abba758af6798fbd04eceeced648f3af4fdb3ded7557a9a5cfb7382612a8a8f3f45947d1a29ce29072928ec193ca25d51071bd5e1984ecf402f306ea762f0f25282f5296d997658be3f983696ffa6d095c6369b4daf79e9a5d3136229128f8eb63c12b9e9fa78aff7a3e9e19a62022493cd136defbb5bb7ba1b938f367fd2f63eb5ca76c0b0ff21b9e36c3f07230cf3c3074e5da587040a76975d7e39f4494ace5486fcbf380ab7558c4fe89656335b82e4db8659509eab46a19613126e594042732dd4c411f41aa8cdeac71c0fb40a94e6da558c05e77b6182806f26d9afdf3da00c69419222c8186a6efad600b410e6ce2f2a797e49dc1f135319801fa6f396b06f975e2a190a023e474b618e7" ) );
  EXPECT_EQ( koinos::util::to_hex( h1.digest() ),
             "0x0ec8d9d20ddf0a7b0251e941a7261b557507ff6287b504362a8f1734c5a91012" );

  auto h2 = koinos::crypto::hash(
    koinos::crypto::multicodec::keccak_256,
    koinos::util::from_hex< std::string >(
      "0x4fbdc596508d24a2a0010e140980b809fb9c6d55ec75125891dd985d37665bd80f9beb6a50207588abf3ceee8c77cd8a5ad48a9e0aa074ed388738362496d2fb2c87543bb3349ea64997ce3e7b424ea92d122f57dbb0855a803058437fe08afb0c8b5e7179b9044bbf4d81a7163b3139e30888b536b0f957eff99a7162f4ca5aa756a4a982dfadbf31ef255083c4b5c6c1b99a107d7d3afffdb89147c2cc4c9a2643f478e5e2d393aea37b4c7cb4b5e97dadcf16b6b50aae0f3b549ece47746db6ce6f67dd4406cd4e75595d5103d13f9dfa79372924d328f8dd1fcbeb5a8e2e8bf4c76de08e3fc46aa021f989c49329c7acac5a688556d7bcbcb2a5d4be69d3284e9c40ec4838ee8592120ce20a0b635ecadaa84fd5690509f54f77e35a417c584648bc9839b974e07bfab0038e90295d0b13902530a830d1c2bdd53f1f9c9faed43ca4eed0a8dd761bc7edbdda28a287c60cd42af5f9c758e5c7250231c09a582563689afc65e2b79a7a2b68200667752e9101746f03184e2399e4ed8835cb8e9ae90e296af220ae234259fe0bd0bcc60f7a4a5ff3f70c5ed4de9c8c519a10e962f673c82c5e9351786a8a3bfd570031857bd4c87f4fca31ed4d50e14f2107da02cb5058700b74ea241a8b41d78461658f1b2b90bfd84a4c2c9d6543861ab3c56451757dcfb9ba60333488dbdd02d601b41aae317ca7474eb6e6dd" ) );
  EXPECT_EQ( koinos::util::to_hex( h2.digest() ),
             "0x0ea33e2e34f572440640244c7f1f5f04697ce97139bda72a6558d8663c02b388" );
}

TEST( multihash, comparison )
{
  auto sha256_hash = koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 );
  auto sha1_hash   = hash( koinos::crypto::multicodec::sha1, sha256_hash );
  auto sha512_hash = hash( koinos::crypto::multicodec::sha2_512, sha1_hash );

  EXPECT_TRUE( sha1_hash < sha256_hash );
  EXPECT_TRUE( !( sha1_hash > sha256_hash ) );
  EXPECT_TRUE( !( sha256_hash < sha1_hash ) );
  EXPECT_TRUE( sha256_hash > sha1_hash );

  EXPECT_TRUE( sha1_hash < sha512_hash );
  EXPECT_TRUE( !( sha1_hash > sha512_hash ) );
  EXPECT_TRUE( !( sha512_hash < sha1_hash ) );
  EXPECT_TRUE( sha512_hash > sha1_hash );

  EXPECT_TRUE( sha256_hash < sha512_hash );
  EXPECT_TRUE( !( sha256_hash > sha512_hash ) );
  EXPECT_TRUE( !( sha512_hash < sha256_hash ) );
  EXPECT_TRUE( sha512_hash > sha256_hash );

  auto sha1_hash2   = hash( koinos::crypto::multicodec::sha1, sha1_hash );
  auto sha256_hash2 = hash( koinos::crypto::multicodec::sha2_256, sha256_hash );
  auto sha512_hash2 = hash( koinos::crypto::multicodec::sha2_512, sha512_hash );

  EXPECT_TRUE( sha1_hash < sha1_hash2 );
  EXPECT_TRUE( !( sha1_hash > sha1_hash2 ) );
  EXPECT_TRUE( !( sha1_hash2 < sha1_hash ) );
  EXPECT_TRUE( sha1_hash2 > sha1_hash );

  EXPECT_TRUE( sha256_hash < sha256_hash2 );
  EXPECT_TRUE( !( sha256_hash > sha256_hash2 ) );
  EXPECT_TRUE( !( sha256_hash2 < sha256_hash ) );
  EXPECT_TRUE( sha256_hash2 > sha256_hash );

  EXPECT_TRUE( sha512_hash > sha512_hash2 );
  EXPECT_TRUE( !( sha512_hash < sha512_hash2 ) );
  EXPECT_TRUE( !( sha512_hash2 > sha512_hash ) );
  EXPECT_TRUE( sha512_hash2 < sha512_hash );
}

TEST( multihash, zero )
{
  koinos::crypto::multihash mh;
  mh = koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 );
  EXPECT_EQ( mh.code(), koinos::crypto::multicodec::sha2_256 );
  EXPECT_EQ( mh.digest().size(), 256 / 8 );

  mh = koinos::crypto::multihash::zero( koinos::crypto::multicodec::ripemd_160 );
  EXPECT_EQ( mh.code(), koinos::crypto::multicodec::ripemd_160 );
  EXPECT_EQ( mh.digest().size(), 160 / 8 );
}

TEST( multihash, empty )
{
  koinos::crypto::multihash mh = koinos::crypto::multihash::empty( koinos::crypto::multicodec::sha2_256 );
  EXPECT_EQ( "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", hex_string( mh.digest() ) );
}

TEST( multihash, protobuf )
{
  std::string id_str       = "id";
  std::string previous_str = "previous";

  koinos::block_topology block_topology;
  block_topology.set_height( 100 );
  block_topology.set_id(
    koinos::util::converter::as< std::string >( hash( koinos::crypto::multicodec::sha1, id_str ) ) );
  block_topology.set_previous(
    koinos::util::converter::as< std::string >( hash( koinos::crypto::multicodec::sha2_512, previous_str ) ) );

  auto mhash = hash( koinos::crypto::multicodec::sha2_256, block_topology );

  std::stringstream stream;
  block_topology.SerializeToOstream( &stream );
  std::string str = stream.str();

  std::vector< std::byte > bytes( str.size() );
  std::transform( str.begin(),
                  str.end(),
                  bytes.begin(),
                  []( char c )
                  {
                    return std::byte( c );
                  } );

  EXPECT_EQ( hash( koinos::crypto::multicodec::sha2_256, bytes ), mhash );

  auto id_hash = koinos::util::converter::to< koinos::crypto::multihash >( block_topology.id() );
  EXPECT_EQ( id_hash, hash( koinos::crypto::multicodec::sha1, id_str ) );

  auto previous_hash = koinos::util::converter::to< koinos::crypto::multihash >( block_topology.previous() );
  EXPECT_EQ( previous_hash, hash( koinos::crypto::multicodec::sha2_512, previous_str ) );

  auto mhash2 = hash( koinos::crypto::multicodec::sha2_256, &block_topology );
  EXPECT_EQ( mhash, mhash2 );
}

TEST( multihash, serialization )
{
  auto mhash =
    hash( koinos::crypto::multicodec::ripemd_160, std::string( "a quick brown fox jumps over the lazy dog" ) );

  std::stringstream stream;
  koinos::to_binary( stream, mhash );

  koinos::crypto::multihash tmp;
  koinos::from_binary( stream, tmp );
  EXPECT_EQ( mhash, tmp );

  std::stringstream ss;
  ss << mhash;
  EXPECT_EQ( ss.str(), "0xd3201409c999f213afff19793d8288023c512f71873deb" );

  try
  {
    KOINOS_THROW( koinos::exception, "test multihash in exception: ${mh}", ( "mh", mhash ) );
    EXPECT_TRUE( false );
  }
  catch( const koinos::exception& e )
  {
    EXPECT_EQ( e.what(),
               std::string( "test multihash in exception: 0xd3201409c999f213afff19793d8288023c512f71873deb" ) );
  }
}

TEST( multihash, variadic )
{
  std::string id_str       = "id";
  std::string previous_str = "previous";

  koinos::block_topology block_topology;
  block_topology.set_height( 100 );
  block_topology.set_id(
    koinos::util::converter::as< std::string >( hash( koinos::crypto::multicodec::sha1, id_str ) ) );
  block_topology.set_previous(
    koinos::util::converter::as< std::string >( hash( koinos::crypto::multicodec::sha2_512, previous_str ) ) );

  std::stringstream ss;
  block_topology.SerializeToOstream( &ss );
  ss << "a quick brown fox jumps over the lazy dog";

  koinos::uint256_t x = 0;
  koinos::to_binary( ss, x );

  auto mhash1 = hash( koinos::crypto::multicodec::ripemd_160, ss.str() );
  auto mhash2 = hash( koinos::crypto::multicodec::ripemd_160,
                      block_topology,
                      std::string( "a quick brown fox jumps over the lazy dog" ),
                      x );

  EXPECT_EQ( mhash1, mhash2 );
}
