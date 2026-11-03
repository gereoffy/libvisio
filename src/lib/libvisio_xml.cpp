/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "libvisio_xml.h"

#ifndef BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
#define BOOST_LEXICAL_CAST_ASSUME_C_LOCALE 1
#endif
#include <boost/lexical_cast.hpp>

#include "VSDTypes.h"
#include "libvisio_utils.h"

namespace libvisio
{

namespace
{

extern "C"
{

  static int vsdxInputCloseFunc(void *)
  {
    return 0;
  }

  static int vsdxInputReadFunc(void *context, char *buffer, int len)
  {
    auto *input = (librevenge::RVNGInputStream *)context;

    if ((!input) || (!buffer) || (len < 0))
      return -1;

    if (input->isEnd())
      return 0;

    unsigned long tmpNumBytesRead = 0;
    const unsigned char *tmpBuffer = input->read(len, tmpNumBytesRead);

    if (tmpBuffer && tmpNumBytesRead)
      memcpy(buffer, tmpBuffer, tmpNumBytesRead);
    return tmpNumBytesRead;
  }

#ifdef DEBUG
  static void vsdxReaderErrorFunc(void *arg, const char *message, xmlParserSeverities severity, xmlTextReaderLocatorPtr)
#else
  static void vsdxReaderErrorFunc(void *arg, const char *, xmlParserSeverities severity, xmlTextReaderLocatorPtr)
#endif
  {
    auto *const watcher = reinterpret_cast<XMLErrorWatcher *>(arg);
    switch (severity)
    {
    case XML_PARSER_SEVERITY_VALIDITY_WARNING:
      VSD_DEBUG_MSG(("Found xml parser severity validity warning %s\n", message));
      break;
    case XML_PARSER_SEVERITY_VALIDITY_ERROR:
      VSD_DEBUG_MSG(("Found xml parser severity validity error %s\n", message));
      break;
    case XML_PARSER_SEVERITY_WARNING:
      VSD_DEBUG_MSG(("Found xml parser severity warning %s\n", message));
      break;
    case XML_PARSER_SEVERITY_ERROR:
      VSD_DEBUG_MSG(("Found xml parser severity error %s\n", message));
      if (watcher)
        watcher->setError();
      break;
    default:
      break;
    }
  }

} // extern "C"

} // anonymous namespace

XMLErrorWatcher::XMLErrorWatcher()
  : m_error(false)
{
}

bool XMLErrorWatcher::isError() const
{
  return m_error;
}

void XMLErrorWatcher::setError()
{
  m_error = true;
}

std::unique_ptr<xmlTextReader, void (*)(xmlTextReaderPtr)>
xmlReaderForStream(librevenge::RVNGInputStream *input, XMLErrorWatcher *const watcher, bool recover)
{
  int options = XML_PARSE_NOBLANKS | XML_PARSE_NONET;
  if (recover)
    options |= XML_PARSE_RECOVER;
  std::unique_ptr<xmlTextReader, void (*)(xmlTextReaderPtr)> reader
  {
    xmlReaderForIO(vsdxInputReadFunc, vsdxInputCloseFunc, (void *)input, nullptr, nullptr, options),
    xmlFreeTextReader
  };
  if (reader)
    xmlTextReaderSetErrorHandler(reader.get(), vsdxReaderErrorFunc, watcher);
  return reader;
}

Colour xmlStringToColour(const xmlChar *s)
{
  if (xmlStrEqual(s, BAD_CAST("Themed")))
    return Colour();
  std::string str((const char *)s);
  if (str[0] == '#')
  {
    if (str.length() != 7)
    {
      VSD_DEBUG_MSG(("Throwing XmlParserException\n"));
      throw XmlParserException();
    }
    else
      str.erase(str.begin());
  }
  else
  {
    if (str.length() != 6)
    {
      VSD_DEBUG_MSG(("Throwing XmlParserException\n"));
      throw XmlParserException();
    }
  }

  std::istringstream istr(str);
  unsigned val = 0;
  istr >> std::hex >> val;

  return Colour((val & 0xff0000) >> 16, (val & 0xff00) >> 8, val & 0xff, 0);
}

Colour xmlStringToColour(const std::shared_ptr<xmlChar> &s)
{
  return xmlStringToColour(s.get());
}

long xmlStringToLong(const xmlChar *s)
{
  using boost::lexical_cast;
  using boost::bad_lexical_cast;
  if (xmlStrEqual(s, BAD_CAST("Themed")))
    return 0;

  try
  {
    return boost::lexical_cast<long, const char *>((const char *)s);
  }
  catch (const boost::bad_lexical_cast &)
  {
    VSD_DEBUG_MSG(("Throwing XmlParserException\n"));
    throw XmlParserException();
  }
  return 0;
}

long xmlStringToLong(const std::shared_ptr<xmlChar> &s)
{
  return xmlStringToLong(s.get());
}

namespace
{

// The usual form of a number, [-+]digits[.digits][(e|E)[-+]digits], with at most 19 significant
// digits whose value fits in a double's 53 bits and a power of ten of at most 22: then the result
// is one correctly rounded multiplication or division of two exact doubles, the same value
// lexical_cast gives, without its stream (that took a tenth of the loading of large XML files).
// Anything else: false, left to lexical_cast.
bool fastStringToDouble(const char *s, double &value)
{
  static const double pow10[] =
  {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22
  };
  const char *p = s;
  bool negative = false;
  if (*p == '-' || *p == '+')
    negative = *p++ == '-';
  unsigned long long mantissa = 0;
  int digits = 0, exponent = 0;
  bool any = false;
  for (; *p >= '0' && *p <= '9'; ++p, any = true)
  {
    if ((mantissa || *p != '0') && ++digits > 19)
      return false;
    mantissa = mantissa * 10 + (unsigned)(*p - '0');
  }
  if (*p == '.')
  {
    for (++p; *p >= '0' && *p <= '9'; ++p, any = true)
    {
      if ((mantissa || *p != '0') && ++digits > 19)
        return false;
      mantissa = mantissa * 10 + (unsigned)(*p - '0');
      --exponent;
    }
  }
  if (!any)
    return false;
  if (*p == 'e' || *p == 'E')
  {
    ++p;
    bool negativeExp = false;
    if (*p == '-' || *p == '+')
      negativeExp = *p++ == '-';
    if (*p < '0' || *p > '9')
      return false;
    int e = 0;
    for (; *p >= '0' && *p <= '9'; ++p)
      if (e < 10000)
        e = e * 10 + (*p - '0');
    exponent += negativeExp ? -e : e;
  }
  if (*p || mantissa > (1ULL << 53) || exponent < -22 || exponent > 22)
    return false;
  double v = (double)mantissa;
  v = exponent < 0 ? v / pow10[-exponent] : v * pow10[exponent];
  value = negative ? -v : v;
  return true;
}

}

double xmlStringToDouble(const xmlChar *s) try
{
  if (xmlStrEqual(s, BAD_CAST("Themed")))
    return 0.0;

  double value;
  if (fastStringToDouble((const char *)s, value))
    return value;
  return boost::lexical_cast<double, const char *>((const char *)s);
}
catch (const boost::bad_lexical_cast &)
{
  VSD_DEBUG_MSG(("Throwing XmlParserException\n"));
  throw XmlParserException();
}

double xmlStringToDouble(const std::shared_ptr<xmlChar> &s)
{
  return xmlStringToDouble(s.get());
}

bool xmlStringToBool(const xmlChar *s)
{
  if (xmlStrEqual(s, BAD_CAST("Themed")))
    return 0;

  bool value = false;
  if (xmlStrEqual(s, BAD_CAST("true")) || xmlStrEqual(s, BAD_CAST("1")))
    value = true;
  else if (xmlStrEqual(s, BAD_CAST("false")) || xmlStrEqual(s, BAD_CAST("0")))
    value = false;
  else
  {
    VSD_DEBUG_MSG(("Throwing XmlParserException\n"));
    throw XmlParserException();
  }
  return value;

}

bool xmlStringToBool(const std::shared_ptr<xmlChar> &s)
{
  return xmlStringToBool(s.get());
}

} // namespace libvisio

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
