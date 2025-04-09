#include <iostream>

namespace google::protobuf {
class Message;
} // namespace google::protobuf

namespace koinos {

std::string serialize_canonically( const google::protobuf::Message& );
void serialize_canonically( std::string&, const google::protobuf::Message& );
void serialize_canonically( std::ostream&, const google::protobuf::Message& );

} // namespace koinos
