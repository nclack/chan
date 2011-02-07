#include "fifo.h"
#include <gtest/gtest.h>

class FifoTest:public ::testing::Test
{ protected:
  void fill()
  { while(FIFO_SUCCESS(Fifo_Push_Try(full,&buf,sz)));
  }
  virtual void SetUp()
  {
    empty = Fifo_Alloc(16,10);  // make these the same size 
    full  = Fifo_Alloc(16,10);  //   so <buf> can be reused
    buf   = Fifo_Alloc_Token_Buffer(empty);
    sz    = Fifo_Buffer_Size_Bytes(empty);
    fill();
  };
  virtual void TearDown()
  {
    Fifo_Free(empty);
    Fifo_Free(full );
    Fifo_Free_Token_Buffer(buf);
  };

  Fifo* empty;
  Fifo* full;
  void *buf;
  size_t sz;
};

TEST_F(FifoTest,BufferCount)
{ EXPECT_EQ(16,Fifo_Buffer_Count(empty));
  EXPECT_EQ(16,Fifo_Buffer_Count(full )); 
}

TEST_F(FifoTest,BufferSize)
{ EXPECT_EQ(10,Fifo_Buffer_Size_Bytes(empty));
  EXPECT_EQ(10,Fifo_Buffer_Size_Bytes(full )); 
}

TEST_F(FifoTest,AllocNonPow2)
{ EXPECT_TRUE(Fifo_Alloc(10,10)==(void*)0);
}

TEST_F(FifoTest,InitiallyEmpty)
{ EXPECT_TRUE(Fifo_Is_Empty(empty));
}

TEST_F(FifoTest,InitiallyNotFull)
{ EXPECT_FALSE(Fifo_Is_Full(empty));
}

TEST_F(FifoTest,Full)
{ EXPECT_TRUE(Fifo_Is_Full(full));
  EXPECT_FALSE(Fifo_Is_Empty(full));
}

TEST_F(FifoTest,Drain)
{ while(FIFO_SUCCESS(Fifo_Pop(full,&buf,sz)))
  { EXPECT_FALSE(Fifo_Is_Full(full)); 
  }
  EXPECT_FALSE(Fifo_Is_Full(full)); 
  EXPECT_TRUE(Fifo_Is_Empty(full)); 
}

TEST_F(FifoTest,Fill)
{ 
  while(FIFO_SUCCESS(Fifo_Push_Try(empty,&buf,sz)))
  { EXPECT_FALSE(Fifo_Is_Empty(empty)); 
  }
  EXPECT_TRUE(Fifo_Is_Full(empty)); 
  EXPECT_FALSE(Fifo_Is_Empty(empty)); 
}

TEST_F(FifoTest,PopEmpty)
{ EXPECT_FALSE(FIFO_SUCCESS(Fifo_Pop(empty,&buf,sz)));
}

TEST_F(FifoTest,PushFull)
{ EXPECT_FALSE(FIFO_SUCCESS(Fifo_Push_Try(full,&buf,sz)));
  EXPECT_FALSE(FIFO_SUCCESS(Fifo_Push(full,&buf,sz,0)));  //no expand - fails
  EXPECT_TRUE (FIFO_SUCCESS(Fifo_Push(full,&buf,sz,1)));  //   expand - allocates new memory
}

TEST_F(FifoTest,PeekEmpty)
{ EXPECT_FALSE(FIFO_SUCCESS(Fifo_Peek(empty,&buf,sz))); 
  EXPECT_FALSE(FIFO_SUCCESS(Fifo_Peek_At(empty,&buf,sz,0)));  
}

TEST_F(FifoTest,PeekFull)
{ EXPECT_TRUE(FIFO_SUCCESS(Fifo_Peek(full,&buf,sz))); 
  EXPECT_TRUE(FIFO_SUCCESS(Fifo_Peek_At(full,&buf,sz,0)));  
}


