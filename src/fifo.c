#include "fifo.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//////////////////////////////////////////////////////////////////////
//  Logging    ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#define fifo_warning(...) printf(__VA_ARGS__);
#define fifo_error(...)   do{fprintf(stderr,__VA_ARGS__);exit(-1);}while(0)
#ifdef DEBUG_RING_FIFO
#define fifo_debug(...) printf(__VA_ARGS__)
#else
#define fifo_debug(...)
#endif

// warnings
#if 1
#define DEBUG_RING_FIFO_WARN_RESIZE_ON_PUSH  fifo_warning("Resized on push: *pbuf was smaller than nominal token buffer size. (%zu < %zu)\r\n",sz,self->buffer_size_bytes)
#define DEBUG_RING_FIFO_WARN_RESIZE_ON_PEEK  fifo_warning("Resized on peek: *pbuf was smaller than nominal token buffer size. (%zu < %zu)\r\n",sz,self->buffer_size_bytes)
#else
#define DEBUG_RING_FIFO_WARN_RESIZE_ON_PUSH
#define DEBUG_RING_FIFO_WARN_RESIZE_ON_PEEK
#endif

// debug
#if 0
#define DEBUG_RING_FIFO
#define DEBUG_RING_FIFO_PUSH
#define DEBUG_REQUEST_STORAGE \
  fifo_debug("REQUEST %7d bytes (%7d items) above current %7d bytes by %s\n",request * bytes_per_elem, request, *nelem * bytes_per_elem, msg)
#define Fifo_Assert(expression) \
  if(!(expression))\
    fifo_error("Assertion failed: %s\n\tIn %s (line: %u)\n", #expression, __FILE__ , __LINE__ )
#else
#define Fifo_Assert(expression) (expression)  
#define DEBUG_REQUEST_STORAGE
#endif

//////////////////////////////////////////////////////////////////////
//  Utilities  ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#define return_if_fail(cond)          { if(!(cond)) return; }
#define return_val_if(cond,val)       { if( (cond)) return (val); }
#define IS_POW2_OR_ZERO(v) ( ((v) & ((v) - 1)) ==  0  )
#define IS_POW2(v)         (!((v) & ((v) - 1)) && (v) )
#define MOD_UNSIGNED_POW2(n,d)   ( (n) & ((d)-1) )

void *Fifo_Malloc( size_t nelem, const char *msg )
{ void *item = malloc( nelem );
  if( !item )
    fifo_error("Could not allocate memory.\n%s\n",msg);
  return item;
}
void *Fifo_Calloc( size_t nelem, size_t bytes_per_elem, const char *msg )
{ void *item = calloc( nelem, bytes_per_elem );
  if( !item )
    fifo_error("Could not allocate memory.\n%s\n",msg);
  return item;
}
void Fifo_Realloc( void **item, size_t nelem, const char *msg )
{ void *it = *item;
  Fifo_Assert(item);
  if( !it )
    it = malloc( nelem );
  else
    it = realloc( it, nelem );
  if( !it )
    fifo_error("Could not reallocate memory.\n%s\n",msg);
  *item = it;
}

void RequestStorage( void** array, size_t *nelem, size_t request, size_t bytes_per_elem, const char *msg )
{ size_t n = request+1;
  if( n <= *nelem ) return;
  *nelem = (size_t) (1.25 * n + 64 );
  DEBUG_REQUEST_STORAGE;
  Fifo_Realloc( array, *nelem * bytes_per_elem, "Resize" );
}

inline size_t _next_pow2_size_t(size_t v)
{ v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
#if SIZET_BYTES==8
  v |= v >> 32;
#endif
  v++;
  return v;
}
void RequestStorageLog2( void** array, size_t *nelem, size_t request, size_t bytes_per_elem, const char *msg )
{ size_t n = request+1;
  if( n <= *nelem ) return;
  *nelem = _next_pow2_size_t(n);
  DEBUG_REQUEST_STORAGE;
  Fifo_Realloc( array, *nelem * bytes_per_elem, "Resize" );
}

//////////////////////////////////////////////////////////////////////
//  Vector     ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
typedef void* PVOID;
typedef struct _vector_PVOID   
        { PVOID* contents;      
          size_t nelem;         
          size_t count;         
          size_t bytes_per_elem;
        } vector_PVOID;        
                                  
vector_PVOID *vector_PVOID_alloc   ( size_t nelem );
void          vector_PVOID_request ( vector_PVOID *self, size_t idx );
void          vector_PVOID_request_pow2 ( vector_PVOID *self, size_t idx );
void          vector_PVOID_free    ( vector_PVOID *self );
void          vector_PVOID_dump    ( vector_PVOID *self, char* filename );  

#define VECTOR_EMPTY { NULL, 0, 0, 0 }

vector_PVOID *vector_PVOID_alloc   ( size_t nelem )                                            
{ size_t bytes_per_elem = sizeof( PVOID );                                                          
  vector_PVOID *self   = (vector_PVOID*) Fifo_Malloc( sizeof(vector_PVOID), "vector_init" ); 
  self->contents        = (PVOID*) Fifo_Calloc( nelem, bytes_per_elem, "vector_init");           
  self->bytes_per_elem  = bytes_per_elem; 
  self->count           = 0;              
  self->nelem           = nelem;          
  return self;                            
} 
  
void vector_PVOID_request( vector_PVOID *self, size_t idx ) 
{ if( !self->bytes_per_elem )                
    self->bytes_per_elem = sizeof(PVOID);     
  RequestStorage( (void**) &self->contents,  
                  &self->nelem,              
                  idx,                       
                  self->bytes_per_elem,      
                  "vector_request" );        
} 
  
void vector_PVOID_request_pow2( vector_PVOID *self, size_t idx ) 
{ if( !self->bytes_per_elem )                
    self->bytes_per_elem = sizeof(PVOID);     
  RequestStorageLog2( (void**) &self->contents,  
                      &self->nelem,              
                      idx,                       
                      self->bytes_per_elem,      
                      "vector_log2_request" );   
} 
  
void vector_PVOID_free( vector_PVOID *self ) 
{ if(self)                                       
  { if( self->contents ) free( self->contents ); 
    self->contents = NULL;                       
    free(self);                                  
  }                                              
} 
  
void vector_PVOID_free_contents( vector_PVOID *self ) 
{ if(self)                                       
  { if( self->contents ) free( self->contents ); 
    self->contents = NULL;                       
    self->nelem = 0;                             
    self->count = 0;                             
  }                                              
} 
  
void vector_PVOID_dump( vector_PVOID *self, char* filename ) 
{ FILE *fp;                                                      
  Fifo_Assert(fp = fopen(filename,"wb"));                     
  fwrite(self->contents,sizeof(PVOID),self->nelem,fp);            
  fclose(fp);                                                    
  fifo_warning("Wrote %s\r\n",filename);                              
}

//////////////////////////////////////////////////////////////////////
//  Fifo   ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
typedef struct _ring_fifo
{ vector_PVOID *ring;
  size_t        head; // write cursor
  size_t        tail; // read  cursor

  size_t        buffer_size_bytes;
} Fifo_;

Fifo*
Fifo_Alloc(size_t buffer_count, size_t buffer_size_bytes )
{ Fifo_ *self = (Fifo_ *)Fifo_Malloc( sizeof(Fifo_), "Fifo_Alloc" );
  
  Fifo_Assert( IS_POW2( buffer_count ) );
  return_val_if(!IS_POW2(buffer_count),NULL);

  self->head = 0;
  self->tail = 0;
  self->buffer_size_bytes = buffer_size_bytes;

  self->ring = vector_PVOID_alloc( buffer_count );
  { vector_PVOID *r = self->ring;
    PVOID *cur = r->contents + r->nelem,
          *beg = r->contents;
    while( cur-- > beg )
      *cur = Fifo_Malloc( buffer_size_bytes, "Fifo_Alloc: Allocating buffers" );
  }

#ifdef DEBUG_RINGFIFO_ALLOC
  Fifo_Assert( Fifo_Is_Empty(self) );
  Fifo_Assert(!Fifo_Is_Full (self) );
#endif
  return self;
}

void 
Fifo_Free( Fifo *self_ )
{ Fifo_ *self = (Fifo_*)self_;
  return_if_fail(self);
  if( self->ring )
  { vector_PVOID *r = self->ring;
    PVOID *cur = r->contents + r->nelem,
          *beg = r->contents;
    while( cur-- > beg )
      free(*cur);
    vector_PVOID_free( r );
    self->ring = NULL;    
  }
  free(self);	
}

void 
Fifo_Expand( Fifo *self_ ) 
{ Fifo_ *self = (Fifo_*)self_;
  vector_PVOID *r = self->ring;
  size_t old  = r->nelem,   // size of ring buffer _before_ realloc
         n,                 // delta in size (number of added elements)
         head = MOD_UNSIGNED_POW2( self->head, old ),
         tail = MOD_UNSIGNED_POW2( self->tail, old ),
         buffer_size_bytes = self->buffer_size_bytes;

  vector_PVOID_request_pow2( r, old/*+1*/ ); // size to next pow2  
  n = r->nelem - old; // the number of slots added
    
  { PVOID *buf = r->contents,
          *beg = buf,     // (will be) beginning of interval requiring new malloced data
          *cur = buf + n; // (will be) end       "  "        "         "   "        "
    
    // Need to guarantee that the new interval is outside of the
    //    tail -> head interval (active data runs from tail to head).
    if( head <= tail && (head!=0 || tail!=0) ) //The `else` case can handle when head=tail=0 and avoid a copy
    { // Move data to end
      size_t nelem = old-tail;  // size of terminal interval
      beg += tail;              // source
      cur += tail;              // dest
      if( n > nelem ) memcpy ( cur, beg, nelem * sizeof(PVOID) ); // no overlap - this should be the common case
      else            memmove( cur, beg, nelem * sizeof(PVOID) ); // some overlap
      // adjust indices
      self->head += tail + n - self->tail; // want to maintain head-tail == # queue items
      self->tail = tail + n;
    } else // tail < head - no need to move data
    { //adjust indices
      self->head += tail - self->tail;
      self->tail = tail;
      
      //setup interval for mallocing new data
      beg += old;
      cur += old;
    }
    while( cur-- > beg )
      *cur = Fifo_Malloc( buffer_size_bytes, 
                             "Fifo_Expand: Allocating new buffers" );
  }
}

void
Fifo_Resize(Fifo* self_, size_t buffer_size_bytes)
{ Fifo_ *self = (Fifo_*)self_;
  vector_PVOID *r = self->ring;
  size_t i,n = r->nelem,
         head = MOD_UNSIGNED_POW2( self->head, n ), // Write point (push)- points to a "dead" buffer
         tail = MOD_UNSIGNED_POW2( self->tail, n ); // Read point (pop)  - points to a "live" buffer
  if (self->buffer_size_bytes < buffer_size_bytes)
  {
    // Resize the buffers    
    for(i=0;i<n;++i)
    { size_t idx;
      void *t;
      idx = MOD_UNSIGNED_POW2(i,n);
      Fifo_Assert(t = realloc(r->contents[idx], buffer_size_bytes));
      r->contents[idx] = t;
    }
  }
  self->buffer_size_bytes = buffer_size_bytes;
}

static inline size_t
_swap( Fifo *self_, void **pbuf, size_t idx)
{ Fifo_ *self = (Fifo_*)self_;
  vector_PVOID *r = self->ring;                  // taking the mod on read (here) saves some ops                     
  idx = MOD_UNSIGNED_POW2( idx, r->nelem );      //   relative to on write since that involve writing to a reference.
  { void **cur = r->contents + idx,              // in pop/push idx could overflow, but that 
          *t   = *cur;                           //   would be ok since r->nelem is a divisor of
    *cur  = *pbuf;                               //   2^sizeof(size_t)
    *pbuf = t;                                   // Note that the ...Is_Empty and ...Is_Full 
  }                                              //   constructs still work with the possibility of
  return idx;                                    //   an overflow
}

unsigned int
Fifo_Pop( Fifo *self_, void **pbuf, size_t sz)
{ Fifo_ *self = (Fifo_*)self_;
  fifo_debug("- head: %-5d tail: %-5d size: %-5d\r\n",self->head, self->tail, self->head - self->tail);
  return_val_if( Fifo_Is_Empty(self), 1);
  if( sz<self->buffer_size_bytes )                          //small arg - police  - resize to larger before swap
    Fifo_Assert(*pbuf = realloc(*pbuf,self->buffer_size_bytes)); //null  arg -         - also handled by this mechanism
  _swap( self, pbuf, self->tail++ );                        //big   arg - ignored
  return 0;
}

unsigned int
Fifo_Peek( Fifo *self_, void **pbuf, size_t sz)
{ Fifo_ *self = (Fifo_*)self_;
  fifo_debug("o head: %-5d tail: %-5d size: %-5d\r\n",self->head, self->tail, self->head - self->tail);
  return_val_if( Fifo_Is_Empty(self), 1);
  if( sz<self->buffer_size_bytes )                          //small arg - police  - resize to larger before swap
  { Fifo_Assert(*pbuf = realloc(*pbuf,self->buffer_size_bytes)); //null  arg -         - also handled by this mechanism
    DEBUG_RING_FIFO_WARN_RESIZE_ON_PEEK;
  }
  
  { vector_PVOID *r = self->ring;
    memcpy( *pbuf, 
            r->contents[MOD_UNSIGNED_POW2(self->tail, r->nelem)],
            self->buffer_size_bytes );
  }
  return 0;
}

unsigned int
Fifo_Peek_At( Fifo *self_, void **pbuf, size_t sz, size_t index)
{ Fifo_ *self = (Fifo_*)self_;
  fifo_debug("o head: %-5d tail: %-5d size: %-5d\r\n",self->head, self->tail, self->head - self->tail);
  return_val_if( Fifo_Is_Empty(self), 1);  
  if( sz<self->buffer_size_bytes )                          //small arg - police  - resize to larger before swap
  { Fifo_Assert(*pbuf = realloc(*pbuf,self->buffer_size_bytes)); //null  arg -         - also handled by this mechanism
    DEBUG_RING_FIFO_WARN_RESIZE_ON_PEEK;
  }
    
  { vector_PVOID *r = self->ring;
    memcpy( *pbuf, 
            r->contents[MOD_UNSIGNED_POW2(self->tail + index, r->nelem)],
            self->buffer_size_bytes );
  }
  return 0;
}

unsigned int
Fifo_Push_Try( Fifo *self_, void **pbuf, size_t sz)
{ //fifo_debug("+?head: %-5d tail: %-5d size: %-5d TRY\r\n",self->head, self->tail, self->head - self->tail);
  Fifo_ *self = (Fifo_*)self_;
  if( Fifo_Is_Full(self) )
    return 1;
      
  if( sz<self->buffer_size_bytes )                          //small arg - police  - resize to larger before swap
  { Fifo_Assert(*pbuf = realloc(*pbuf,self->buffer_size_bytes)); //null  arg -         - also handled by this mechanism
    DEBUG_RING_FIFO_WARN_RESIZE_ON_PUSH;                    //big   arg - police  - resize queue storage.
  }
  if(sz>self->buffer_size_bytes)                            
    Fifo_Resize(self,sz);
    
  _swap( self, pbuf, self->head++ );
  return 0;
}

unsigned int
Fifo_Push( Fifo *self_, void **pbuf, size_t sz, int expand_on_full)
{ Fifo_ *self = (Fifo_*)self_;
  fifo_debug("+ head: %-5d tail: %-5d size: %-5d\r\n",self->head, self->tail, self->head - self->tail);

  return_val_if( 0==Fifo_Push_Try(self, pbuf, sz), 0 );
  
  // Handle when full      
    
  if( sz<self->buffer_size_bytes )                          //small arg - police  - resize to larger before swap
  { Fifo_Assert(*pbuf = realloc(*pbuf,self->buffer_size_bytes)); //null  arg -         - also handled by this mechanism
    DEBUG_RING_FIFO_WARN_RESIZE_ON_PUSH;                    //big   arg - police  - resize queue storage.
  }
  if(sz>self->buffer_size_bytes)                            
    Fifo_Resize(self,sz);  
    
  if( expand_on_full )      // Expand
    Fifo_Expand(self);  
  else                      // Overwrite
    self->tail++;
  _swap( self, pbuf, self->head++ );
  return !expand_on_full;   // return true iff data was overwritten
}

inline 
size_t Fifo_Buffer_Size_Bytes(Fifo *self)
{ return ((Fifo_*)self)->buffer_size_bytes;
}
void*
Fifo_Alloc_Token_Buffer( Fifo *self_ )
{ Fifo_ *self = (Fifo_*)self_;
  return Fifo_Malloc( self->buffer_size_bytes, 
                         "Fifo_Alloc_Token_Buffer" );
}

void Fifo_Resize_Token_Buffer( Fifo *self_, void **pbuf )
{ Fifo_ *self = (Fifo_*)self_;
  Fifo_Realloc( pbuf, self->buffer_size_bytes, 
                         "Fifo_Realloc_Token_Buffer" );
}

unsigned char Fifo_Is_Empty(Fifo *self_)
{ Fifo_ *self = (Fifo_*)self_;
  return ( (self)->head == (self)->tail );
}
unsigned char Fifo_Is_Full (Fifo *self_)
{ Fifo_ *self = (Fifo_*)self_;
  return ( (self)->head == (self)->tail + (self)->ring->nelem );
}
