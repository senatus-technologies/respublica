#pragma once

#include <koinos/state_db/backends/iterator.hpp>
#include <koinos/state_db/backends/map/types.hpp>

#include <map>
#include <vector>

namespace koinos::state_db::backends::map {

class map_backend;

class map_iterator final: public abstract_iterator
{
public:
  map_iterator( std::unique_ptr< iterator_type > itr, map_type& map );
  ~map_iterator();

  value_type operator*() const override;

  const std::vector< std::byte >& key() const override;

  std::pair< std::vector< std::byte >, std::vector< std::byte > > release() override;

  abstract_iterator& operator++() override;
  abstract_iterator& operator--() override;

private:
  bool valid() const override;
  std::unique_ptr< abstract_iterator > copy() const override;

  std::unique_ptr< iterator_type > _itr;
  map_type& _map;
};

} // namespace koinos::state_db::backends::map
