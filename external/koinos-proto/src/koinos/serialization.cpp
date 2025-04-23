#include <koinos/serialization.hpp>

#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>

#include <stdexcept>

namespace koinos {

bool has_map_field( const google::protobuf::Message& msg )
{
  auto desc = msg.GetDescriptor();

  for( int i = 1; i <= desc->field_count(); i++ )
  {
    auto fd = desc->FindFieldByNumber( i );
    if( fd != nullptr && fd->is_map() )
      return true;

    if( fd != nullptr && fd->type() == google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE )
    {
      auto refl = msg.GetReflection();

      if( fd->label() == google::protobuf::FieldDescriptor::Label::LABEL_REPEATED )
      {
        const auto sub_msg = refl->GetRepeatedFieldRef< google::protobuf::Message >( msg, fd );
        if( sub_msg.size() && has_map_field( *sub_msg.begin() ) )
          return true;
      }
      else
      {
        const auto& sub_msg = refl->GetMessage( msg, fd );
        if( has_map_field( sub_msg ) )
          return true;
      }
    }
  }

  return false;
}

std::string serialize_canonically( const google::protobuf::Message& msg )
{
  std::string str;

  serialize_canonically( str, msg );

  return std::move( str );
}

void serialize_canonically( std::string& str, const google::protobuf::Message& msg )
{
  if( has_map_field( msg ) )
    throw std::runtime_error( "protobuf message has map field" );

  msg.SerializeToString( &str );
}

void serialize_canonically( std::ostream& os, const google::protobuf::Message& msg )
{
  if( has_map_field( msg ) )
    throw std::runtime_error( "protobuf message has map field" );

  msg.SerializeToOstream( &os );
}

} // namespace koinos
