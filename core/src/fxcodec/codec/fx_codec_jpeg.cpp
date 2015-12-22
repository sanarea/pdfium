// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <setjmp.h>

#include "codec_int.h"
#include "core/include/fxcodec/fx_codec.h"
#include "core/include/fxcrt/fx_safe_types.h"
#include "core/include/fxge/fx_dib.h"

extern "C" {
#undef FAR
#if defined(USE_SYSTEM_LIBJPEG)
#include <jpeglib.h>
#elif defined(USE_LIBJPEG_TURBO)
#include "third_party/libjpeg_turbo/jpeglib.h"
#else
#include "third_party/libjpeg/jpeglib.h"
#endif
}

extern "C" {
static void _JpegScanSOI(const uint8_t*& src_buf, FX_DWORD& src_size) {
  if (src_size == 0) {
    return;
  }
  FX_DWORD offset = 0;
  while (offset < src_size - 1) {
    if (src_buf[offset] == 0xff && src_buf[offset + 1] == 0xd8) {
      src_buf += offset;
      src_size -= offset;
      return;
    }
    offset++;
  }
}
};
extern "C" {
static void _src_do_nothing(struct jpeg_decompress_struct* cinfo) {}
};
extern "C" {
static void _error_fatal(j_common_ptr cinfo) {
  longjmp(*(jmp_buf*)cinfo->client_data, -1);
}
};
extern "C" {
static void _src_skip_data(struct jpeg_decompress_struct* cinfo, long num) {
  if (num > (long)cinfo->src->bytes_in_buffer) {
    _error_fatal((j_common_ptr)cinfo);
  }
  cinfo->src->next_input_byte += num;
  cinfo->src->bytes_in_buffer -= num;
}
};
extern "C" {
static boolean _src_fill_buffer(j_decompress_ptr cinfo) {
  return 0;
}
};
extern "C" {
static boolean _src_resync(j_decompress_ptr cinfo, int desired) {
  return 0;
}
};
extern "C" {
static void _error_do_nothing(j_common_ptr cinfo) {}
};
extern "C" {
static void _error_do_nothing1(j_common_ptr cinfo, int) {}
};
extern "C" {
static void _error_do_nothing2(j_common_ptr cinfo, char*) {}
};

#define JPEG_MARKER_ICC (JPEG_APP0 + 2)
#define JPEG_MARKER_MAXSIZE 0xFFFF

static bool JpegLoadInfo(const uint8_t* src_buf,
                         FX_DWORD src_size,
                         int* width,
                         int* height,
                         int* num_components,
                         int* bits_per_components,
                         bool* color_transform) {
  _JpegScanSOI(src_buf, src_size);
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  jerr.error_exit = _error_fatal;
  jerr.emit_message = _error_do_nothing1;
  jerr.output_message = _error_do_nothing;
  jerr.format_message = _error_do_nothing2;
  jerr.reset_error_mgr = _error_do_nothing;
  jerr.trace_level = 0;
  cinfo.err = &jerr;
  jmp_buf mark;
  cinfo.client_data = &mark;
  if (setjmp(mark) == -1) {
    return false;
  }
  jpeg_create_decompress(&cinfo);
  struct jpeg_source_mgr src;
  src.init_source = _src_do_nothing;
  src.term_source = _src_do_nothing;
  src.skip_input_data = _src_skip_data;
  src.fill_input_buffer = _src_fill_buffer;
  src.resync_to_restart = _src_resync;
  src.bytes_in_buffer = src_size;
  src.next_input_byte = src_buf;
  cinfo.src = &src;
  if (setjmp(mark) == -1) {
    jpeg_destroy_decompress(&cinfo);
    return false;
  }
  int ret = jpeg_read_header(&cinfo, TRUE);
  if (ret != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&cinfo);
    return false;
  }
  *width = cinfo.image_width;
  *height = cinfo.image_height;
  *num_components = cinfo.num_components;
  *color_transform =
      cinfo.jpeg_color_space == JCS_YCbCr || cinfo.jpeg_color_space == JCS_YCCK;
  *bits_per_components = cinfo.data_precision;
  jpeg_destroy_decompress(&cinfo);
  return true;
}

class CCodec_JpegDecoder : public CCodec_ScanlineDecoder {
 public:
  CCodec_JpegDecoder();
  ~CCodec_JpegDecoder() override;

  FX_BOOL Create(const uint8_t* src_buf,
                 FX_DWORD src_size,
                 int width,
                 int height,
                 int nComps,
                 FX_BOOL ColorTransform);
  void Destroy() { delete this; }

  // CCodec_ScanlineDecoder
  void v_DownScale(int dest_width, int dest_height) override;
  FX_BOOL v_Rewind() override;
  uint8_t* v_GetNextLine() override;
  FX_DWORD GetSrcOffset() override;

  FX_BOOL InitDecode();

  jmp_buf m_JmpBuf;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  struct jpeg_source_mgr src;
  const uint8_t* m_SrcBuf;
  FX_DWORD m_SrcSize;
  uint8_t* m_pScanlineBuf;

  FX_BOOL m_bInited;
  FX_BOOL m_bStarted;
  FX_BOOL m_bJpegTransform;

 protected:
  FX_DWORD m_nDefaultScaleDenom;
};

CCodec_JpegDecoder::CCodec_JpegDecoder() {
  m_pScanlineBuf = NULL;
  m_DownScale = 1;
  m_bStarted = FALSE;
  m_bInited = FALSE;
  FXSYS_memset(&cinfo, 0, sizeof(cinfo));
  FXSYS_memset(&jerr, 0, sizeof(jerr));
  FXSYS_memset(&src, 0, sizeof(src));
  m_nDefaultScaleDenom = 1;
}
CCodec_JpegDecoder::~CCodec_JpegDecoder() {
  FX_Free(m_pScanlineBuf);
  if (m_bInited) {
    jpeg_destroy_decompress(&cinfo);
  }
}
FX_BOOL CCodec_JpegDecoder::InitDecode() {
  cinfo.err = &jerr;
  cinfo.client_data = &m_JmpBuf;
  if (setjmp(m_JmpBuf) == -1) {
    return FALSE;
  }
  jpeg_create_decompress(&cinfo);
  m_bInited = TRUE;
  cinfo.src = &src;
  src.bytes_in_buffer = m_SrcSize;
  src.next_input_byte = m_SrcBuf;
  if (setjmp(m_JmpBuf) == -1) {
    jpeg_destroy_decompress(&cinfo);
    m_bInited = FALSE;
    return FALSE;
  }
  cinfo.image_width = m_OrigWidth;
  cinfo.image_height = m_OrigHeight;
  int ret = jpeg_read_header(&cinfo, TRUE);
  if (ret != JPEG_HEADER_OK) {
    return FALSE;
  }
  if (cinfo.saw_Adobe_marker) {
    m_bJpegTransform = TRUE;
  }
  if (cinfo.num_components == 3 && !m_bJpegTransform) {
    cinfo.out_color_space = cinfo.jpeg_color_space;
  }
  m_OrigWidth = cinfo.image_width;
  m_OrigHeight = cinfo.image_height;
  m_OutputWidth = m_OrigWidth;
  m_OutputHeight = m_OrigHeight;
  m_nDefaultScaleDenom = cinfo.scale_denom;
  return TRUE;
}
FX_BOOL CCodec_JpegDecoder::Create(const uint8_t* src_buf,
                                   FX_DWORD src_size,
                                   int width,
                                   int height,
                                   int nComps,
                                   FX_BOOL ColorTransform) {
  _JpegScanSOI(src_buf, src_size);
  m_SrcBuf = src_buf;
  m_SrcSize = src_size;
  jerr.error_exit = _error_fatal;
  jerr.emit_message = _error_do_nothing1;
  jerr.output_message = _error_do_nothing;
  jerr.format_message = _error_do_nothing2;
  jerr.reset_error_mgr = _error_do_nothing;
  src.init_source = _src_do_nothing;
  src.term_source = _src_do_nothing;
  src.skip_input_data = _src_skip_data;
  src.fill_input_buffer = _src_fill_buffer;
  src.resync_to_restart = _src_resync;
  m_bJpegTransform = ColorTransform;
  if (src_size > 1 &&
      FXSYS_memcmp(src_buf + src_size - 2, "\xFF\xD9", 2) != 0) {
    ((uint8_t*)src_buf)[src_size - 2] = 0xFF;
    ((uint8_t*)src_buf)[src_size - 1] = 0xD9;
  }
  m_OutputWidth = m_OrigWidth = width;
  m_OutputHeight = m_OrigHeight = height;
  if (!InitDecode()) {
    return FALSE;
  }
  if (cinfo.num_components < nComps) {
    return FALSE;
  }
  if ((int)cinfo.image_width < width) {
    return FALSE;
  }
  m_Pitch =
      (static_cast<FX_DWORD>(cinfo.image_width) * cinfo.num_components + 3) /
      4 * 4;
  m_pScanlineBuf = FX_Alloc(uint8_t, m_Pitch);
  m_nComps = cinfo.num_components;
  m_bpc = 8;
  m_bColorTransformed = FALSE;
  m_bStarted = FALSE;
  return TRUE;
}
extern "C" {
int32_t FX_GetDownsampleRatio(int32_t originWidth,
                              int32_t originHeight,
                              int32_t downsampleWidth,
                              int32_t downsampleHeight) {
  int iratio_w = originWidth / downsampleWidth;
  int iratio_h = originHeight / downsampleHeight;
  int ratio = (iratio_w > iratio_h) ? iratio_h : iratio_w;
  if (ratio >= 8) {
    return 8;
  }
  if (ratio >= 4) {
    return 4;
  }
  if (ratio >= 2) {
    return 2;
  }
  return 1;
}
}
void CCodec_JpegDecoder::v_DownScale(int dest_width, int dest_height) {
  int old_scale = m_DownScale;
  m_DownScale =
      FX_GetDownsampleRatio(m_OrigWidth, m_OrigHeight, dest_width, dest_height);
  m_OutputWidth = (m_OrigWidth + m_DownScale - 1) / m_DownScale;
  m_OutputHeight = (m_OrigHeight + m_DownScale - 1) / m_DownScale;
  m_Pitch = (static_cast<FX_DWORD>(m_OutputWidth) * m_nComps + 3) / 4 * 4;
  if (old_scale != m_DownScale) {
    m_NextLine = -1;
  }
}
FX_BOOL CCodec_JpegDecoder::v_Rewind() {
  if (m_bStarted) {
    jpeg_destroy_decompress(&cinfo);
    if (!InitDecode()) {
      return FALSE;
    }
  }
  if (setjmp(m_JmpBuf) == -1) {
    return FALSE;
  }
  cinfo.scale_denom = m_nDefaultScaleDenom * m_DownScale;
  m_OutputWidth = (m_OrigWidth + m_DownScale - 1) / m_DownScale;
  m_OutputHeight = (m_OrigHeight + m_DownScale - 1) / m_DownScale;
  if (!jpeg_start_decompress(&cinfo)) {
    jpeg_destroy_decompress(&cinfo);
    return FALSE;
  }
  if ((int)cinfo.output_width > m_OrigWidth) {
    FXSYS_assert(FALSE);
    return FALSE;
  }
  m_bStarted = TRUE;
  return TRUE;
}
uint8_t* CCodec_JpegDecoder::v_GetNextLine() {
  if (setjmp(m_JmpBuf) == -1)
    return nullptr;

  int nlines = jpeg_read_scanlines(&cinfo, &m_pScanlineBuf, 1);
  if (nlines < 1) {
    return nullptr;
  }
  return m_pScanlineBuf;
}
FX_DWORD CCodec_JpegDecoder::GetSrcOffset() {
  return (FX_DWORD)(m_SrcSize - src.bytes_in_buffer);
}
ICodec_ScanlineDecoder* CCodec_JpegModule::CreateDecoder(
    const uint8_t* src_buf,
    FX_DWORD src_size,
    int width,
    int height,
    int nComps,
    FX_BOOL ColorTransform) {
  if (!src_buf || src_size == 0) {
    return NULL;
  }
  CCodec_JpegDecoder* pDecoder = new CCodec_JpegDecoder;
  if (!pDecoder->Create(src_buf, src_size, width, height, nComps,
                        ColorTransform)) {
    delete pDecoder;
    return NULL;
  }
  return pDecoder;
}
bool CCodec_JpegModule::LoadInfo(const uint8_t* src_buf,
                                 FX_DWORD src_size,
                                 int* width,
                                 int* height,
                                 int* num_components,
                                 int* bits_per_components,
                                 bool* color_transform) {
  return JpegLoadInfo(src_buf, src_size, width, height, num_components,
                      bits_per_components, color_transform);
}

struct FXJPEG_Context {
  jmp_buf m_JumpMark;
  jpeg_decompress_struct m_Info;
  jpeg_error_mgr m_ErrMgr;
  jpeg_source_mgr m_SrcMgr;
  unsigned int m_SkipSize;
  void* (*m_AllocFunc)(unsigned int);
  void (*m_FreeFunc)(void*);
};
extern "C" {
static void _error_fatal1(j_common_ptr cinfo) {
  longjmp(((FXJPEG_Context*)cinfo->client_data)->m_JumpMark, -1);
}
};
extern "C" {
static void _src_skip_data1(struct jpeg_decompress_struct* cinfo, long num) {
  if (cinfo->src->bytes_in_buffer < (size_t)num) {
    ((FXJPEG_Context*)cinfo->client_data)->m_SkipSize =
        (unsigned int)(num - cinfo->src->bytes_in_buffer);
    cinfo->src->bytes_in_buffer = 0;
  } else {
    cinfo->src->next_input_byte += num;
    cinfo->src->bytes_in_buffer -= num;
  }
}
};
static void* jpeg_alloc_func(unsigned int size) {
  return FX_Alloc(char, size);
}
static void jpeg_free_func(void* p) {
  FX_Free(p);
}
void* CCodec_JpegModule::Start() {
  FXJPEG_Context* p = FX_Alloc(FXJPEG_Context, 1);
  p->m_AllocFunc = jpeg_alloc_func;
  p->m_FreeFunc = jpeg_free_func;
  p->m_ErrMgr.error_exit = _error_fatal1;
  p->m_ErrMgr.emit_message = _error_do_nothing1;
  p->m_ErrMgr.output_message = _error_do_nothing;
  p->m_ErrMgr.format_message = _error_do_nothing2;
  p->m_ErrMgr.reset_error_mgr = _error_do_nothing;
  p->m_SrcMgr.init_source = _src_do_nothing;
  p->m_SrcMgr.term_source = _src_do_nothing;
  p->m_SrcMgr.skip_input_data = _src_skip_data1;
  p->m_SrcMgr.fill_input_buffer = _src_fill_buffer;
  p->m_SrcMgr.resync_to_restart = _src_resync;
  p->m_Info.client_data = p;
  p->m_Info.err = &p->m_ErrMgr;
  if (setjmp(p->m_JumpMark) == -1) {
    return 0;
  }
  jpeg_create_decompress(&p->m_Info);
  p->m_Info.src = &p->m_SrcMgr;
  p->m_SkipSize = 0;
  return p;
}
void CCodec_JpegModule::Finish(void* pContext) {
  FXJPEG_Context* p = (FXJPEG_Context*)pContext;
  jpeg_destroy_decompress(&p->m_Info);
  p->m_FreeFunc(p);
}
void CCodec_JpegModule::Input(void* pContext,
                              const unsigned char* src_buf,
                              FX_DWORD src_size) {
  FXJPEG_Context* p = (FXJPEG_Context*)pContext;
  if (p->m_SkipSize) {
    if (p->m_SkipSize > src_size) {
      p->m_SrcMgr.bytes_in_buffer = 0;
      p->m_SkipSize -= src_size;
      return;
    }
    src_size -= p->m_SkipSize;
    src_buf += p->m_SkipSize;
    p->m_SkipSize = 0;
  }
  p->m_SrcMgr.next_input_byte = src_buf;
  p->m_SrcMgr.bytes_in_buffer = src_size;
}
int CCodec_JpegModule::ReadHeader(void* pContext,
                                  int* width,
                                  int* height,
                                  int* nComps) {
  FXJPEG_Context* p = (FXJPEG_Context*)pContext;
  if (setjmp(p->m_JumpMark) == -1) {
    return 1;
  }
  int ret = jpeg_read_header(&p->m_Info, true);
  if (ret == JPEG_SUSPENDED) {
    return 2;
  }
  if (ret != JPEG_HEADER_OK) {
    return 1;
  }
  *width = p->m_Info.image_width;
  *height = p->m_Info.image_height;
  *nComps = p->m_Info.num_components;
  return 0;
}
int CCodec_JpegModule::StartScanline(void* pContext, int down_scale) {
  FXJPEG_Context* p = (FXJPEG_Context*)pContext;
  if (setjmp(p->m_JumpMark) == -1) {
    return 0;
  }
  p->m_Info.scale_denom = down_scale;
  return jpeg_start_decompress(&p->m_Info);
}
FX_BOOL CCodec_JpegModule::ReadScanline(void* pContext,
                                        unsigned char* dest_buf) {
  FXJPEG_Context* p = (FXJPEG_Context*)pContext;
  if (setjmp(p->m_JumpMark) == -1) {
    return FALSE;
  }
  int nlines = jpeg_read_scanlines(&p->m_Info, &dest_buf, 1);
  return nlines == 1;
}
FX_DWORD CCodec_JpegModule::GetAvailInput(void* pContext,
                                          uint8_t** avail_buf_ptr) {
  if (avail_buf_ptr) {
    *avail_buf_ptr = NULL;
    if (((FXJPEG_Context*)pContext)->m_SrcMgr.bytes_in_buffer > 0) {
      *avail_buf_ptr =
          (uint8_t*)((FXJPEG_Context*)pContext)->m_SrcMgr.next_input_byte;
    }
  }
  return (FX_DWORD)((FXJPEG_Context*)pContext)->m_SrcMgr.bytes_in_buffer;
}
