#include "chan.h"
#include <gtest/gtest.h>

class ChanTest:public ::testing::Test
{ protected:
  void fill()
  { chan *writer = Chan_Open(full,CHAN_WRITE);
    while(CHAN_SUCCESS(Chan_Next_Try(full,&buf,sz)));
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

  chan* empty;
  chan* full;
  void *buf;
  size_t sz;
};

TEST_F(ChanTest,RefCounting)
{ chan *t;
  EXPECT_EQ(Chan_Get_Ref_Count(empty),1);
  t = Chan_Open(empty,CHAN_READ);
  EXPECT_EQ(Chan_Get_Ref_Count(t),2);
  EXPECT_EQ(Chan_Get_Ref_Count(empty),2);
  Chan_Close(t);
  EXPECT_EQ(Chan_Get_Ref_Count(empty),1);
}

TEST_F(ChanTest,AllocNonPow2)
{ EXPECT_TRUE(Chan_Alloc(10,10)==(void*)0);
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
{ chan *reader = Chan_Open(full,CHAN_READ);
  while(CHAN_SUCCESS(Chan_Next(reader,&buf,sz)))
  { EXPECT_FALSE(Chan_Is_Full(reader)); 
  }
  Chan_Close(reader);
  EXPECT_FALSE(Chan_Is_Full(full)); 
  EXPECT_TRUE(Chan_Is_Empty(full)); 
}

TEST_F(ChanTest,Fill)
{ chan *writer = Chan_Open(empty,CHAN_WRITE);
  while(CHAN_SUCCESS(Chan_Next_Try(writer,&buf,sz)))
  { EXPECT_FALSE(Chan_Is_Empty(writer)); 
  }
  Chan_Close(writer);
  EXPECT_TRUE(Chan_Is_Full(empty)); 
  EXPECT_FALSE(Chan_Is_Empty(empty)); 
}

TEST_F(ChanTest,PopEmpty)
{ chan *reader = Chan_Open(full,CHAN_READ); 
  EXPECT_FALSE(CHAN_SUCCESS(Chan_Next(reader,&buf,sz)));
  Chan_Close(reader);
}

TEST_F(ChanTest,PushFull)
{ chan *writer = Chan_Open(empty,CHAN_WRITE); 
  EXPECT_FALSE(CHAN_SUCCESS(Chan_Next_Try(writer,&buf,sz)));
  Chan_Set_Expand_On_Full(writer,0);
  EXPECT_FALSE(CHAN_SUCCESS(Chan_Next(writer,&buf,sz)));  //no expand - fails
  Chan_Set_Expand_On_Full(writer,1);
  EXPECT_TRUE (CHAN_SUCCESS(Chan_Next(writer,&buf,sz)));  //   expand - allocates new memory
}

TEST_F(ChanTest,PeekEmpty)
{ EXPECT_FALSE(CHAN_SUCCESS(Chan_Peek(empty,&buf,sz))); 
}

TEST_F(ChanTest,PeekFull)
{ EXPECT_TRUE(CHAN_SUCCESS(Chan_Peek(full,&buf,sz))); 
}



