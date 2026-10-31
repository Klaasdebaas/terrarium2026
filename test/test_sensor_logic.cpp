#include "minitest.h"
#include "sensor_logic.h"
#include <math.h>

TEST(dht_valid)   { CHECK(validDht(25,80)); CHECK(!validDht(NAN,80)); CHECK(!validDht(25,101)); CHECK(!validDht(81,50)); }
TEST(ds_valid)    { CHECK(validDs(27.5)); CHECK(!validDs(-127)); CHECK(!validDs(85.0f)); CHECK(!validDs(NAN)); }
TEST(fault_initial_not_usable_not_faulty) { FaultCounter f; CHECK(!f.usable()); CHECK(!f.faulty()); }
TEST(fault_after_three) { FaultCounter f; f.record(true); f.record(false); f.record(false); CHECK(f.usable()); CHECK(!f.faulty());
                          f.record(false); CHECK(f.faulty()); CHECK(!f.usable()); f.record(true); CHECK(f.usable()); }
TEST(fault_never_valid) { FaultCounter f; f.record(false); f.record(false); f.record(false); CHECK(f.faulty()); CHECK(!f.usable()); }
