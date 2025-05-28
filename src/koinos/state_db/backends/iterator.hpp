#pragma once

#include <koinos/state_db/types.hpp>

#include <memory>

namespace koinos::state_db::backends {

class iterator;

class abstract_iterator
{
public:
  virtual ~abstract_iterator() {};

  virtual value_type operator*() const = 0;

  virtual key_type key() const = 0;

  virtual abstract_iterator& operator++() = 0;
  virtual abstract_iterator& operator--() = 0;

private:
  friend class iterator;

  virtual bool valid() const                                = 0;
  virtual std::unique_ptr< abstract_iterator > copy() const = 0;
};

class iterator final
{
public:
  iterator( std::unique_ptr< abstract_iterator > );
  iterator( const iterator& other );
  iterator( iterator&& other );

  value_type operator*() const;

  key_type key() const;
  value_type value() const;

  iterator& operator++();
  iterator& operator--();

  iterator& operator=( iterator&& other );

  friend bool operator==( const iterator& x, const iterator& y );
  friend bool operator!=( const iterator& x, const iterator& y );

private:
  bool valid() const;

  std::unique_ptr< abstract_iterator > _itr;
};

} // namespace koinos::state_db::backends
