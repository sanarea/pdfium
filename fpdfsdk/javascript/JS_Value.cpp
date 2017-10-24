// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/JS_Value.h"

#include <time.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "fpdfsdk/javascript/Document.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_Object.h"

namespace {

double
MakeDate(int year, int mon, int day, int hour, int min, int sec, int ms) {
  return JS_MakeDate(JS_MakeDay(year, mon, day),
                     JS_MakeTime(hour, min, sec, ms));
}

double GetLocalTZA() {
  if (!FSDK_IsSandBoxPolicyEnabled(FPDF_POLICY_MACHINETIME_ACCESS))
    return 0;
  time_t t = 0;
  time(&t);
  localtime(&t);
#if _FX_PLATFORM_ == _FX_PLATFORM_WINDOWS_
  // In gcc 'timezone' is a global variable declared in time.h. In VC++, that
  // variable was removed in VC++ 2015, with _get_timezone replacing it.
  long timezone = 0;
  _get_timezone(&timezone);
#endif  // _FX_PLATFORM_ == _FX_PLATFORM_WINDOWS_
  return (double)(-(timezone * 1000));
}

int GetDaylightSavingTA(double d) {
  if (!FSDK_IsSandBoxPolicyEnabled(FPDF_POLICY_MACHINETIME_ACCESS))
    return 0;
  time_t t = (time_t)(d / 1000);
  struct tm* tmp = localtime(&t);
  if (!tmp)
    return 0;
  if (tmp->tm_isdst > 0)
    // One hour.
    return (int)60 * 60 * 1000;
  return 0;
}

double Mod(double x, double y) {
  double r = fmod(x, y);
  if (r < 0)
    r += y;
  return r;
}

bool IsLeapYear(int year) {
  return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 != 0));
}

int DayFromYear(int y) {
  return (int)(365 * (y - 1970.0) + floor((y - 1969.0) / 4) -
               floor((y - 1901.0) / 100) + floor((y - 1601.0) / 400));
}

double TimeFromYear(int y) {
  return 86400000.0 * DayFromYear(y);
}

static const uint16_t daysMonth[12] = {0,   31,  59,  90,  120, 151,
                                       181, 212, 243, 273, 304, 334};
static const uint16_t leapDaysMonth[12] = {0,   31,  60,  91,  121, 152,
                                           182, 213, 244, 274, 305, 335};

double TimeFromYearMonth(int y, int m) {
  const uint16_t* pMonth = IsLeapYear(y) ? leapDaysMonth : daysMonth;
  return TimeFromYear(y) + ((double)pMonth[m]) * 86400000;
}

int Day(double t) {
  return static_cast<int>(floor(t / 86400000.0));
}

int YearFromTime(double t) {
  // estimate the time.
  int y = 1970 + static_cast<int>(t / (365.2425 * 86400000.0));
  if (TimeFromYear(y) <= t) {
    while (TimeFromYear(y + 1) <= t)
      y++;
  } else {
    while (TimeFromYear(y) > t)
      y--;
  }
  return y;
}

int DayWithinYear(double t) {
  int year = YearFromTime(t);
  int day = Day(t);
  return day - DayFromYear(year);
}

int MonthFromTime(double t) {
  int day = DayWithinYear(t);
  int year = YearFromTime(t);
  if (0 <= day && day < 31)
    return 0;
  if (31 <= day && day < 59 + IsLeapYear(year))
    return 1;
  if ((59 + IsLeapYear(year)) <= day && day < (90 + IsLeapYear(year)))
    return 2;
  if ((90 + IsLeapYear(year)) <= day && day < (120 + IsLeapYear(year)))
    return 3;
  if ((120 + IsLeapYear(year)) <= day && day < (151 + IsLeapYear(year)))
    return 4;
  if ((151 + IsLeapYear(year)) <= day && day < (181 + IsLeapYear(year)))
    return 5;
  if ((181 + IsLeapYear(year)) <= day && day < (212 + IsLeapYear(year)))
    return 6;
  if ((212 + IsLeapYear(year)) <= day && day < (243 + IsLeapYear(year)))
    return 7;
  if ((243 + IsLeapYear(year)) <= day && day < (273 + IsLeapYear(year)))
    return 8;
  if ((273 + IsLeapYear(year)) <= day && day < (304 + IsLeapYear(year)))
    return 9;
  if ((304 + IsLeapYear(year)) <= day && day < (334 + IsLeapYear(year)))
    return 10;
  if ((334 + IsLeapYear(year)) <= day && day < (365 + IsLeapYear(year)))
    return 11;

  return -1;
}

int DateFromTime(double t) {
  int day = DayWithinYear(t);
  int year = YearFromTime(t);
  int leap = IsLeapYear(year);
  int month = MonthFromTime(t);
  switch (month) {
    case 0:
      return day + 1;
    case 1:
      return day - 30;
    case 2:
      return day - 58 - leap;
    case 3:
      return day - 89 - leap;
    case 4:
      return day - 119 - leap;
    case 5:
      return day - 150 - leap;
    case 6:
      return day - 180 - leap;
    case 7:
      return day - 211 - leap;
    case 8:
      return day - 242 - leap;
    case 9:
      return day - 272 - leap;
    case 10:
      return day - 303 - leap;
    case 11:
      return day - 333 - leap;
    default:
      return 0;
  }
}

double JS_LocalTime(double d) {
  return d + GetLocalTZA() + GetDaylightSavingTA(d);
}

}  // namespace

CJS_Value::CJS_Value() {}

CJS_Value::CJS_Value(v8::Local<v8::Value> pValue) : m_pValue(pValue) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, int iValue)
    : CJS_Value(pRuntime->NewNumber(iValue)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, bool bValue)
    : CJS_Value(pRuntime->NewBoolean(bValue)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, double dValue)
    : CJS_Value(pRuntime->NewNumber(dValue)) {}

CJS_Value::CJS_Value(CJS_Object* pObj) {
  if (pObj)
    m_pValue = pObj->ToV8Object();
}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const wchar_t* pWstr)
    : CJS_Value(pRuntime->NewString(pWstr)) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const char* pStr)
    : CJS_Value(pRuntime->NewString(WideString::FromLocal(pStr).c_str())) {}

CJS_Value::CJS_Value(CJS_Runtime* pRuntime, const CJS_Array& array)
    : CJS_Value(array.ToV8Array(pRuntime)) {}

CJS_Value::CJS_Value(const CJS_Date& date) : CJS_Value(date.ToV8Date()) {}

CJS_Value::~CJS_Value() {}

CJS_Value::CJS_Value(const CJS_Value& other) = default;

void CJS_Value::Set(v8::Local<v8::Value> pValue) {
  m_pValue = pValue;
}

int CJS_Value::ToInt(CJS_Runtime* pRuntime) const {
  return pRuntime->ToInt32(m_pValue);
}

bool CJS_Value::ToBool(CJS_Runtime* pRuntime) const {
  return pRuntime->ToBoolean(m_pValue);
}

double CJS_Value::ToDouble(CJS_Runtime* pRuntime) const {
  return pRuntime->ToDouble(m_pValue);
}

float CJS_Value::ToFloat(CJS_Runtime* pRuntime) const {
  return static_cast<float>(ToDouble(pRuntime));
}

CJS_Object* CJS_Value::ToObject(CJS_Runtime* pRuntime) const {
  v8::Local<v8::Object> pObj = pRuntime->ToObject(m_pValue);
  return static_cast<CJS_Object*>(pRuntime->GetObjectPrivate(pObj));
}

CJS_Document* CJS_Value::ToDocument(CJS_Runtime* pRuntime) const {
  return static_cast<CJS_Document*>(ToObject(pRuntime));
}

CJS_Array CJS_Value::ToArray(CJS_Runtime* pRuntime) const {
  ASSERT(IsArrayObject());
  return CJS_Array(pRuntime->ToArray(m_pValue));
}

CJS_Date CJS_Value::ToDate() const {
  ASSERT(IsDateObject());
  v8::Local<v8::Value> mutable_value = m_pValue;
  return CJS_Date(mutable_value.As<v8::Date>());
}

v8::Local<v8::Object> CJS_Value::ToV8Object(CJS_Runtime* pRuntime) const {
  return pRuntime->ToObject(m_pValue);
}

WideString CJS_Value::ToWideString(CJS_Runtime* pRuntime) const {
  return pRuntime->ToWideString(m_pValue);
}

ByteString CJS_Value::ToByteString(CJS_Runtime* pRuntime) const {
  return ByteString::FromUnicode(ToWideString(pRuntime));
}

v8::Local<v8::Value> CJS_Value::ToV8Value() const {
  return m_pValue;
}

v8::Local<v8::Array> CJS_Value::ToV8Array(CJS_Runtime* pRuntime) const {
  return pRuntime->ToArray(m_pValue);
}

void CJS_Value::MaybeCoerceToNumber(CJS_Runtime* pRuntime) {
  bool bAllowNaN = false;
  if (GetType() == VT_string) {
    ByteString bstr = ToByteString(pRuntime);
    if (bstr.GetLength() == 0)
      return;
    if (bstr == "NaN")
      bAllowNaN = true;
  }
  v8::Isolate* pIsolate = pRuntime->GetIsolate();
  v8::TryCatch try_catch(pIsolate);
  v8::MaybeLocal<v8::Number> maybeNum =
      m_pValue->ToNumber(pIsolate->GetCurrentContext());
  if (maybeNum.IsEmpty())
    return;
  v8::Local<v8::Number> num = maybeNum.ToLocalChecked();
  if (std::isnan(num->Value()) && !bAllowNaN)
    return;
  m_pValue = num;
}

// static
CJS_Value::Type CJS_Value::GetValueType(v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return VT_unknown;
  if (value->IsString())
    return VT_string;
  if (value->IsNumber())
    return VT_number;
  if (value->IsBoolean())
    return VT_boolean;
  if (value->IsDate())
    return VT_date;
  if (value->IsObject())
    return VT_object;
  if (value->IsNull())
    return VT_null;
  if (value->IsUndefined())
    return VT_undefined;
  return VT_unknown;
}

bool CJS_Value::IsArrayObject() const {
  return !m_pValue.IsEmpty() && m_pValue->IsArray();
}

bool CJS_Value::IsDateObject() const {
  return !m_pValue.IsEmpty() && m_pValue->IsDate();
}

CJS_Array::CJS_Array() {}

CJS_Array::CJS_Array(v8::Local<v8::Array> pArray) : m_pArray(pArray) {}

CJS_Array::CJS_Array(const CJS_Array& other) = default;

CJS_Array::~CJS_Array() {}

CJS_Value CJS_Array::GetElement(CJS_Runtime* pRuntime, unsigned index) const {
  if (!m_pArray.IsEmpty())
    return CJS_Value(
        v8::Local<v8::Value>(pRuntime->GetArrayElement(m_pArray, index)));
  return {};
}

void CJS_Array::SetElement(CJS_Runtime* pRuntime,
                           unsigned index,
                           const CJS_Value& value) {
  if (m_pArray.IsEmpty())
    m_pArray = pRuntime->NewArray();

  pRuntime->PutArrayElement(m_pArray, index, value.ToV8Value());
}

int CJS_Array::GetLength(CJS_Runtime* pRuntime) const {
  if (m_pArray.IsEmpty())
    return 0;
  return pRuntime->GetArrayLength(m_pArray);
}

v8::Local<v8::Array> CJS_Array::ToV8Array(CJS_Runtime* pRuntime) const {
  if (m_pArray.IsEmpty())
    m_pArray = pRuntime->NewArray();

  return m_pArray;
}

CJS_Date::CJS_Date() {}

CJS_Date::CJS_Date(v8::Local<v8::Date> pDate) : m_pDate(pDate) {}

CJS_Date::CJS_Date(CJS_Runtime* pRuntime, double dMsecTime)
    : m_pDate(pRuntime->NewDate(dMsecTime)) {}

CJS_Date::CJS_Date(CJS_Runtime* pRuntime,
                   int year,
                   int mon,
                   int day,
                   int hour,
                   int min,
                   int sec)
    : m_pDate(pRuntime->NewDate(MakeDate(year, mon, day, hour, min, sec, 0))) {}

CJS_Date::CJS_Date(const CJS_Date& other) = default;

CJS_Date::~CJS_Date() {}

bool CJS_Date::IsValidDate(CJS_Runtime* pRuntime) const {
  return !m_pDate.IsEmpty() && !std::isnan(pRuntime->ToDouble(m_pDate));
}

int CJS_Date::GetYear(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetYearFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

int CJS_Date::GetMonth(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;
  return JS_GetMonthFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

int CJS_Date::GetDay(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetDayFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

int CJS_Date::GetHours(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetHourFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

int CJS_Date::GetMinutes(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetMinFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

int CJS_Date::GetSeconds(CJS_Runtime* pRuntime) const {
  if (!IsValidDate(pRuntime))
    return 0;

  return JS_GetSecFromTime(JS_LocalTime(pRuntime->ToDouble(m_pDate)));
}

v8::Local<v8::Date> CJS_Date::ToV8Date() const {
  return m_pDate;
}

double JS_GetDateTime() {
  if (!FSDK_IsSandBoxPolicyEnabled(FPDF_POLICY_MACHINETIME_ACCESS))
    return 0;
  time_t t = time(nullptr);
  struct tm* pTm = localtime(&t);

  int year = pTm->tm_year + 1900;
  double t1 = TimeFromYear(year);

  return t1 + pTm->tm_yday * 86400000.0 + pTm->tm_hour * 3600000.0 +
         pTm->tm_min * 60000.0 + pTm->tm_sec * 1000.0;
}

int JS_GetYearFromTime(double dt) {
  return YearFromTime(dt);
}

int JS_GetMonthFromTime(double dt) {
  return MonthFromTime(dt);
}

int JS_GetDayFromTime(double dt) {
  return DateFromTime(dt);
}

int JS_GetHourFromTime(double dt) {
  return (int)Mod(floor(dt / (60 * 60 * 1000)), 24);
}

int JS_GetMinFromTime(double dt) {
  return (int)Mod(floor(dt / (60 * 1000)), 60);
}

int JS_GetSecFromTime(double dt) {
  return (int)Mod(floor(dt / 1000), 60);
}

double JS_DateParse(const WideString& str) {
  v8::Isolate* pIsolate = v8::Isolate::GetCurrent();
  v8::Isolate::Scope isolate_scope(pIsolate);
  v8::HandleScope scope(pIsolate);

  v8::Local<v8::Context> context = pIsolate->GetCurrentContext();

  // Use the built-in object method.
  v8::Local<v8::Value> v =
      context->Global()
          ->Get(context, v8::String::NewFromUtf8(pIsolate, "Date",
                                                 v8::NewStringType::kNormal)
                             .ToLocalChecked())
          .ToLocalChecked();
  if (v->IsObject()) {
    v8::Local<v8::Object> o = v->ToObject(context).ToLocalChecked();
    v = o->Get(context, v8::String::NewFromUtf8(pIsolate, "parse",
                                                v8::NewStringType::kNormal)
                            .ToLocalChecked())
            .ToLocalChecked();
    if (v->IsFunction()) {
      v8::Local<v8::Function> funC = v8::Local<v8::Function>::Cast(v);
      const int argc = 1;
      v8::Local<v8::Value> timeStr =
          CJS_Runtime::CurrentRuntimeFromIsolate(pIsolate)->NewString(
              str.AsStringView());
      v8::Local<v8::Value> argv[argc] = {timeStr};
      v = funC->Call(context, context->Global(), argc, argv).ToLocalChecked();
      if (v->IsNumber()) {
        double date = v->ToNumber(context).ToLocalChecked()->Value();
        if (!std::isfinite(date))
          return date;
        return JS_LocalTime(date);
      }
    }
  }
  return 0;
}

double JS_MakeDay(int nYear, int nMonth, int nDate) {
  double y = static_cast<double>(nYear);
  double m = static_cast<double>(nMonth);
  double dt = static_cast<double>(nDate);
  double ym = y + floor(m / 12);
  double mn = Mod(m, 12);
  double t = TimeFromYearMonth(static_cast<int>(ym), static_cast<int>(mn));
  if (YearFromTime(t) != ym || MonthFromTime(t) != mn || DateFromTime(t) != 1)
    return std::nan("");

  return Day(t) + dt - 1;
}

double JS_MakeTime(int nHour, int nMin, int nSec, int nMs) {
  double h = static_cast<double>(nHour);
  double m = static_cast<double>(nMin);
  double s = static_cast<double>(nSec);
  double milli = static_cast<double>(nMs);
  return h * 3600000 + m * 60000 + s * 1000 + milli;
}

double JS_MakeDate(double day, double time) {
  if (!std::isfinite(day) || !std::isfinite(time))
    return std::nan("");

  return day * 86400000 + time;
}

std::vector<CJS_Value> ExpandKeywordParams(
    CJS_Runtime* pRuntime,
    const std::vector<CJS_Value>& originals,
    size_t nKeywords,
    ...) {
  ASSERT(nKeywords);

  std::vector<CJS_Value> result(nKeywords, CJS_Value());
  size_t size = std::min(originals.size(), nKeywords);
  for (size_t i = 0; i < size; ++i)
    result[i] = originals[i];

  if (originals.size() != 1 || originals[0].GetType() != CJS_Value::VT_object ||
      originals[0].IsArrayObject()) {
    return result;
  }
  v8::Local<v8::Object> pObj = originals[0].ToV8Object(pRuntime);
  result[0] = CJS_Value();  // Make unknown.

  va_list ap;
  va_start(ap, nKeywords);
  for (size_t i = 0; i < nKeywords; ++i) {
    const wchar_t* property = va_arg(ap, const wchar_t*);
    v8::Local<v8::Value> v8Value = pRuntime->GetObjectProperty(pObj, property);
    if (!v8Value->IsUndefined())
      result[i] = CJS_Value(v8Value);
  }
  va_end(ap);
  return result;
}
