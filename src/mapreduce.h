/** \file
    Map-reduce framework.
    \author Nathan Clack
    \date   2012

    \todo Add a function type that accomidates "generic" types a bit
          better.  By "generic" type, I mean that the function is
          defined for variable width types.
    \todo Add a function type that allows parameters to be passed 
          in, etc...
 */

typedef struct _map_reduce_data MRData;
typedef int (*MRFunction)(void *dst, void *src);

MRData MRPackage(void *buf, size_t bytesof_elem, size_t bytesof_data);
MRData MREmpty(size_t bytesof_elem);
void   MRRelease(MRData *mrdata);

void MRSetWorkerThreadCount(int nthreads);

MRData map  (MRData dst, MRData src, MRFunction f); ///< Returns the result
MRData foldl(MRData dst, MRData src, MRFunction f); ///< \todo implement foldl()

