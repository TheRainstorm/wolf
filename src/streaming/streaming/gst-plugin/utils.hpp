#pragma once

extern "C" {
#include <moonlight-common-c/reedsolomon/rs.h>
}

#include <array>
#include <boost/endian/conversion.hpp>
#include <crypto/crypto.hpp>
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <streaming/data-structures.hpp>
#include <vector>

static void gst_buffer_copy_into(GstBuffer *buf, unsigned char *destination) {
  auto size = gst_buffer_get_size(buf);
  /* get READ access to the memory and fill with vals */
  GstMapInfo info;
  gst_buffer_map(buf, &info, GST_MAP_READ);
  for (int i = 0; i < size; i++) {
    destination[i] = info.data[i];
  }
  gst_buffer_unmap(buf, &info);
}

static std::vector<unsigned char> gst_buffer_copy_content(GstBuffer *buf, unsigned long offset, unsigned long size) {
  auto vals = std::vector<unsigned char>(size);

  /* get READ access to the memory and fill with vals */
  GstMapInfo info;
  gst_buffer_map(buf, &info, GST_MAP_READ);
  for (int i = 0; i < size; i++) {
    vals[i] = info.data[i + offset];
  }
  gst_buffer_unmap(buf, &info);
  return vals;
}

static std::vector<unsigned char> gst_buffer_copy_content(GstBuffer *buf, unsigned long offset) {
  return gst_buffer_copy_content(buf, offset, gst_buffer_get_size(buf) - offset);
}

static std::vector<unsigned char> gst_buffer_copy_content(GstBuffer *buf) {
  return gst_buffer_copy_content(buf, 0);
}

/**
 * Creates a GstBuffer and fill the memory with the given value
 */
static GstBuffer *gst_buffer_new_and_fill(gsize size, int fill_val) {
  GstBuffer *buf = gst_buffer_new_allocate(NULL, size, NULL);

  /* get WRITE access to the memory and fill with fill_val */
  GstMapInfo info;
  gst_buffer_map(buf, &info, GST_MAP_WRITE);
  memset(info.data, fill_val, info.size);
  gst_buffer_unmap(buf, &info);
  return buf;
}

/**
 * Creates a GstBuffer from the given array of chars
 */
static GstBuffer *gst_buffer_new_and_fill(gsize size, const char vals[]) {
  GstBuffer *buf = gst_buffer_new_allocate(NULL, size, NULL);

  /* get WRITE access to the memory and fill with vals */
  GstMapInfo info;
  gst_buffer_map(buf, &info, GST_MAP_WRITE);
  for (int i = 0; i < size; i++) {
    info.data[i] = vals[i];
  }
  gst_buffer_unmap(buf, &info);
  return buf;
}

/**
 * From a list of buffer returns a single buffer that contains them all.
 * No copy of the stored data is done
 */
static GstBuffer *gst_buffer_list_unfold(GstBufferList *buffer_list) {
  GstBuffer *buf = gst_buffer_new_allocate(NULL, 0, NULL);

  for (int idx = 0; idx < gst_buffer_list_length(buffer_list); idx++) {
    auto buf_idx =
        gst_buffer_copy(gst_buffer_list_get(buffer_list, idx)); // copy here is about the buffer object, not the data
    gst_buffer_append(buf, buf_idx);
  }

  return buf;
}

/**
 * Copies out buffer metadata without affecting data
 */
static void gst_copy_timestamps(GstBuffer *src, GstBuffer *dest) {
  dest->pts = src->pts;
  dest->dts = src->dts;
  dest->offset = src->offset;
  dest->duration = src->duration;
  dest->offset_end = src->offset_end;
}

/**
 * Derives the proper IV following Moonlight implementation
 */
static std::string derive_iv(const std::string &aes_iv, int cur_seq_number) {
  auto iv = std::array<std::uint8_t, 16>{};
  *(std::uint32_t *)iv.data() = boost::endian::native_to_big((*(std::uint32_t *)aes_iv.c_str()) + cur_seq_number);
  return {iv.begin(), iv.end()};
}

/**
 * Encrypts the input buffer using AES CBC
 * @returns a new buffer with the content encrypted
 */
static GstBuffer *encrypt_payload(const std::string &aes_key, const std::string &aes_iv, GstBuffer *inbuf) {
  GstMapInfo info;
  gst_buffer_map(inbuf, &info, GST_MAP_READ);
  std::string packet_content((char *)info.data, gst_buffer_get_size(inbuf));

  auto encrypted = crypto::aes_encrypt_cbc(packet_content, aes_key, aes_iv, true);

  gst_buffer_unmap(inbuf, &info);
  return gst_buffer_new_and_fill(encrypted.size(), encrypted.c_str());
}