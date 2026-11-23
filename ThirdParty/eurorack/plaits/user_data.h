// Copyright 2021 Emilie Gillet. MIT License (see LICENSE in this folder).
// Modified for BizGroove: no flash memory on a computer, so no user data.
#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_
#include "stmlib/stmlib.h"
namespace plaits {
class UserData {
 public:
  enum { SIZE = 0x1000 };
  inline const uint8_t* ptr(int) const { return NULL; }
};
}  // namespace plaits
#endif
