// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA version of dsp functions
//
// Author(s):  Prashant Patil   (prashant.patil@imgtec.com)


#include "./dsp.h"

#if defined(WEBP_USE_MSA)

#include "./msa_macro.h"

//------------------------------------------------------------------------------
// Transforms

#define IDCT_1D_W(in0, in1, in2, in3, out0, out1, out2, out3) {  \
  v4i32 a1_m, b1_m, c1_m, d1_m;                                  \
  v4i32 c_tmp1_m, c_tmp2_m, d_tmp1_m, d_tmp2_m;                  \
  const v4i32 cospi8sqrt2minus1 = __msa_fill_w(20091);           \
  const v4i32 sinpi8sqrt2 = __msa_fill_w(35468);                 \
                                                                 \
  a1_m = in0 + in2;                                              \
  b1_m = in0 - in2;                                              \
  c_tmp1_m = (in1 * sinpi8sqrt2) >> 16;                          \
  c_tmp2_m = in3 + ((in3 * cospi8sqrt2minus1) >> 16);            \
  c1_m = c_tmp1_m - c_tmp2_m;                                    \
  d_tmp1_m = in1 + ((in1 * cospi8sqrt2minus1) >> 16);            \
  d_tmp2_m = (in3 * sinpi8sqrt2) >> 16;                          \
  d1_m = d_tmp1_m + d_tmp2_m;                                    \
  BUTTERFLY_4(a1_m, b1_m, c1_m, d1_m, out0, out1, out2, out3);   \
}
#define MULT1(a) ((((a) * 20091) >> 16) + (a))
#define MULT2(a) (((a) * 35468) >> 16)

static void TransformOne(const int16_t* in, uint8_t* dst) {
  v8i16 input0, input1;
  v4i32 in0, in1, in2, in3, hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3;
  v4i32 res0, res1, res2, res3;
  const v16i8 zero = { 0 };
  v16i8 dest0, dest1, dest2, dest3;

  LD_SH2(in, 8, input0, input1);
  UNPCK_SH_SW(input0, in0, in1);
  UNPCK_SH_SW(input1, in2, in3);
  IDCT_1D_W(in0, in1, in2, in3, hz0, hz1, hz2, hz3);
  TRANSPOSE4x4_SW_SW(hz0, hz1, hz2, hz3, hz0, hz1, hz2, hz3);
  IDCT_1D_W(hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3);
  SRARI_W4_SW(vt0, vt1, vt2, vt3, 3);
  TRANSPOSE4x4_SW_SW(vt0, vt1, vt2, vt3, vt0, vt1, vt2, vt3);
  LD_SB4(dst, BPS, dest0, dest1, dest2, dest3);
  ILVR_B4_SW(zero, dest0, zero, dest1, zero, dest2, zero, dest3,
             res0, res1, res2, res3);
  ILVR_H4_SW(zero, res0, zero, res1, zero, res2, zero, res3,
             res0, res1, res2, res3);
  ADD4(res0, vt0, res1, vt1, res2, vt2, res3, vt3, res0, res1, res2, res3);
  CLIP_SW4_0_255(res0, res1, res2, res3);
  PCKEV_B2_SW(res0, res1, res2, res3, vt0, vt1);
  res0 = (v4i32)__msa_pckev_b((v16i8)vt0, (v16i8)vt1);
  ST4x4_UB(res0, res0, 3, 2, 1, 0, dst, BPS);
}

static void TransformTwo(const int16_t* in, uint8_t* dst, int do_two) {
  TransformOne(in, dst);
  if (do_two) {
    TransformOne(in + 16, dst + 4);
  }
}

static void TransformWHT(const int16_t* in, int16_t* out) {
  v8i16 input0, input1;
  const v8i16 mask0 = { 0, 1, 2, 3, 8, 9, 10, 11 };
  const v8i16 mask1 = { 4, 5, 6, 7, 12, 13, 14, 15 };
  const v8i16 mask2 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask3 = { 3, 7, 11, 15, 2, 6, 10, 14 };
  v8i16 tmp0, tmp1, tmp2, tmp3;
  v8i16 out0, out1;

  LD_SH2(in, 8, input0, input1);
  input1 = SLDI_SH(input1, input1, 8);
  tmp0 = input0 + input1;
  tmp1 = input0 - input1;
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  out0 = tmp2 + tmp3;
  out1 = tmp2 - tmp3;
  VSHF_H2_SH(out0, out1, out0, out1, mask2, mask3, input0, input1);
  tmp0 = input0 + input1;
  tmp1 = input0 - input1;
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  tmp0 = tmp2 + tmp3;
  tmp1 = tmp2 - tmp3;
  ADDVI_H2_SH(tmp0, 3, tmp1, 3, out0, out1);
  SRAI_H2_SH(out0, out1, 3);
  out[0] = __msa_copy_s_h(out0, 0);
  out[16] = __msa_copy_s_h(out0, 4);
  out[32] = __msa_copy_s_h(out1, 0);
  out[48] = __msa_copy_s_h(out1, 4);
  out[64] = __msa_copy_s_h(out0, 1);
  out[80] = __msa_copy_s_h(out0, 5);
  out[96] = __msa_copy_s_h(out1, 1);
  out[112] = __msa_copy_s_h(out1, 5);
  out[128] = __msa_copy_s_h(out0, 2);
  out[144] = __msa_copy_s_h(out0, 6);
  out[160] = __msa_copy_s_h(out1, 2);
  out[176] = __msa_copy_s_h(out1, 6);
  out[192] = __msa_copy_s_h(out0, 3);
  out[208] = __msa_copy_s_h(out0, 7);
  out[224] = __msa_copy_s_h(out1, 3);
  out[240] = __msa_copy_s_h(out1, 7);
}

static void TransformDC(const int16_t* in, uint8_t* dst) {
  const int DC = (in[0] + 4) >> 3;
  const v8i16 tmp0 = __msa_fill_h(DC);
  ADDBLK_ST4x4_UB(tmp0, tmp0, tmp0, tmp0, dst, BPS);
}

static void TransformAC3(const int16_t* in, uint8_t* dst) {
  const int a = in[0] + 4;
  const int c4 = MULT2(in[4]);
  const int d4 = MULT1(in[4]);
  const int in2 = MULT2(in[1]);
  const int in3 = MULT1(in[1]);
  v4i32 tmp0 = { 0 };
  v4i32 out0 = __msa_fill_w(a + d4);
  v4i32 out1 = __msa_fill_w(a + c4);
  v4i32 out2 = __msa_fill_w(a - c4);
  v4i32 out3 = __msa_fill_w(a - d4);
  v4i32 res0, res1, res2, res3;
  const v4i32 zero = { 0 };
  v16u8 dest0, dest1, dest2, dest3;

  INSERT_W4_SW(in3, in2, -in2, -in3, tmp0);
  ADD4(out0, tmp0, out1, tmp0, out2, tmp0, out3, tmp0,
       out0, out1, out2, out3);
  SRAI_W4_SW(out0, out1, out2, out3, 3);
  LD_UB4(dst, BPS, dest0, dest1, dest2, dest3);
  ILVR_B4_SW(zero, dest0, zero, dest1, zero, dest2, zero, dest3,
             res0, res1, res2, res3);
  ILVR_H4_SW(zero, res0, zero, res1, zero, res2, zero, res3,
             res0, res1, res2, res3);
  ADD4(res0, out0, res1, out1, res2, out2, res3, out3, res0, res1, res2, res3);
  CLIP_SW4_0_255(res0, res1, res2, res3);
  PCKEV_B2_SW(res0, res1, res2, res3, out0, out1);
  res0 = (v4i32)__msa_pckev_b((v16i8)out0, (v16i8)out1);
  ST4x4_UB(res0, res0, 3, 2, 1, 0, dst, BPS);
}

//------------------------------------------------------------------------------
// Edge filtering functions

#define FLIP_SIGN2(in0, in1, out0, out1) {  \
  out0 = (v16i8)__msa_xori_b(in0, 0x80);    \
  out1 = (v16i8)__msa_xori_b(in1, 0x80);    \
}

#define FLIP_SIGN4(in0, in1, in2, in3, out0, out1, out2, out3) {  \
  FLIP_SIGN2(in0, in1, out0, out1);                               \
  FLIP_SIGN2(in2, in3, out2, out3);                               \
}

#define FILT_VAL(q0_m, p0_m, mask, filt) do {  \
  v16i8 q0_sub_p0;                             \
  q0_sub_p0 = __msa_subs_s_b(q0_m, p0_m);      \
  filt = __msa_adds_s_b(filt, q0_sub_p0);      \
  filt = __msa_adds_s_b(filt, q0_sub_p0);      \
  filt = __msa_adds_s_b(filt, q0_sub_p0);      \
  filt = filt & mask;                          \
} while (0)

#define FILT2(q_m, p_m, q, p) do {            \
  u_r = SRAI_H(temp1, 7);                     \
  u_r = __msa_sat_s_h(u_r, 7);                \
  u_l = SRAI_H(temp3, 7);                     \
  u_l = __msa_sat_s_h(u_l, 7);                \
  u = __msa_pckev_b((v16i8)u_l, (v16i8)u_r);  \
  q_m = __msa_subs_s_b(q_m, u);               \
  p_m = __msa_adds_s_b(p_m, u);               \
  q = __msa_xori_b((v16u8)q_m, 0x80);         \
  p = __msa_xori_b((v16u8)p_m, 0x80);         \
} while (0)

#define LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev) do {  \
  v16i8 p1_m, p0_m, q0_m, q1_m;                         \
  v16i8 filt, t1, t2;                                   \
  const v16i8 cnst4b = __msa_ldi_b(4);                  \
  const v16i8 cnst3b = __msa_ldi_b(3);                  \
                                                        \
  FLIP_SIGN4(p1, p0, q0, q1, p1_m, p0_m, q0_m, q1_m);   \
  filt = __msa_subs_s_b(p1_m, q1_m);                    \
  filt = filt & hev;                                    \
  FILT_VAL(q0_m, p0_m, mask, filt);                     \
  t1 = __msa_adds_s_b(filt, cnst4b);                    \
  t1 = SRAI_B(t1, 3);                                   \
  t2 = __msa_adds_s_b(filt, cnst3b);                    \
  t2 = SRAI_B(t2, 3);                                   \
  q0_m = __msa_subs_s_b(q0_m, t1);                      \
  q0 = __msa_xori_b((v16u8)q0_m, 0x80);                 \
  p0_m = __msa_adds_s_b(p0_m, t2);                      \
  p0 = __msa_xori_b((v16u8)p0_m, 0x80);                 \
  filt = __msa_srari_b(t1, 1);                          \
  hev = __msa_xori_b(hev, 0xff);                        \
  filt = filt & hev;                                    \
  q1_m = __msa_subs_s_b(q1_m, filt);                    \
  q1 = __msa_xori_b((v16u8)q1_m, 0x80);                 \
  p1_m = __msa_adds_s_b(p1_m, filt);                    \
  p1 = __msa_xori_b((v16u8)p1_m, 0x80);                 \
} while (0)

#define LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev) do {  \
  v16i8 p2_m, p1_m, p0_m, q2_m, q1_m, q0_m;                   \
  v16i8 u, filt, t1, t2, filt_sign;                           \
  v8i16 filt_r, filt_l, u_r, u_l;                             \
  v8i16 temp0, temp1, temp2, temp3;                           \
  const v16i8 cnst4b = __msa_ldi_b(4);                        \
  const v16i8 cnst3b = __msa_ldi_b(3);                        \
  const v8i16 cnst9h = __msa_ldi_h(9);                        \
                                                              \
  FLIP_SIGN4(p1, p0, q0, q1, p1_m, p0_m, q0_m, q1_m);         \
  filt = __msa_subs_s_b(p1_m, q1_m);                          \
  FILT_VAL(q0_m, p0_m, mask, filt);                           \
  FLIP_SIGN2(p2, q2, p2_m, q2_m);                             \
  t2 = filt & hev;                                            \
  /* filt_val &= ~hev */                                      \
  hev = __msa_xori_b(hev, 0xff);                              \
  filt = filt & hev;                                          \
  t1 = __msa_adds_s_b(t2, cnst4b);                            \
  t1 = SRAI_B(t1, 3);                                         \
  t2 = __msa_adds_s_b(t2, cnst3b);                            \
  t2 = SRAI_B(t2, 3);                                         \
  q0_m = __msa_subs_s_b(q0_m, t1);                            \
  p0_m = __msa_adds_s_b(p0_m, t2);                            \
  filt_sign = __msa_clti_s_b(filt, 0);                        \
  ILVRL_B2_SH(filt_sign, filt, filt_r, filt_l);               \
  /* update q2/p2 */                                          \
  temp0 = filt_r * cnst9h;                                    \
  temp1 = ADDVI_H(temp0, 63);                                 \
  temp2 = filt_l * cnst9h;                                    \
  temp3 = ADDVI_H(temp2, 63);                                 \
  FILT2(q2_m, p2_m, q2, p2);                                  \
  /* update q1/p1 */                                          \
  temp1 = temp1 + temp0;                                      \
  temp3 = temp3 + temp2;                                      \
  FILT2(q1_m, p1_m, q1, p1);                                  \
  /* update q0/p0 */                                          \
  temp1 = temp1 + temp0;                                      \
  temp3 = temp3 + temp2;                                      \
  FILT2(q0_m, p0_m, q0, p0);                                  \
} while (0)

#define LPF_MASK_HEV(p3_in, p2_in, p1_in, p0_in,                 \
                     q0_in, q1_in, q2_in, q3_in,                 \
                     limit_in, b_limit_in, thresh_in,            \
                     hev_out, mask_out) do {                     \
  v16u8 p3_asub_p2_m, p2_asub_p1_m, p1_asub_p0_m, q1_asub_q0_m;  \
  v16u8 p1_asub_q1_m, p0_asub_q0_m, q3_asub_q2_m, q2_asub_q1_m;  \
  v16u8 flat_out;                                                \
                                                                 \
  /* absolute subtraction of pixel values */                     \
  p3_asub_p2_m = __msa_asub_u_b(p3_in, p2_in);                   \
  p2_asub_p1_m = __msa_asub_u_b(p2_in, p1_in);                   \
  p1_asub_p0_m = __msa_asub_u_b(p1_in, p0_in);                   \
  q1_asub_q0_m = __msa_asub_u_b(q1_in, q0_in);                   \
  q2_asub_q1_m = __msa_asub_u_b(q2_in, q1_in);                   \
  q3_asub_q2_m = __msa_asub_u_b(q3_in, q2_in);                   \
  p0_asub_q0_m = __msa_asub_u_b(p0_in, q0_in);                   \
  p1_asub_q1_m = __msa_asub_u_b(p1_in, q1_in);                   \
  /* calculation of hev */                                       \
  flat_out = __msa_max_u_b(p1_asub_p0_m, q1_asub_q0_m);          \
  hev_out = (thresh_in < flat_out);                              \
  /* calculation of mask */                                      \
  p0_asub_q0_m = __msa_adds_u_b(p0_asub_q0_m, p0_asub_q0_m);     \
  p1_asub_q1_m = SRAI_B(p1_asub_q1_m, 1);                        \
  p0_asub_q0_m = __msa_adds_u_b(p0_asub_q0_m, p1_asub_q1_m);     \
  mask_out = (b_limit_in < p0_asub_q0_m);                        \
  mask_out = __msa_max_u_b(flat_out, mask_out);                  \
  p3_asub_p2_m = __msa_max_u_b(p3_asub_p2_m, p2_asub_p1_m);      \
  mask_out = __msa_max_u_b(p3_asub_p2_m, mask_out);              \
  q2_asub_q1_m = __msa_max_u_b(q2_asub_q1_m, q3_asub_q2_m);      \
  mask_out = __msa_max_u_b(q2_asub_q1_m, mask_out);              \
  mask_out = (limit_in < mask_out);                              \
  mask_out = __msa_xori_b(mask_out, 0xff);                       \
} while (0)

#define ST6x1_UB(in0, in0_idx, in1, in1_idx, pdst, stride) {    \
  const uint16_t tmp0_h = __msa_copy_s_h((v8i16)in1, in1_idx);  \
  const uint32_t tmp0_w = __msa_copy_s_w((v4i32)in0, in0_idx);  \
  SW(tmp0_w, pdst);                                             \
  SH(tmp0_h, pdst + stride);                                    \
}

static void VFilter16(uint8_t *src, int stride,
                      int b_limit_in, int limit_in, int thresh_in) {
  uint8_t *ptemp = src - 4 * stride;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  v16u8 mask, hev;
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);

  LD_UB8(ptemp, stride, p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev);
  ptemp = src - 3 * stride;
  ST_UB4(p2, p1, p0, q0, ptemp, stride);
  ptemp += (4 * stride);
  ST_UB2(q1, q2, ptemp, stride);
}

static void HFilter16(uint8_t *src, int stride,
                      int b_limit_in, int limit_in, int thresh_in) {
  uint8_t *ptmp  = src - 4;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  v16u8 mask, hev;
  v16u8 row0, row1, row2, row3, row4, row5, row6, row7, row8;
  v16u8 row9, row10, row11, row12, row13, row14, row15;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);

  LD_UB8(ptmp, stride, row0, row1, row2, row3, row4, row5, row6, row7);
  ptmp += (8 * stride);
  LD_UB8(ptmp, stride, row8, row9, row10, row11, row12, row13, row14, row15);
  TRANSPOSE16x8_UB_UB(row0, row1, row2, row3, row4, row5, row6, row7,
                      row8, row9, row10, row11, row12, row13, row14, row15,
                      p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev);
  ILVR_B2_SH(p1, p2, q0, p0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp3, tmp4);
  ILVL_B2_SH(p1, p2, q0, p0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp6, tmp7);
  ILVRL_B2_SH(q2, q1, tmp2, tmp5);
  ptmp = src - 3;
  ST6x1_UB(tmp3, 0, tmp2, 0, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp3, 1, tmp2, 1, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp3, 2, tmp2, 2, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp3, 3, tmp2, 3, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 0, tmp2, 4, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 1, tmp2, 5, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 2, tmp2, 6, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 3, tmp2, 7, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 0, tmp5, 0, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 1, tmp5, 1, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 2, tmp5, 2, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 3, tmp5, 3, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 0, tmp5, 4, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 1, tmp5, 5, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 2, tmp5, 6, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 3, tmp5, 7, ptmp, 4);
}

// on three inner edges
static void VFilterHorEdge16i(uint8_t *src, int stride,
                              int b_limit, int limit, int thresh) {
  v16u8 mask, hev;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  const v16u8 thresh0 = (v16u8)__msa_fill_b(thresh);
  const v16u8 b_limit0 = (v16u8)__msa_fill_b(b_limit);
  const v16u8 limit0 = (v16u8)__msa_fill_b(limit);

  LD_UB8((src - 4 * stride), stride, p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit0, b_limit0, thresh0,
               hev, mask);
  LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev);
  ST_UB4(p1, p0, q0, q1, (src - 2 * stride), stride);
}

static void VFilter16i(uint8_t *src_y, int stride,
                       int b_limit, int limit, int thresh) {
  VFilterHorEdge16i(src_y + 4 * stride, stride, b_limit, limit, thresh);
  VFilterHorEdge16i(src_y + 8 * stride, stride, b_limit, limit, thresh);
  VFilterHorEdge16i(src_y + 12 * stride, stride, b_limit, limit, thresh);
}

static void HFilterVertEdge16i(uint8_t *src, int stride,
                               int b_limit, int limit, int thresh) {
  v16u8 mask, hev;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  v16u8 row0, row1, row2, row3, row4, row5, row6, row7;
  v16u8 row8, row9, row10, row11, row12, row13, row14, row15;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
  const v16u8 thresh0 = (v16u8)__msa_fill_b(thresh);
  const v16u8 b_limit0 = (v16u8)__msa_fill_b(b_limit);
  const v16u8 limit0 = (v16u8)__msa_fill_b(limit);

  LD_UB8(src - 4, stride, row0, row1, row2, row3, row4, row5, row6, row7);
  LD_UB8(src - 4 + (8 * stride), stride,
         row8, row9, row10, row11, row12, row13, row14, row15);
  TRANSPOSE16x8_UB_UB(row0, row1, row2, row3, row4, row5, row6, row7,
                      row8, row9, row10, row11, row12, row13, row14, row15,
                      p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit0, b_limit0, thresh0,
               hev, mask);
  LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev);
  ILVR_B2_SH(p0, p1, q1, q0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp2, tmp3);
  ILVL_B2_SH(p0, p1, q1, q0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp4, tmp5);
  src -= 2;
  ST4x8_UB(tmp2, tmp3, src, stride);
  src += (8 * stride);
  ST4x8_UB(tmp4, tmp5, src, stride);
}

static void HFilter16i(uint8_t *src_y, int stride,
                       int b_limit, int limit, int thresh) {
  HFilterVertEdge16i(src_y + 4, stride, b_limit, limit, thresh);
  HFilterVertEdge16i(src_y + 8, stride, b_limit, limit, thresh);
  HFilterVertEdge16i(src_y + 12, stride, b_limit, limit, thresh);
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitMSA(void) {
  VP8TransformWHT = TransformWHT;
  VP8Transform = TransformTwo;
  VP8TransformDC = TransformDC;
  VP8TransformAC3 = TransformAC3;

  VP8VFilter16 = VFilter16;
  VP8HFilter16 = HFilter16;
  VP8VFilter16i = VFilter16i;
  VP8HFilter16i = HFilter16i;
}

#else  // !WEBP_USE_MSA

WEBP_DSP_INIT_STUB(VP8DspInitMSA)

#endif  // WEBP_USE_MSA
