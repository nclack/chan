#include "fifo.h"
#include <gtest/gtest.h>

TEST(Fifo,Alloc)
{ Fifo* q = Fifo_Alloc(10,10);
  ASSERT_NE(q,(Fifo*)0);
  EXPECT_TRUE(Fifo_Is_Empty(q));
  EXPECT_FALSE(Fifo_Is_Full(q));
  Fifo_Free(q);
}
