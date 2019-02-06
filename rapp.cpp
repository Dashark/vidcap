#include "rapp.h"
#include <iostream>
#include <algorithm>
#include <cassert>

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
  //ret = rapp_pad_const_bin(img, dim, 0, width, height, 1, 0);
  std::cout << rapp_error(ret) << std::endl;
  while((ret = rapp_contour_8conn_bin(origin,cont,1000,img, dim,width,height)) >= 0) {
    count += 1;
    //std::cout << "How many contours are found: " << ret << std::endl;
    ret = rapp_fill_8conn_bin(dest, dim,img, dim,width,height,origin[0],origin[1]);

    //std::cout << "Origin " << origin[0] << "," << origin[1] << std::endl;
    int sum = rapp_stat_sum_bin(dest, dim, width, height);
    if(sum == 0) {
      std::cout << "sum break  " << count << std::endl;
      delete[] box;
      box = nullptr;
      break;
    }
    ret = rapp_crop_box_bin(dest, dim,width,height,box);


    //std::cout << "Contour Box: " << box[0] << "," << box[1] << "," << box[2] <<","<< box[3] << "," << sum << std::endl;
    ret = rapp_bitblt_xor_bin(img, dim,0,dest, dim,0,width,height);
    //ret = rapp_pad_const_bin(img, dim, 0, width-20, height-20, 10, 0);
    if(filt(sum, box[2], box[3])) {
      std::cout << "connected break " << count << std::endl;
      std::cout << "Contour Box: " << box[0] << "," << box[1] << "," << box[2] <<","<< box[3] << "," << sum << std::endl;
      break;
    }
    sum = rapp_stat_sum_bin(img, dim, width, height);
    if(sum == 0) {
      std::cout << "sum break  " << count << std::endl;
      delete[] box;
      box = nullptr;
      break;
    }

    origin[0] = origin[1] = 0;
  }
  rapp_free(dest);
  //std::cout << rapp_error(ret) << std::endl;
  return box;
}

bool Filter::filt(int32_t sum, unsigned bwidth, unsigned bheight) {
  unsigned area = bwidth * bheight;
  uint8_t iout = sum * 100 / area;
  return bwidth > minSize_ && bheight > minSize_ && area > minArea_ && area < maxArea_ && iout > this->iou_;
}

////////////////////////////////////////////////////////////////////
Contour::Contour(uint8_t *org, uint32_t dim, uint32_t width, uint32_t height):dim_(dim), width_(width), height_(height) {
  assert(org != nullptr);

  img_ = static_cast<uint8_t*>(rapp_malloc(dim_ * height, 0));
  assert(img_ != nullptr);
  std::uninitialized_copy_n(org, dim_ * height, img_);
  //std::uninitialized_fill_n(img_, dim_, 0);
  //std::uninitialized_fill_n(img_+dim_*(height_-1), dim_, 0);

  bin_ = nullptr;
  dim8_ = 0;
  threshold_ = 0;
}

Contour::~Contour() {
  rapp_free(img_);
  freeBin();
}

void Contour::thresh_gt(unsigned thresh) {
  alignBin();
  assert(bin_ != nullptr);
  rapp_thresh_gt_u8(bin_, dim8_, img_, dim_, width_, height_, thresh);
  std::cout << dim8_ << "  " << thresh << std::endl;
  threshold_ = thresh;
}

void Contour::thresh_lt(unsigned thresh) {
  alignBin();
  assert(bin_ != nullptr);
  rapp_thresh_lt_u8(bin_, dim8_, img_, dim_, width_, height_, thresh);
  threshold_ = thresh;
}

Contour* Contour::search(Filter *filter) {
  assert(filter != nullptr);
  Contour *cont = nullptr;
  unsigned *box = filter->contour(bin_, dim8_, width_, height_);  //at least 4
  if(box != nullptr) {
    std::cout << "Contour Box for Crop: " << box[0] << "," << box[1] << "," << box[2] <<","<< box[3] << std::endl;
    uint8_t *img = cropByFill(box);
    cont = new Contour(img, dim_, width_, height_);
    rapp_free(img);
  }
  return cont;
}

uint8_t* Contour::cropByFill(unsigned box[4]) {
  uint32_t size = dim_ * height_;
  uint8_t *org = static_cast<uint8_t*>(rapp_malloc(size, 0));
  assert(org != nullptr);
  std::uninitialized_copy_n(img_, size, org);

  size = box[1] * dim_; //upper size of image
  std::uninitialized_fill_n(org, size, 255);  // y
  uint32_t off = size;
  uint32_t size_right = dim_ - box[0] - box[2];
  for(uint32_t i = 0; i < box[3]; ++i) {
    std::uninitialized_fill_n(org + off, box[0], 255);  //left part of image
    std::uninitialized_fill_n(org + off + box[0] + box[2], size_right, 255); //right part of image
    off += dim_; //next row
  }
  off = (box[1] + box[3]) * dim_; //lower part of image
  std::uninitialized_fill_n(org + off, dim_ * (height_ - box[1] - box[3]), 255); //lower of y+height

  return org;
}

void Contour::freeBin() {
  if(bin_ != nullptr)
    rapp_free(bin_);
  bin_ = nullptr;
}

void Contour::alignBin() {
  freeBin();
  uint32_t width8 = width_ % 8 == 0 ? 0 : 1;
  width8 += width_ / 8;
  dim8_ = (width8 % rapp_alignment == 0 ? 0 : 1) * rapp_alignment;
  dim8_ += width8;
  bin_ = static_cast<uint8_t*>(rapp_malloc(dim8_ * height_, 0));
}

void Contour::alignCenter(float *mapx, float *mapy, size_t srcw, size_t srch, size_t dstw, size_t dsth) {
  //align center of 2 images.
  for(size_t i = 0; i < dstw; ++i) {mapx[i] = (i + 0.5) * srcw / dstw - 0.5;}
  for(size_t i = 0; i < dsth; ++i) {mapy[i] = (i + 0.5) * srch / dsth - 0.5;}
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
    if (x_1 < 0 | x_2 > (nrows - 1) |  y_1 < 0 | y_2 > (ncols - 1)){
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
  //for testing
  float mx[10], my[10];
  alignCenter(mx, my, 5, 5, 10, 10);
  std::cout << mx[0] << " " << mx[1] << " " << mx[2] << std::endl;

  uint32_t size = width_ * height_, cn = 0;
  int* buf = new int[size];
  //std::uninitialized_fill_n(buf, size, 255);
  for(uint32_t i = 0; i < size; ++i) {
    if(img_[i] != 255) {
      buf[cn] = img_[i];
      cn += 1;
    }
  }
  std::cout << "Packed image size:" << cn << std::endl;
  return buf;
}

uint8_t* Contour::getData() {
  return img_;
}

void Contour::save(char file[]) {
  FILE *pf = fopen(file, "wb");
  fwrite(img_, 1, dim_*height_, pf);
  fclose(pf);
}

void Contour::save_bin(char file[]) {
  uint8_t *buf = static_cast<uint8_t*>(rapp_malloc(dim_ * height_, 0));
  uint8_t *buf1 = static_cast<uint8_t*>(rapp_malloc(dim8_ * height_, 0));
  rapp_bitblt_not_bin(buf1, dim8_, 0, bin_, dim8_, 0, width_, height_);
  FILE *pf = fopen(file, "wb");
  rapp_type_bin_to_u8(buf, dim_, bin_, dim8_, width_, height_);
  fwrite(buf, 1, dim_*height_, pf);
  fclose(pf);
  rapp_free(buf);
  rapp_free(buf1);
}
