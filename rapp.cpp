#include "rapp.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <SDL/SDL.h>

/**
 *  The vertical (left/right) buffer padding value in bytes.
 */
#define RAPP_BMARK_VPAD 2

/**
 *  The horizontal (top/bottom) buffer padding value in pixels.
 */
#define RAPP_BMARK_HPAD 16

#ifdef YUV_SHOW
static void YUVplayer(const char title[], const unsigned char *yuv,int w,int h) {
  SDL_Rect *rect = new SDL_Rect;

  SDL_Surface* screen = SDL_SetVideoMode(w, h, 0, 0);
  if (screen == NULL){
    fprintf(stderr, "create surface error!\n");
    exit(1);
  }

  SDL_Overlay* overlay = SDL_CreateYUVOverlay(w, h, SDL_YV12_OVERLAY, screen);
  if (overlay == NULL){
    fprintf(stderr, "create overlay error!\n");
    exit(1);
  }

  SDL_LockSurface(screen);
  SDL_LockYUVOverlay(overlay);

  memcpy(overlay->pixels[0], yuv, w*h);
  memset(overlay->pixels[1], 0x80, w*h/4);
  memset(overlay->pixels[2], 0x80, w*h/4);

  SDL_UnlockYUVOverlay(overlay);
  SDL_UnlockSurface(screen);

  rect->w = w;
  rect->h = h;
  rect->x = rect->y = 0;
  SDL_WM_SetCaption(title, 0);
  SDL_DisplayYUVOverlay(overlay, rect);

  SDL_Delay(10);

  SDL_FreeYUVOverlay(overlay);
  SDL_FreeSurface(screen);
  delete rect;
}
#endif
////////////////////////////////////////////////////////////////////
Filter::Filter(uint8_t iou, uint32_t mina, uint32_t maxa, uint32_t mins) {
  this->iou_ = iou;
  this->minArea_ = mina;
  this->maxArea_ = maxa;
  this->minSize_ = mins;
}

unsigned * Filter::contour(uint8_t *img, uint32_t dim, uint32_t width, uint32_t height) {
  unsigned origin[2], count = 0;
  char cont[1000];
  unsigned* box = new unsigned[4];
  box[0]=box[1]=box[2]=box[3] = 0;
  uint8_t *dest = static_cast<uint8_t*>(rapp_malloc(dim * height,0));
  int ret = 0;
  while((ret = rapp_contour_8conn_bin(origin,cont,1000,img, dim,width,height)) >= 0) {
    count += 1;
    //std::cout << "How many contours are found: " << ret << std::endl;
    ret = rapp_fill_8conn_bin(dest, dim,img, dim,width,height,origin[0],origin[1]);
    if(ret < 0) {
      //std::cout << "rapp_fill_8conn_bin : " << rapp_error(ret) << std::endl;
      //std::cout << origin[0] << "::" << origin[1] << std::endl;
      delete[] box;
      box = nullptr;
      break;
    }
    //std::cout << "Origin " << origin[0] << "," << origin[1] << std::endl;
    int sum = rapp_stat_sum_bin(dest, dim, width, height);
    if(sum <= 0) {
      //std::cout << "sum break  " << count << std::endl;
      delete[] box;
      box = nullptr;
      break;
    }
    ret = rapp_crop_box_bin(dest, dim,width,height,box);
    if(ret <= 0)
      std::cout << "rapp_crop_box_bin : " << rapp_error(ret) << std::endl;

    //std::cout << "Contour Box: " << box[0] << "," << box[1] << "," << box[2] <<","<< box[3] << "," << sum << std::endl;
    ret = rapp_bitblt_xor_bin(img, dim,0,dest, dim,0,width,height);
    if(ret < 0)
      std::cout << "rapp_bitblt_xor_bin : " << rapp_error(ret) << std::endl;
    //std::cout << "rapp_bitblt_xor_bin : " << rapp_error(ret) << std::endl;
    //YUVplayer("testing", img, dim, height);
    //ret = rapp_pad_const_bin(img, dim, 0, width-20, height-20, 10, 0);
    if(filt(sum, box[2], box[3])) {
      //std::cout << "connected break " << count << std::endl;
      std::cout << "Contour Box: " << box[0] << "," << box[1] << "," << box[2] <<","<< box[3] << "," << sum << "," << (1.0f*sum)/(box[2]*box[3]) << std::endl;
      break;
    }

    sum = rapp_stat_sum_bin(img, dim, width, height);
    if(sum <= 0) {
      //std::cout << "sum break2  " << count << std::endl;
      delete[] box;
      box = nullptr;

      break;
    }

    origin[0] = origin[1] = 0;
  }
  rapp_free(dest);
  //std::cout << "Filter::contour end : " << rapp_error(ret) << std::endl;
  return box;
}

bool Filter::filt(int32_t sum, unsigned bwidth, unsigned bheight) {
  unsigned area = bwidth * bheight;
  uint8_t iout = sum * 100 / area;
  return bwidth > minSize_ && bheight > minSize_ && area > minArea_ && area < maxArea_ && iout > this->iou_;
}

////////////////////////////////////////////////////////////////////
Contour::Contour(uint8_t *org, uint32_t dim, uint32_t width, uint32_t height):dim_u8_(rapp_align(width)), dim_bin_(rapp_align((width + 7) / 8)), rot_u8_(rapp_align(height)), rot_bin_(rapp_align((height + 7) / 8)), pad_u8_(rapp_align(RAPP_BMARK_VPAD)), pad_bin_(rapp_align((RAPP_BMARK_VPAD + 7) / 8)), off_u8_(RAPP_BMARK_HPAD*(dim_u8_+2*pad_u8_) + pad_u8_), off_bin_(RAPP_BMARK_HPAD*(dim_bin_+2*pad_bin_) + pad_bin_), width_(width), height_(height) {
  assert(org != nullptr);
  size_t u8sz = (height + 2 * RAPP_BMARK_HPAD) * (dim_u8_ + 2 * pad_u8_);
  size_t binsz = (height + 2 * RAPP_BMARK_HPAD) * (dim_bin_ + 2 * pad_bin_);
  img_ = static_cast<uint8_t*>(rapp_malloc(u8sz, 0));
  assert(img_ != nullptr);
  rapp_pixop_copy_u8(img_+off_u8_, dim_u8_ + 2 * pad_u8_, org, dim, width, height);
  //for(uint32_t i = 0; i < height; ++i)
  //std::uninitialized_copy_n(org + i * width, width, img_ + i * dim);
  //std::uninitialized_fill_n(img_, dim_, 0);
  //std::uninitialized_fill_n(img_+dim_*(height_-1), dim_, 0);

  bin_ = static_cast<uint8_t*>(rapp_malloc(binsz, 0));
  //dim8_ = 0;
  threshold_ = 0;
}

Contour::~Contour() {
  //std::cout << "Contour destructor!!!" << std::endl;
  rapp_free(img_);
  rapp_free(bin_);
  //std::cout << "Contour destructor end!!!" << std::endl;
}

void Contour::thresh_gt(unsigned thresh) {
  assert(bin_ != nullptr);
  int ret = rapp_thresh_gt_u8(bin_+off_bin_, dim_bin_+2*pad_bin_, img_+off_u8_, dim_u8_+2*pad_u8_, width_, height_, thresh);
  //std::cout << "thresh_gt: " << rapp_error(ret) << std::endl;
  //std::cout << "thresh_gt: " << dim_bin_ << "  " << thresh << std::endl;
  ret = rapp_pad_const_bin(bin_+off_bin_, dim_bin_+2*pad_bin_, 0, width_, height_, 1, 0);
  //std::cout << "rapp_pad_const_bin:" << rapp_error(ret) << std::endl;
  threshold_ = thresh;
}

void Contour::thresh_lt(unsigned thresh) {
  assert(bin_ != nullptr);
  int ret = rapp_thresh_lt_u8(bin_+off_bin_, dim_bin_+2*pad_bin_, img_+off_u8_, dim_u8_+2*pad_u8_, width_, height_, thresh);
  //std::cout << "thresh_lt: " << rapp_error(ret) << std::endl;
  //std::cout << "thresh_lt: " << dim_bin_ << "  " << thresh << std::endl;
  ret = rapp_pad_const_bin(bin_+off_bin_, dim_bin_+2*pad_bin_, 0, width_, height_, 1, 0);
  //std::cout << "rapp_pad_const_bin:" << rapp_error(ret) << std::endl;
  threshold_ = thresh;
}

unsigned getU8dim(unsigned width, unsigned height) {
  size_t size = width;
  size_t asize = rapp_align(size);
  //std::cout << size << "," << asize << std::endl;
  return asize;
}

Contour* Contour::search(Filter *filter) {
  assert(filter != nullptr);
  Contour *cont = nullptr;
  unsigned *box = filter->contour(bin_+off_bin_, dim_bin_+2*pad_bin_, width_, height_);  //at least 4
  if(box != nullptr) {
    //std::cout << "Contour Box for Crop: " << box[0] << "," << box[1] << "," << box[2] <<","<< box[3] << std::endl;
    //unsigned boxdim = getU8dim(box[2], box[3]);

    //box[2] = rapp_align(box[2]);
    //std::cout << "Box dim: " << box[2] << std::endl;
    uint8_t *img = crop(box);
    cont = new Contour(img, rapp_align(box[2]), box[2], box[3]);
    delete[] img;
    delete[] box;
  }

  return cont;
}

uint8_t* Contour::crop(unsigned box[4]) {
  unsigned dim = rapp_align(box[2]);
  size_t size = dim * box[3];
  uint8_t* buf = new uint8_t[size];
  assert(buf != nullptr);
  unsigned srcoff = off_u8_ + box[0] + box[1] * (dim_u8_ + 2*pad_u8_);
  unsigned dstoff = 0;
  for(unsigned i = 0; i < box[3]; ++i) {
    std::uninitialized_copy_n(img_ + srcoff, box[2], buf + dstoff);
    srcoff += dim_u8_ + 2*pad_u8_;
    dstoff += dim;
  }
  return buf;
}

uint8_t* Contour::cropByFill(unsigned box[4]) {
  uint32_t size = dim_u8_ * height_;
  uint8_t *org = static_cast<uint8_t*>(rapp_malloc(size, 0));
  assert(org != nullptr);
  std::uninitialized_copy_n(img_, size, org);

  size = box[1] * dim_u8_; //upper size of image
  std::uninitialized_fill_n(org, size, 255);  // y
  uint32_t off = size;
  uint32_t size_right = dim_u8_ - box[0] - box[2];
  for(uint32_t i = 0; i < box[3]; ++i) {
    std::uninitialized_fill_n(org + off, box[0], 255);  //left part of image
    std::uninitialized_fill_n(org + off + box[0] + box[2], size_right, 255); //right part of image
    off += dim_u8_; //next row
  }
  off = (box[1] + box[3]) * dim_u8_; //lower part of image
  std::uninitialized_fill_n(org + off, dim_u8_ * (height_ - box[1] - box[3]), 255); //lower of y+height

  return org;
}
/*
void Contour::freeBin() {
  if(bin_ != nullptr) {
    rapp_free(bin_);
  }
  bin_ = nullptr;
}

void Contour::alignBin() {
  freeBin();
  dim8_ = rapp_align((width_ + 7) / 8);
  bin_ = static_cast<uint8_t*>(rapp_malloc(dim8_ * height_, 0));
  assert(bin_ != nullptr);
}
*/
void Contour::alignCenter(float *mapx, float *mapy, size_t srcw, size_t srch, size_t dstw, size_t dsth) {
  //align center of 2 images.
  //size_t sz = dstw * dsth;
  //for(size_t i = 0; i < sz; ++i) {mapx[i] = (i + 0.5) * srcw / dstw - 0.5;}
  //for(size_t i = 0; i < sz; ++i) {mapy[i] = (i + 0.5) * srch / dsth - 0.5;}
  for(size_t h = 0; h < dsth; ++h) {
    for(size_t r = 0; r < dstw; ++r) {
      mapx[r + h * dstw] = (r + 0.5) * srcw / dstw - 0.5;
      mapy[r + h * dstw] = (h + 0.5) * srch / dsth - 0.5;
    }
  }
}

template <typename T>
void Contour::interp2_F(const T* const data,
               const size_t& nrows, const size_t& ncols,
               const T* const x, const T* const y,
               const size_t& N, T* result,
               const long long& origin_offset=0){

  for (int i = 0; i < N; ++i) {

    // get coordinates of bounding grid locations
    long long x_1 = ( long long) std::floor(x[i]) - origin_offset;
    long long x_2 = x_1 + 1;
    long long y_1 = ( long long) std::floor(y[i]) - origin_offset;
    long long y_2 = y_1 + 1;

    // handle special case where x/y is the last element
    if ( (x[i] - origin_offset) == (nrows-1) )   { x_2 -= 1; x_1 -= 1;}
    if ( (y[i] - origin_offset) == (ncols-1) )   { y_2 -= 1; y_1 -= 1;}

    // return 0 for target values that are out of bounds
    if (x_1 < 0 || x_2 > (nrows - 1) ||  y_1 < 0 || y_2 > (ncols - 1)){
      result[i] = 0;
    } 
    else {
      
      // get the array values
      const T& f_11 = data[x_1 + y_1*nrows];
      const T& f_12 = data[x_1 + y_2*nrows];
      const T& f_21 = data[x_2 + y_1*nrows];
      const T& f_22 = data[x_2 + y_2*nrows];

      // compute weights
      T w_x1 = x_2 - (x[i] - origin_offset);
      T w_x2 = (x[i] - origin_offset) - x_1;
      T w_y1 = y_2 - (y[i] - origin_offset);
      T w_y2 = (y[i] - origin_offset) - y_1;

      T a,b;
      a = f_11 * w_x1 + f_21 * w_x2;
      b = f_12 * w_x1 + f_22 * w_x2;
      result[i] = a * w_y1 + b * w_y2;
    }
  }
}

int* Contour::getPacked() {

  float mx[100], my[100];
  alignCenter(mx, my, width_, height_, 10, 10);
  /*
  for(int i = 1; i < 10; ++i) {
    std::uninitialized_copy_n(mx, 10, mx + i * 10);
    std::uninitialized_copy_n(my, 10, my + i * 10);
  }
  */
  //std::cout << "getPacked: " << width_ << " " << height_ << std::endl;
  //std::cout << "getPacked: " << mx[0] << " " << mx[1] << " " << mx[2] << std::endl;
  //std::cout << "getPacked: " << my[0] << " " << my[1] << " " << my[2] << std::endl;
  //convert uint8 to float
  float *imgb = new float[width_*height_];
  uint32_t dim = dim_u8_ + 2 * pad_u8_;
  for(uint32_t h = 0; h < height_; ++h) {
    uint32_t dstoff = h * width_;
    uint32_t srcoff = off_u8_ + h * dim;
    for(uint32_t r = 0; r < width_; ++r) {
      imgb[dstoff + r] = static_cast<float>(img_[srcoff + r]);
    }
  }
  float* buf = new float[100];
  interp2_F(imgb, width_, height_, mx, my, 100, buf, 0);

  //convert float to int
  int *ibuf = new int[100];
  for(int i = 0; i < 100; ++i)
    ibuf[i] = std::ceil(buf[i]);
#ifdef YUV_SHOW

  uint8_t *tmp = new uint8_t[100];
  for(int i = 0; i < 100; ++i)
    tmp[i] = static_cast<uint8_t>(std::ceil(buf[i]));
  YUVplayer("resized", tmp, 10, 10);
  delete[] tmp;

#endif

  delete[] imgb;
  delete[] buf;
  return ibuf;
}

uint8_t* Contour::getData() {
  return img_;
}

void Contour::showBin(const char file[]) {
#ifdef YUV_SHOW
  YUVplayer(file, bin_+off_bin_, dim_bin_+2*pad_bin_, height_);
#endif
}
void Contour::showU8(const char file[]) {
#ifdef YUV_SHOW
  YUVplayer(file, img_+off_u8_, dim_u8_+2*pad_u8_, height_);
#endif
}

void Contour::save(const char file[]) {
  FILE *pf = fopen(file, "wb");
  uint8_t *buf = img_ + off_u8_;
  for(int i = 0; i < height_; ++i) {
    fwrite(buf, 1, dim_u8_, pf);
    buf += dim_u8_ + 2 * pad_u8_;
  }
  fclose(pf);
}

void Contour::save_bin(const char file[]) {
  uint8_t *buf = static_cast<uint8_t*>(rapp_malloc(dim_u8_ * height_, 0));
  uint8_t *buf1 = static_cast<uint8_t*>(rapp_malloc(dim_bin_ * height_, 0));
  rapp_bitblt_not_bin(buf1, dim_bin_, 0, bin_, dim_bin_, 0, width_, height_);
  FILE *pf = fopen(file, "wb");
  rapp_type_bin_to_u8(buf, dim_u8_, bin_ + off_bin_, dim_bin_ + 2*pad_bin_, width_, height_);
  fwrite(buf, 1, dim_u8_*height_, pf);
  fclose(pf);
  rapp_free(buf);
  rapp_free(buf1);
}
