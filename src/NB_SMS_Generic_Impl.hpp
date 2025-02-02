/*********************************************************************************************************************************
  NB_SMS_Generic_Impl.hpp
  
  For ESP8266, ESP32, SAMD21/SAMD51, nRF52, SAM DUE, Teensy and STM32 with NB modules
  
  NB_Generic is a library for the ESP8266, ESP32, SAMD21/SAMD51, nRF52, SAM DUE, Teensy and STM32 with NB modules
  
  Based on and modified from MKRNB Library (https://github.com/arduino-libraries/MKRNB)
  
  Built by Khoi Hoang https://github.com/khoih-prog/NB_Generic
  Licensed under GNU Lesser General Public License
  
  Copyright (C) 2018  Arduino AG (http://www.arduino.cc/)
  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License 
  as published by the Free Software Foundation, either version 2.1 of the License, or (at your option) any later version.
  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty 
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with this program.
  If not, see <https://www.gnu.org/licenses/>.  
 
  Version: 1.1.0
  
  Version Modified By   Date      Comments
  ------- -----------  ---------- -----------
  1.0.0    K Hoang     18/03/2021 Initial public release to add support to many boards / modules besides MKRNB 1500 / SARA R4
  1.0.1    K Hoang     18/03/2021 Add Advanced examples (MQTT, Blynk)
  1.1.0    K Hoang     19/03/2021 Rewrite to prepare for supporting more GSM/GPRS modules. Add FileUtils examples.
 **********************************************************************************************************************************/

#pragma once

#ifndef _NB_SMS_GENERIC_IMPL_H_INCLUDED
#define _NB_SMS_GENERIC_IMPL_H_INCLUDED

#define NYBBLETOHEX(x)    ((x)<=9?(x)+'0':(x)-10+'A')
#define HEXTONYBBLE(x)    ((x)<='9'?(x)-'0':(x)+10-'A')
#define ITOHEX(x)         NYBBLETOHEX((x)&0xF)



NB_SMS::NB_SMS(bool synch)
{
  _smsData.synch          = synch;
  _smsData.state          = SMS_STATE_IDLE;
  _smsData.smsTxActive    = false;
  _smsData.charset        = SMS_CHARSET_NONE;

  memset(_smsData.bufferUTF8, 0, sizeof(_smsData.bufferUTF8));

  _smsData.indexUTF8      = 0;
  _smsData.ptrUTF8        = "";
}

/* Translation tables from GSM_03.38 are equal to UTF-8 for the
   positions 0x0A, 0x0D, 0x1B, 0x20-0x23, 0x25-0x3F, 0x41-0x5A, 0x61-0x7A.
   Collect the others into two translation tables.
   Code uses a simplified range test. */

struct gsm_mapping
{
  const unsigned char gsmc;
  const char *utf8;

  gsm_mapping(const char gsmc, const char *utf8)
    : gsmc(gsmc), utf8(utf8) {}
};

gsm_mapping _gsmUTF8map[] =
{
  {0x00, "@"}, {0x10, "Δ"},
  {0x40, "¡"}, {0x60, "¿"},
  {0x01, "£"}, {0x11, "_"},
  {0x02, "$"}, {0x12, "Φ"},
  {0x03, "¥"}, {0x13, "Γ"},
  {0x04, "è"}, {0x14, "Λ"},
  {0x24, "¤"},
  {0x05, "é"}, {0x15, "Ω"},
  {0x06, "ù"}, {0x16, "Π"},
  {0x07, "ì"}, {0x17, "Ψ"},
  {0x08, "ò"}, {0x18, "Σ"},
  {0x09, "Ç"}, {0x19, "Θ"},
  /* Text mode SMS uses 0x1A as send marker so Ξ is not available. */
  //{0x1A,"Ξ"},
  {0x0B, "Ø"},
  {0x5B, "Ä"}, {0x7B, "ä"},
  {0x0C, "ø"}, {0x1C, "Æ"},
  {0x5C, "Ö"}, {0x7C, "ö"},
  {0x1D, "æ"},
  {0x5D, "Ñ"}, {0x7D, "ñ"},
  {0x0E, "Å"}, {0x1E, "ß"},
  {0x5E, "Ü"}, {0x7E, "ü"},
  {0x0F, "å"}, {0x1F, "É"},
  {0x5F, "§"}, {0x7F, "à"}
};

/* Text mode SMS uses 0x1B as abort marker so extended set is not available. */
#if 0
gsm_mapping _gsmXUTF8map[] =
{
  {0x40, "|"},
  {0x14, "^"},
  {0x65, "€"},
  {0x28, "{"},
  {0x29, "}"},
  {0x0A, "\f"},
  {0x1B, "\b"},
  {0x3C, "["},
  {0x0D, "\n"}, {0x3D, "~"},
  {0x3E, "]"},
  {0x2F, "\\"}
};
* /
#endif

/////////////////////////////////////////////////////////////////

int NB_SMS::setCharset(const char* charset)
{
  return MODEM.setCharset(_smsData.charset, charset);
}

int NB_SMS::beginSMS(const char* to)
{
  return MODEM.beginSMS(_smsData, to);
}

int NB_SMS::available()
{
  return MODEM.available(_smsData, _incomingBuffer);
}

void NB_SMS::flush()
{
  MODEM.flush(_smsData, _incomingBuffer);
}

void NB_SMS::clear(int flag)
{
  MODEM.clear(_smsData, flag);
}

/////////////////////////////////////////////////////////////////

int NB_SMS::ready()
{
  int ready = MODEM.ready();

  if (ready == 0)
  {
    return 0;
  }

  switch (_smsData.state)
  {
    case SMS_STATE_IDLE:
    default:
      {
        break;
      }

    case SMS_STATE_LIST_MESSAGES:
      {
        MODEM.setResponseDataStorage(&_incomingBuffer);
        MODEM.listUnreadSMS();

        _smsData.state = SMS_STATE_WAIT_LIST_MESSAGES_RESPONSE;
        ready = 0;
        break;
      }

    case SMS_STATE_WAIT_LIST_MESSAGES_RESPONSE:
      {
        _smsData.state = SMS_STATE_IDLE;
        break;
      }
  }

  return ready;
}

size_t NB_SMS::write(uint8_t c)
{
  if (_smsData.smsTxActive)
  {
    if (_smsData.charset == SMS_CHARSET_GSM
        && (c >= 0x80 || c <= 0x24 || (c & 0x1F) == 0 || (c & 0x1F) >= 0x1B))
    {
      _smsData.bufferUTF8[_smsData.indexUTF8++] = c;

      if (_smsData.bufferUTF8[0] < 0x80
          || (_smsData.indexUTF8 == 2 && (_smsData.bufferUTF8[0] & 0xE0) == 0xC0)
          || (_smsData.indexUTF8 == 3 && (_smsData.bufferUTF8[0] & 0xF0) == 0xE0)
          || _smsData.indexUTF8 == 4)
      {
        for (auto &gsmchar : _gsmUTF8map)
        {
          if (strncmp(_smsData.bufferUTF8, gsmchar.utf8, _smsData.indexUTF8) == 0)
          {
            _smsData.indexUTF8 = 0;
            return MODEM.write(gsmchar.gsmc);
          }
        }

        // No UTF8 match, echo buffer
        for (c = 0; c < _smsData.indexUTF8; MODEM.write(_smsData.bufferUTF8[c++]));

        _smsData.indexUTF8 = 0;
      }

      return 1;
    }
    
    if (_smsData.charset == SMS_CHARSET_UCS2)
    {
      if (c < 0x80)
      {
        MODEM.write('0');
        MODEM.write('0');
        MODEM.write(ITOHEX(c >> 4));
      }
      else
      {
        _smsData.bufferUTF8[_smsData.indexUTF8++] = c;

        if (_smsData.indexUTF8 == 2 && (_smsData.bufferUTF8[0] & 0xE0) == 0xC0)
        {
          MODEM.write('0');
          MODEM.write(ITOHEX(_smsData.bufferUTF8[0] >> 2));
          MODEM.write(ITOHEX((_smsData.bufferUTF8[0] << 2) | ((c >> 4) & 0x3)));
        }
        else if (_smsData.indexUTF8 == 3 && (_smsData.bufferUTF8[0] & 0xF0) == 0xE0)
        {
          MODEM.write(ITOHEX(_smsData.bufferUTF8[0]));
          MODEM.write(ITOHEX(_smsData.bufferUTF8[1] >> 2));
          MODEM.write(ITOHEX((_smsData.bufferUTF8[1] << 2) | ((c >> 4) & 0x3)));
        }
        else if (_smsData.indexUTF8 == 4)
        {
          // Missing in UCS2, output SPC
          MODEM.write('0');
          MODEM.write('0');
          MODEM.write('2');
          c = 0;
        }
        else
        {
          return 1;
        }
      }

      _smsData.indexUTF8 = 0;
      c = ITOHEX(c);
    }

    return MODEM.write(c);
  }

  return 0;
}

int NB_SMS::endSMS()
{
  int r;

  if (_smsData.smsTxActive)
  {
    // Echo remaining content of UTF8 buffer, empty if no conversion
    for (r = 0; r < _smsData.indexUTF8; MODEM.write(_smsData.bufferUTF8[r++]));

    _smsData.indexUTF8 = 0;
    MODEM.write(26);

    if (_smsData.synch)
    {
      r = MODEM.waitForResponse(3 * 60 * 1000);
    }
    else
    {
      r = MODEM.ready();
    }

    return r;
  }
  else
  {
    return (_smsData.synch ? 0 : 2);
  }
}



int NB_SMS::remoteNumber(char* number, int nlength)
{
#define PHONE_NUMBER_START_SEARCH_PATTERN "\"REC UNREAD\",\""

  int phoneNumberStartIndex = _incomingBuffer.indexOf(PHONE_NUMBER_START_SEARCH_PATTERN);

  if (phoneNumberStartIndex != SUBSTRING_NOT_FOUND)
  {
    int i = phoneNumberStartIndex + sizeof(PHONE_NUMBER_START_SEARCH_PATTERN) - 1;

    if (_smsData.charset == SMS_CHARSET_UCS2 && _incomingBuffer.substring(i, i + 4) == "002B")
    {
      *number++ = '+';
      i += 4;
    }

    while (i < (int)_incomingBuffer.length() && nlength > 1)
    {
      if (_smsData.charset == SMS_CHARSET_UCS2)
      {
        i += 3;
      }

      char c = _incomingBuffer[i];

      if (c == '"')
      {
        break;
      }

      *number++ = c;
      nlength--;
      i++;
    }

    *number = '\0';
    return 1;
  }
  else
  {
    *number = '\0';
  }

  return 2;
}

int NB_SMS::read()
{
  if (*_smsData.ptrUTF8 != 0)
  {
    return *_smsData.ptrUTF8++;
  }

  if (_smsData.smsDataIndex < (signed)_incomingBuffer.length() && _smsData.smsDataIndex <= _smsData.smsDataEndIndex)
  {
    char c;

    if (_smsData.charset != SMS_CHARSET_UCS2)
    {
      c = _incomingBuffer[_smsData.smsDataIndex++];

      if (_smsData.charset == SMS_CHARSET_GSM
          && (c >= 0x80 || c <= 0x24 || (c & 0x1F) == 0 || (c & 0x1F) >= 0x1B))
      {
        for (auto &gsmchar : _gsmUTF8map)
        {
          if (c == gsmchar.gsmc)
          {
            _smsData.ptrUTF8 = gsmchar.utf8;
            return *_smsData.ptrUTF8++;
          }
        }
      }
    }
    else
    {
      c = (HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 2]) << 4)
          | HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 3]);

      if (strncmp(&_incomingBuffer[_smsData.smsDataIndex], "008", 3) >= 0)
      {
        _smsData.ptrUTF8 = _smsData.bufferUTF8 + 1;
        _smsData.bufferUTF8[2] = 0;
        _smsData.bufferUTF8[1] = (c & 0x3F) | 0x80;
        c = 0xC0 | (HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 1]) << 2)
            | (HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 2]) >> 2);

        if (strncmp(&_incomingBuffer[_smsData.smsDataIndex], "08", 2) >= 0)
        {
          _smsData.ptrUTF8 = _smsData.bufferUTF8;
          _smsData.bufferUTF8[0] = c & (0x80 | 0x3F);
          c = 0xE0 | HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex]);
        }
      }

      _smsData.smsDataIndex += 4;
    }

    return c;
  }

  return -1;
}

int NB_SMS::peek()
{
  if (*_smsData.ptrUTF8 != 0)
  {
    return *_smsData.ptrUTF8;
  }

  if (_smsData.smsDataIndex < (signed)_incomingBuffer.length() && _smsData.smsDataIndex <= _smsData.smsDataEndIndex)
  {
    char c = _incomingBuffer[_smsData.smsDataIndex++];

    if (_smsData.charset == SMS_CHARSET_GSM
        && (c >= 0x80 || c <= 0x24 || (c & 0x1F) == 0 || (c & 0x1F) >= 0x1B))
    {
      for (auto &gsmchar : _gsmUTF8map)
      {
        if (c == gsmchar.gsmc)
        {
          return gsmchar.utf8[0];
        }
      }
    }

    if (_smsData.charset == SMS_CHARSET_UCS2)
    {
      c = (HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 2]) << 4)
          | HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 3]);

      if (strncmp(&_incomingBuffer[_smsData.smsDataIndex], "008", 3) >= 0)
      {
        c = 0xC0 | (HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 1]) << 2)
            | (HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex + 2]) >> 2);

        if (strncmp(&_incomingBuffer[_smsData.smsDataIndex], "08", 2) >= 0)
        {
          c = 0xE0 | HEXTONYBBLE(_incomingBuffer[_smsData.smsDataIndex]);
        }
      }
    }

    return c;
  }

  return -1;
}

#endif    // _NB_SMS_GENERIC_IMPL_H_INCLUDED
