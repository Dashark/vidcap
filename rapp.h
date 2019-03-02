#ifndef _RAPP_H_
#define _RAPP_H_
#include <rapp/rapp.h>
class Filter {
 public:
  Filter(uint8_t iou, uint32_t mina, uint32_t maxa, uint32_t mins);
  unsigned* contour(uint8_t *img, uint32_t dim, uint32_t width, uint32_t height);

 private:
  uint8_t iou_;
  uint32_t minArea_, maxArea_, minSize_;

  bool filt(int32_t sum, unsigned bwidth, unsigned bheight);
};

class Contour {
 public:
  Contour(uint8_t *org, uint32_t dim, uint32_t width, uint32_t height);
  Contour *search(Filter *filter);
  ~Contour();
  void thresh_gt(unsigned thresh);
  void thresh_lt(unsigned thresh);
  int* getPacked();
  uint8_t* getData();
  void showU8(const char name[]);
  void showBin(const char name[]);
  void save(const char file[]);
  void save_bin(const char file[]);
 private:
  const uint32_t dim_u8_, dim_bin_, rot_u8_, rot_bin_, pad_u8_, pad_bin_, off_u8_, off_bin_, width_, height_;
  uint8_t *img_, *bin_;

  uint8_t threshold_;

  uint8_t* crop(unsigned box[4]);
  uint8_t *cropByFill(unsigned box[4]);
  //void freeBin();
  //void alignBin();

  void alignCenter(float *mapx, float *mapy, size_t srcw, size_t srch, size_t dstw, size_t dsth);
  template <typename T>
    void interp2_F(const T* const data,
                   const size_t& nrows, const size_t& ncols,
                   const T* const x, const T* const y,
                   const size_t& N, T* result,
                   const long long& origin_offset);
};

#endif
