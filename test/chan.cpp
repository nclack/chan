#include "chan.h"
#include <gtest/gtest.h>

class ChanTest:public ::testing::Test
{ protected:
  void fill()
  { Chan *writer = Chan_Open(full,CHAN_WRITE);
    while(CHAN_SUCCESS(Chan_Next_Try(writer,&buf,sz)));
    Chan_Close(writer);
  }
  virtual void SetUp()
  {
    empty = Chan_Alloc(16,10);  // make these the same size 
    full  = Chan_Alloc(16,10);  //   so <buf> can be reused
    buf   = Chan_Token_Buffer_Alloc(empty);
    sz    = Chan_Buffer_Size_Bytes(empty);
    fill();
  };
  virtual void TearDown()
  {
    Chan_Close(empty);
    Chan_Close(full );
    Chan_Token_Buffer_Free(buf);
  };

  Chan* empty;
  Chan* full;
  void* buf;
  size_t sz;
};

TEST_F(ChanTest,BufferCount)
{ EXPECT_EQ(16,Chan_Buffer_Count(empty));
  EXPECT_EQ(16,Chan_Buffer_Count(full )); 
}

TEST_F(ChanTest,BufferSize)
{ EXPECT_EQ(10,Chan_Buffer_Size_Bytes(empty));
  EXPECT_EQ(10,Chan_Buffer_Size_Bytes(full )); 
}

TEST_F(ChanTest,RefCounting)
{ Chan *t;
  EXPECT_EQ(1,Chan_Get_Ref_Count(empty));
  t = Chan_Open(empty,CHAN_READ);
  EXPECT_EQ(2,Chan_Get_Ref_Count(t));
  EXPECT_EQ(2,Chan_Get_Ref_Count(empty));
  Chan_Close(t);
  EXPECT_EQ(1,Chan_Get_Ref_Count(empty));
}

TEST_F(ChanTest,AllocNonPow2)
{ 
  EXPECT_EQ(0,Chan_Alloc(10,10)); 
}

TEST_F(ChanTest,InitiallyEmpty)
{ EXPECT_TRUE(Chan_Is_Empty(empty));
}

TEST_F(ChanTest,InitiallyNotFull)
{ EXPECT_FALSE(Chan_Is_Full(empty));
}

TEST_F(ChanTest,Full)
{ EXPECT_TRUE(Chan_Is_Full(full));
  EXPECT_FALSE(Chan_Is_Empty(full));
}

TEST_F(ChanTest,Drain)
{ Chan *reader = Chan_Open(full,CHAN_READ);
  EXPECT_TRUE(Chan_Is_Full(full)); 
  while(CHAN_SUCCESS(Chan_Next(reader,&buf,sz)))
  { EXPECT_FALSE(Chan_Is_Full(reader)); 
  }
  Chan_Close(reader);
  EXPECT_FALSE(Chan_Is_Full(full)); 
  EXPECT_TRUE(Chan_Is_Empty(full)); 
}

TEST_F(ChanTest,Fill)
{ Chan *writer = Chan_Open(empty,CHAN_WRITE);
  while(CHAN_SUCCESS(Chan_Next_Try(writer,&buf,sz)))
  { EXPECT_FALSE(Chan_Is_Empty(writer)); 
  }
  Chan_Close(writer);
  EXPECT_TRUE(Chan_Is_Full(empty)); 
  EXPECT_FALSE(Chan_Is_Empty(empty)); 
}

TEST_F(ChanTest,PopEmpty)
{ Chan *writer = Chan_Open(empty,CHAN_WRITE);  
  Chan *reader = Chan_Open(empty,CHAN_READ); 
  Chan_Close(writer); // should cause chan to flush to avoid deadlock
  EXPECT_FALSE(CHAN_SUCCESS(Chan_Next(reader,&buf,sz)));
  Chan_Close(reader);
}

TEST_F(ChanTest,PushFull)
{ Chan *writer = Chan_Open(full,CHAN_WRITE); 
  EXPECT_FALSE(CHAN_SUCCESS(Chan_Next_Try(writer,&buf,sz)));
  Chan_Set_Expand_On_Full(writer,0);
  EXPECT_FALSE(CHAN_SUCCESS(Chan_Next_Try(writer,&buf,sz)));  //no expand - fails
  Chan_Set_Expand_On_Full(writer,1);
  EXPECT_TRUE (CHAN_SUCCESS(Chan_Next(writer,&buf,sz)));      //   expand - allocates new memory
  Chan_Close(writer);
}

TEST_F(ChanTest,PeekEmpty)
{ Chan *reader = Chan_Open(empty,CHAN_READ);
  EXPECT_FALSE(CHAN_SUCCESS(Chan_Peek(reader,&buf,sz)));
  Chan_Close(reader);
}

TEST_F(ChanTest,PeekFull)
{ Chan *reader = Chan_Open(full,CHAN_READ);
  EXPECT_TRUE(CHAN_SUCCESS(Chan_Peek(reader,&buf,sz))); 
  Chan_Close(reader);
}



