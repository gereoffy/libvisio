/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "libvisio_utils.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>   // MultiByteToWideChar: Windows has no iconv
#include <cstdlib>
#else
#include <cerrno>
#include <iconv.h>
#endif
#include "VSDInternalStream.h"

namespace
{

bool isUnicodeChar(uint32_t c)
{
  return c < 0x110000 && (c < 0xd800 || c > 0xdfff) && (c & 0xfffe) != 0xfffe && (c < 0xfdd0 || c > 0xfdef);
}

}

uint8_t libvisio::readU8(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint8_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint8_t))
    return *(uint8_t const *)(p);
  VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
  throw EndOfStreamException();
}

uint16_t libvisio::readU16(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint16_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint16_t))
    return (uint16_t)p[0]|((uint16_t)p[1]<<8);
  VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
  throw EndOfStreamException();
}

int16_t libvisio::readS16(librevenge::RVNGInputStream *input)
{
  return (int16_t)readU16(input);
}

uint32_t libvisio::readU32(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint32_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint32_t))
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
  VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
  throw EndOfStreamException();
}

int32_t libvisio::readS32(librevenge::RVNGInputStream *input)
{
  return (int32_t)readU32(input);
}

uint64_t libvisio::readU64(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint64_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint64_t))
    return (uint64_t)p[0]|((uint64_t)p[1]<<8)|((uint64_t)p[2]<<16)|((uint64_t)p[3]<<24)|((uint64_t)p[4]<<32)|((uint64_t)p[5]<<40)|((uint64_t)p[6]<<48)|((uint64_t)p[7]<<56);
  VSD_DEBUG_MSG(("Throwing EndOfStreamException\n"));
  throw EndOfStreamException();
}

double libvisio::readDouble(librevenge::RVNGInputStream *input)
{
  union
  {
    uint64_t u;
    double d;
  } tmpUnion;

  tmpUnion.u = readU64(input);

  return tmpUnion.d;
}

const librevenge::RVNGString libvisio::getColourString(const Colour &c)
{
  librevenge::RVNGString sColour;
  sColour.sprintf("#%.2x%.2x%.2x", c.r, c.g, c.b);
  return sColour;
}

unsigned long libvisio::getRemainingLength(librevenge::RVNGInputStream *const input)
{
  if (!input)
    throw EndOfStreamException();

  const long begin = input->tell();

  if (input->seek(0, librevenge::RVNG_SEEK_END) != 0)
  {
    // librevenge::RVNG_SEEK_END does not work. Use the harder way.
    while (!input->isEnd())
      readU8(input);
  }
  const long end = input->tell();

  input->seek(begin, librevenge::RVNG_SEEK_SET);

  if (end < begin)
    throw EndOfStreamException();
  return static_cast<unsigned long>(end - begin);
}

void libvisio::appendUCS4(librevenge::RVNGString &text, uint32_t ucs4Character)
{
  // Convert carriage returns to new line characters
  if (ucs4Character == 0x0d || ucs4Character == 0x0e)
    ucs4Character = '\n';

  unsigned char outbuf[5];
  int i = 0;
  if (ucs4Character < 0x80)
    outbuf[i++] = (unsigned char)ucs4Character;
  else if (ucs4Character < 0x800)
  {
    outbuf[i++] = (unsigned char)(0xc0 | (ucs4Character >> 6));
    outbuf[i++] = (unsigned char)(0x80 | (ucs4Character & 0x3f));
  }
  else if (ucs4Character < 0x10000)
  {
    outbuf[i++] = (unsigned char)(0xe0 | (ucs4Character >> 12));
    outbuf[i++] = (unsigned char)(0x80 | ((ucs4Character >> 6) & 0x3f));
    outbuf[i++] = (unsigned char)(0x80 | (ucs4Character & 0x3f));
  }
  else
  {
    outbuf[i++] = (unsigned char)(0xf0 | (ucs4Character >> 18));
    outbuf[i++] = (unsigned char)(0x80 | ((ucs4Character >> 12) & 0x3f));
    outbuf[i++] = (unsigned char)(0x80 | ((ucs4Character >> 6) & 0x3f));
    outbuf[i++] = (unsigned char)(0x80 | (ucs4Character & 0x3f));
  }
  outbuf[i] = 0;

  text.append((char *)outbuf);
}

void libvisio::appendCodepageCharacters(librevenge::RVNGString &text, const std::vector<unsigned char> &characters, const char *codepage)
{
  if (characters.empty())
    return;
#ifdef _WIN32
  // Windows' own converter, the code page number from the "CPnnnn" name; characters it can't
  // convert (its replacement character) are skipped
  const unsigned cp = strncmp(codepage, "CP", 2) == 0 ? (unsigned)atoi(codepage + 2) : 0;
  if (!cp)
    return;
  const int n = MultiByteToWideChar(cp, 0, reinterpret_cast<const char *>(characters.data()), (int)characters.size(), nullptr, 0);
  if (n <= 0)
    return;
  std::vector<wchar_t> wide((size_t)n);
  MultiByteToWideChar(cp, 0, reinterpret_cast<const char *>(characters.data()), (int)characters.size(), wide.data(), n);
  for (size_t i = 0; i < wide.size(); ++i)
  {
    uint32_t c = (uint16_t)wide[i];
    if (c >= 0xd800 && c < 0xdc00 && i + 1 < wide.size() && (uint16_t)wide[i + 1] >= 0xdc00 && (uint16_t)wide[i + 1] < 0xe000)
    {
      c = ((c - 0xd800) << 10) + ((uint16_t)wide[i + 1] - 0xdc00) + 0x10000;
      ++i;
    }
    if (c != 0xfffd && isUnicodeChar(c))
      appendUCS4(text, c);
  }
#else
  iconv_t cd = iconv_open("UTF-32LE", codepage);
  if (cd == (iconv_t)-1)
    return;
  auto *src = const_cast<char *>(reinterpret_cast<const char *>(characters.data()));
  size_t srcLeft = characters.size();
  while (srcLeft)
  {
    unsigned char out[4 * 64];
    auto *dst = reinterpret_cast<char *>(out);
    size_t dstLeft = sizeof(out);
    size_t r = iconv(cd, &src, &srcLeft, &dst, &dstLeft);
    for (size_t k = 0; k + 4 <= sizeof(out) - dstLeft; k += 4)
    {
      uint32_t c = out[k] | (out[k + 1] << 8) | (out[k + 2] << 16) | ((uint32_t)out[k + 3] << 24);
      if (isUnicodeChar(c))
        appendUCS4(text, c);
    }
    if (r == (size_t)-1 && errno != E2BIG)
    {
      if (errno == EINVAL)   // an incomplete sequence at the end
        break;
      src++;   // EILSEQ: skip the byte
      srcLeft--;
    }
  }
  iconv_close(cd);
#endif
}

void libvisio::appendUTF16Characters(librevenge::RVNGString &text, const std::vector<unsigned char> &characters)
{
  // UTF-16LE: unpaired surrogates are skipped
  for (size_t i = 0; i + 1 < characters.size(); i += 2)
  {
    uint32_t c = characters[i] | (characters[i + 1] << 8);
    if (c >= 0xd800 && c <= 0xdbff && i + 3 < characters.size())
    {
      uint32_t l = characters[i + 2] | (characters[i + 3] << 8);
      if (l >= 0xdc00 && l <= 0xdfff)
      {
        c = ((c - 0xd800) << 10) + (l - 0xdc00) + 0x10000;
        i += 2;
      }
    }
    if (isUnicodeChar(c))
      appendUCS4(text, c);
  }
}

void libvisio::debugPrint(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
