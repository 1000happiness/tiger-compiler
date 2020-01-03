#ifndef TIGER_UTIL_UTIL_H_
#define TIGER_UTIL_UTIL_H_

#include <string>

namespace U {

class BoolList {
 public:
  bool head;
  BoolList* tail;

  BoolList(bool head, BoolList* tail) : head(head), tail(tail) {}
};

// std::string demicalToHex(int demical){
//   std::stringstream ss;
//   ss << std::hex << demical;
//   std::string hexString;
//   ss >> hexString; 
//   hexString = "0x" + hexString; 
//   return hexString;
// };

}  // namespace U

#endif  // TIGER_UTIL_UTIL_H_