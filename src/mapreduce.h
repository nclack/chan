
typedef struct _map_reduce_data MRData;
/** \todo Add a function type that accomidates "generic" types a bit
          better.  By "generic" type, I mean that the function is
          defined for variable width types.
    \todo Add a function type that allows parameters to be passed 
          in, etc...
 */
typedef int (*MRFunction)(void *dst, void *src);

MRData MRPackage(void *buf, size_t bytesof_elem, size_t bytesof_data);
MRData MREmpty(size_t bytesof_elem);
void   MRRelease(MRData *mrdata);

void MRSetWorkerThreadCount(int nthreads);

MRData map  (MRData dst, MRData src, MRFunction f); ///< Returns the 
MRData foldl(MRData dst, MRData src, MRFunction f);

/** \file
    \section map() example

    Take an array of 8-bit RGB tuples from a color image and map to 
    convert to grayscale, outputing an array of doubles.

    The function to be applied is defined:
    \code
    int grayscale(void *_dst, void *_src)
    { double        *dst = (double*) _dst;
      unsigned char *src = (unsigned char*) _src;
      *dst = 0.630*src[0] + 0.310*src[1] + 0.155*src[2];
      return 0;
    }
    \endcode

    Perform the grayscale conversion in parallel over a 512x512 RGB image.
    The output image will be dynamically allocated.
    \code
    MRData result = 
      map(MREmpty(sizeof(double)),
          MRPackage(image,3,512*512*3),
          grayscale);
    // use the result
    MRRelease(&result);
    \endcode

    If the output image has already been allocated:
    \code
    double out[512*512];
    map(MRPackage(out,8,512*512)),
        MRPackage(image,3,512*512*3),
        grayscale);
    // use "out"    
    \endcode
  */