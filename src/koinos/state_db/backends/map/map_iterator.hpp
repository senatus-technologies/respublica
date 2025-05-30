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
  map_iterator( std::unique_ptr< iterator_type > itr, const map_type& map );
  ~map_iterator();

  virtual value_type operator*() const override;

  virtual const std::vector< std::byte >& key() const override;

  virtual abstract_iterator& operator++() override;
  virtual abstract_iterator& operator--() override;

private:
  virtual bool valid() const override;
  virtual std::unique_ptr< abstract_iterator > copy() const override;

  std::unique_ptr< iterator_type > _itr;
  const map_type& _map;
};

} // namespace koinos::state_db::backends::map
