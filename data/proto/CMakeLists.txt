add_library(proto)

target_sources(proto
  PRIVATE
    koinos/account_history/account_history.proto
    koinos/block_store/block_store.proto
    koinos/broadcast/broadcast.proto
    koinos/chain/authority.proto
    koinos/chain/chain.proto
    koinos/chain/object_spaces.proto
    koinos/chain/value.proto
    koinos/contract_meta_store/contract_meta_store.proto
    koinos/contracts/claim/claim.proto
    koinos/contracts/governance/governance.proto
    koinos/contracts/koin/koin.proto
    koinos/contracts/name_service/name_service.proto
    koinos/contracts/pob/pob.proto
    koinos/contracts/pow/pow.proto
    koinos/contracts/resources/resources.proto
    koinos/contracts/token/token.proto
    koinos/contracts/vhp/vhp.proto
    koinos/koindx_tracker/koindx_tracker.proto
    koinos/mempool/mempool.proto
    koinos/protocol/protocol.proto
    koinos/rpc/rpc.proto
    koinos/rpc/services.proto
    koinos/rpc/account_history/account_history_rpc.proto
    koinos/rpc/block_store/block_store_rpc.proto
    koinos/rpc/chain/chain_rpc.proto
    koinos/rpc/contract_meta_store/contract_meta_store_rpc.proto
    koinos/rpc/koindx_tracker/koindx_tracker_rpc.proto
    koinos/rpc/mempool/mempool_rpc.proto
    koinos/rpc/p2p/p2p_rpc.proto
    koinos/rpc/transaction_store/transaction_store_rpc.proto
    koinos/standards/kcs1.proto
    koinos/standards/kcs2.proto
    koinos/standards/kcs3.proto
    koinos/standards/kcs4.proto
    koinos/standards/kcs5.proto
    koinos/transaction_store/transaction_store.proto
    koinos/common.proto
    koinos/options.proto)

target_include_directories(proto
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/proto>)

target_link_libraries(proto
  PRIVATE
    protobuf::protobuf)

add_library(Koinos::proto ALIAS proto)

protobuf_generate(
  TARGET proto
  PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}/proto)

