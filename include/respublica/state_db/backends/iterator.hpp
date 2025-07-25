#pragma once

#include <respublica/state_db/types.hpp>

#include <memory>
#include <vector>

namespace respublica::state_db::backends {

class iterator;

class abstract_iterator
{
public:
  abstract_iterator()                                      = default;
  abstract_iterator( const abstract_iterator& )            = default;
  abstract_iterator( abstract_iterator&& )                 = delete;
  abstract_iterator& operator=( const abstract_iterator& ) = default;
  abstract_iterator& operator=( abstract_iterator&& )      = delete;
  virtual ~abstract_iterator()                             = default;

  virtual const std::pair< const std::vector< std::byte >, std::vector< std::byte > >& operator*() const = 0;

  virtual std::pair< std::vector< std::byte >, std::vector< std::byte > > release() = 0;

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
  iterator( iterator&& other ) noexcept;
  ~iterator() = default;

  const std::pair< const std::vector< std::byte >, std::vector< std::byte > >& operator*() const;
  const std::pair< const std::vector< std::byte >, std::vector< std::byte > >* operator->() const;

  std::pair< std::vector< std::byte >, std::vector< std::byte > > release();

  iterator& operator++();
  iterator& operator--();

  iterator& operator=( const iterator& other );
  iterator& operator=( iterator&& other ) noexcept;

  friend bool operator==( const iterator& x, const iterator& y );
  friend bool operator!=( const iterator& x, const iterator& y );

private:
  bool valid() const;

  std::unique_ptr< abstract_iterator > _itr;
};

} // namespace respublica::state_db::backends
