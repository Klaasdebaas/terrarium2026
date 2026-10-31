#include "minitest.h"
#include "types.h"
#include "state.h"

TEST(smoke_types) {
  CHECK_EQ(RELAY_COUNT, 4);
  CHECK_EQ(ALARM_COUNT, 10);
}
