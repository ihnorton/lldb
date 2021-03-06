//===-- MICmnLLDBUtilSBValue.cpp --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Third party headers:
#include <cinttypes>

// In-house headers:
#include "MICmnLLDBUtilSBValue.h"
#include "MICmnLLDBDebugSessionInfo.h"
#include "MICmnMIValueConst.h"
#include "MICmnMIValueTuple.h"
#include "MIUtilString.h"

//++ ------------------------------------------------------------------------------------
// Details: CMICmnLLDBUtilSBValue constructor.
// Type:    Method.
// Args:    vrValue             - (R) The LLDB value object.
//          vbHandleCharType    - (R) True = Yes return text molding to char type,
//                                    False = just return data.
// Return:  None.
// Throws:  None.
//--
CMICmnLLDBUtilSBValue::CMICmnLLDBUtilSBValue(const lldb::SBValue &vrValue, const bool vbHandleCharType /* = false */,
                                             const bool vbHandleArrayType /* = true */)
    : m_rValue(const_cast<lldb::SBValue &>(vrValue))
    , m_pUnkwn("??")
    , m_pComposite("{...}")
    , m_bHandleCharType(vbHandleCharType)
    , m_bHandleArrayType(vbHandleArrayType)
{
    m_bValidSBValue = m_rValue.IsValid();
}

//++ ------------------------------------------------------------------------------------
// Details: CMICmnLLDBUtilSBValue destructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmnLLDBUtilSBValue::~CMICmnLLDBUtilSBValue()
{
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve from the LLDB SB Value object the name of the variable. If the name
//          is invalid (or the SBValue object invalid) then "??" is returned.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString   - Name of the variable or "??" for unknown.
// Throws:  None.
//--
CMIUtilString
CMICmnLLDBUtilSBValue::GetName() const
{
    const char *pName = m_bValidSBValue ? m_rValue.GetName() : nullptr;
    const CMIUtilString text((pName != nullptr) ? pName : m_pUnkwn);

    return text;
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve from the LLDB SB Value object the value of the variable described in
//          text. If the value is invalid (or the SBValue object invalid) then "??" is
//          returned.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString   - Text description of the variable's value or "??".
// Throws:  None.
//--
CMIUtilString
CMICmnLLDBUtilSBValue::GetValue(const bool vbExpandAggregates /* = false */) const
{
    if (!m_bValidSBValue)
        return m_pUnkwn;

    CMICmnLLDBDebugSessionInfo &rSessionInfo(CMICmnLLDBDebugSessionInfo::Instance());
    bool bPrintExpandAggregates = false;
    bPrintExpandAggregates = rSessionInfo.SharedDataRetrieve<bool>(rSessionInfo.m_constStrPrintExpandAggregates,
                                                                   bPrintExpandAggregates) && bPrintExpandAggregates;

    const bool bHandleArrayTypeAsSimple = m_bHandleArrayType && !vbExpandAggregates && !bPrintExpandAggregates;
    CMIUtilString value;
    const bool bIsSimpleValue = GetSimpleValue(bHandleArrayTypeAsSimple, value);
    if (bIsSimpleValue)
        return value;

    if (!vbExpandAggregates && !bPrintExpandAggregates)
        return m_pComposite;

    bool bPrintAggregateFieldNames = false;
    bPrintAggregateFieldNames = !rSessionInfo.SharedDataRetrieve<bool>(rSessionInfo.m_constStrPrintAggregateFieldNames,
                                                                       bPrintAggregateFieldNames) || bPrintAggregateFieldNames;

    CMICmnMIValueTuple miValueTuple;
    const bool bOk = GetCompositeValue(bPrintAggregateFieldNames, miValueTuple);
    if (!bOk)
        return m_pUnkwn;

    value = miValueTuple.GetString();
    return value;
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve from the LLDB SB Value object the value of the variable described in
//          text if it has a simple format (not composite).
// Type:    Method.
// Args:    vwrValue          - (W) The SBValue in a string format.
// Return:  MIstatus::success - Function succeeded.
//          MIstatus::failure - Function failed.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::GetSimpleValue(const bool vbHandleArrayType, CMIUtilString &vwrValue) const
{
    const MIuint nChildren = m_rValue.GetNumChildren();
    if (nChildren == 0)
    {
        if (m_bHandleCharType && IsCharType())
        {
            vwrValue = GetSimpleValueChar();
            return MIstatus::success;
        }
        else
        {
            const char *pValue = m_rValue.GetValue();
            vwrValue = pValue != nullptr ? pValue : m_pUnkwn;
            return MIstatus::success;
        }
    }
    else if (IsPointerType())
    {
        if (m_bHandleCharType && IsPointeeCharType())
        {
            vwrValue = GetSimpleValueCStringPointer();
            return MIstatus::success;
        }
        else
        {
            const char *pValue = m_rValue.GetValue();
            vwrValue = pValue != nullptr ? pValue : m_pUnkwn;
            return MIstatus::success;
        }
    }
    else if (IsArrayType())
    {
        CMICmnLLDBDebugSessionInfo &rSessionInfo(CMICmnLLDBDebugSessionInfo::Instance());
        bool bPrintCharArrayAsString = false;
        bPrintCharArrayAsString = rSessionInfo.SharedDataRetrieve<bool>(rSessionInfo.m_constStrPrintCharArrayAsString,
                                                                        bPrintCharArrayAsString) && bPrintCharArrayAsString;
        if (bPrintCharArrayAsString && m_bHandleCharType && IsFirstChildCharType())
        {
            vwrValue = GetSimpleValueCStringArray();
            return MIstatus::success;
        }
        else if (vbHandleArrayType)
        {
            vwrValue = CMIUtilString::Format("[%u]", nChildren);
            return MIstatus::success;
        }
    }

    // Composite variable type i.e. struct
    return MIstatus::failure;
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve from the LLDB SB Value object the char value of the variable.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString   - The char value of the variable.
// Throws:  None.
//--
CMIUtilString
CMICmnLLDBUtilSBValue::GetSimpleValueChar() const
{
    const uint64_t value = m_rValue.GetValueAsUnsigned();
    if (value == 0)
    {
        const uint64_t nFailValue = 1;
        if (nFailValue == m_rValue.GetValueAsUnsigned(nFailValue))
            return m_pUnkwn;
    }

    const lldb::BasicType eType = m_rValue.GetType().GetBasicType();
    switch (eType)
    {
        default:
            assert(0 && "value must be a char type");
        case lldb::eBasicTypeChar:
        case lldb::eBasicTypeSignedChar:
        case lldb::eBasicTypeUnsignedChar:
        {
            const CMIUtilString prefix(CMIUtilString::ConvertToPrintableASCII((char)value));
            return CMIUtilString::Format("%" PRIu8 " '%s'", (uint8_t)value, prefix.c_str());
        }
        case lldb::eBasicTypeChar16:
        {
            const CMIUtilString prefix(CMIUtilString::ConvertToPrintableASCII((char16_t)value));
            return CMIUtilString::Format("U+%04" PRIx16 " u'%s'", (uint16_t)value, prefix.c_str());
        }
        case lldb::eBasicTypeChar32:
        {
            const CMIUtilString prefix(CMIUtilString::ConvertToPrintableASCII((char32_t)value));
            return CMIUtilString::Format("U+%08" PRIx32 " U'%s'", (uint32_t)value, prefix.c_str());
        }
    }
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve from the LLDB SB Value object of type char* the c-string value.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString   - The c-string value of the variable.
// Throws:  None.
//--
CMIUtilString
CMICmnLLDBUtilSBValue::GetSimpleValueCStringPointer() const
{
    const char *value = m_rValue.GetValue();
    if (value == nullptr)
        return m_pUnkwn;

    lldb::SBValue child = m_rValue.GetChildAtIndex(0);
    const lldb::BasicType eType = child.GetType().GetBasicType();
    switch (eType)
    {
        default:
            assert(0 && "child must be a char type");
        case lldb::eBasicTypeChar:
        case lldb::eBasicTypeSignedChar:
        case lldb::eBasicTypeUnsignedChar:
        {
            const CMIUtilString prefix(ReadCStringFromHostMemory<char>(child));
            // Note code that has const in will not show the text suffix to the string pointer
            // i.e. const char * pMyStr = "blah"; ==> "0x00007000"" <-- Eclipse shows this
            // but        char * pMyStr = "blah"; ==> "0x00007000" "blah"" <-- Eclipse shows this
            return CMIUtilString::Format("%s \"%s\"", value, prefix.c_str());
        }
        case lldb::eBasicTypeChar16:
        {
            const CMIUtilString prefix(ReadCStringFromHostMemory<char16_t>(child));
            return CMIUtilString::Format("%s u\"%s\"", value, prefix.c_str());
        }
        case lldb::eBasicTypeChar32:
        {
            const CMIUtilString prefix(ReadCStringFromHostMemory<char32_t>(child));
            return CMIUtilString::Format("%s U\"%s\"", value, prefix.c_str());
        }
    }
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve from the LLDB SB Value object of type char[] the c-string value.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString   - The c-string value of the variable.
// Throws:  None.
//--
CMIUtilString
CMICmnLLDBUtilSBValue::GetSimpleValueCStringArray() const
{
    const MIuint nChildren = m_rValue.GetNumChildren();
    lldb::SBValue child = m_rValue.GetChildAtIndex(0);
    const lldb::BasicType eType = child.GetType().GetBasicType();
    switch (eType)
    {
        default:
            assert(0 && "value must be a char[] type");
        case lldb::eBasicTypeChar:
        case lldb::eBasicTypeSignedChar:
        case lldb::eBasicTypeUnsignedChar:
        {
            const CMIUtilString prefix(ReadCStringFromHostMemory<char>(m_rValue, nChildren));
            // TODO: to match char* it should be the following
            //       return CMIUtilString::Format("[%u] \"%s\"", nChildren, prefix.c_str());
            return CMIUtilString::Format("\"%s\"", prefix.c_str());
        }
        case lldb::eBasicTypeChar16:
        {
            const CMIUtilString prefix(ReadCStringFromHostMemory<char16_t>(m_rValue, nChildren));
            return CMIUtilString::Format("u\"%s\"", prefix.c_str());
        }
        case lldb::eBasicTypeChar32:
        {
            const CMIUtilString prefix(ReadCStringFromHostMemory<char32_t>(m_rValue, nChildren));
            return CMIUtilString::Format("U\"%s\"", prefix.c_str());
        }
    }
}

bool
CMICmnLLDBUtilSBValue::GetCompositeValue(const bool vbPrintFieldNames, CMICmnMIValueTuple &vwrMiValueTuple,
                                         const MIuint vnDepth /* = 1 */) const
{
    const MIuint nMaxDepth = 10;
    const MIuint nChildren = m_rValue.GetNumChildren();
    for (MIuint i = 0; i < nChildren; ++i)
    {
        const lldb::SBValue member = m_rValue.GetChildAtIndex(i);
        const CMICmnLLDBUtilSBValue utilMember(member, m_bHandleCharType, m_bHandleArrayType);
        const bool bHandleArrayTypeAsSimple = false;
        CMIUtilString value;
        const bool bIsSimpleValue = utilMember.GetSimpleValue(bHandleArrayTypeAsSimple, value);
        if (bIsSimpleValue)
        {
            // OK. Value is simple (not composite) and was successfully got
        }
        else if (vnDepth < nMaxDepth)
        {
            // Need to get value from composite type
            CMICmnMIValueTuple miValueTuple;
            const bool bOk = utilMember.GetCompositeValue(vbPrintFieldNames, miValueTuple, vnDepth + 1);
            if (!bOk)
                // Can't obtain composite type
                value = m_pUnkwn;
            else
                // OK. Value is composite and was successfully got
                value = miValueTuple.GetString();
        }
        else
        {
            // Need to get value from composite type, but vnMaxDepth is reached
            value = m_pComposite;
        }
        const bool bNoQuotes = true;
        const CMICmnMIValueConst miValueConst(value, bNoQuotes);
        if (vbPrintFieldNames)
        {
            const bool bUseSpacing = true;
            const CMICmnMIValueResult miValueResult(utilMember.GetName(), miValueConst, bUseSpacing);
            vwrMiValueTuple.Add(miValueResult, bUseSpacing);
        }
        else
        {
            const bool bUseSpacing = false;
            vwrMiValueTuple.Add(miValueConst, bUseSpacing);
        }
    }

    return MIstatus::success;
}

//++ ------------------------------------------------------------------------------------
// Details: Check that basic type is a char type. Char type can be signed or unsigned.
// Type:    Static.
// Args:    eType   - type to check
// Return:  bool    - True = Yes is a char type, false = some other type.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsCharBasicType(lldb::BasicType eType)
{
    switch (eType)
    {
        case lldb::eBasicTypeChar:
        case lldb::eBasicTypeSignedChar:
        case lldb::eBasicTypeUnsignedChar:
        case lldb::eBasicTypeChar16:
        case lldb::eBasicTypeChar32:
            return true;
        default:
            return false;
    }
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the flag stating whether this value object is a char type or some
//          other type. Char type can be signed or unsigned.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = Yes is a char type, false = some other type.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsCharType() const
{
    const lldb::BasicType eType = m_rValue.GetType().GetBasicType();
    return IsCharBasicType(eType);
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the flag stating whether first child value object of *this object is
//          a char type or some other type. Returns false if there are not children. Char
//          type can be signed or unsigned.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = Yes is a char type, false = some other type.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsFirstChildCharType() const
{
    const MIuint nChildren = m_rValue.GetNumChildren();

    // Is it a basic type
    if (nChildren == 0)
        return false;

    const lldb::SBValue member = m_rValue.GetChildAtIndex(0);
    const CMICmnLLDBUtilSBValue utilValue(member);
    return utilValue.IsCharType();
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the flag stating whether pointee object of *this object is
//          a char type or some other type. Returns false if there are not children. Char
//          type can be signed or unsigned.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = Yes is a char type, false = some other type.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsPointeeCharType() const
{
    const MIuint nChildren = m_rValue.GetNumChildren();

    // Is it a basic type
    if (nChildren == 0)
        return false;

    const lldb::BasicType eType = m_rValue.GetType().GetPointeeType().GetBasicType();
    return IsCharBasicType(eType);
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the flag stating whether this value object is a integer type or some
//          other type. Char type can be signed or unsigned and short or long/very long.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = Yes is a integer type, false = some other type.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsIntegerType() const
{
    const lldb::BasicType eType = m_rValue.GetType().GetBasicType();
    return ((eType == lldb::eBasicTypeShort) || (eType == lldb::eBasicTypeUnsignedShort) ||
            (eType == lldb::eBasicTypeInt) || (eType == lldb::eBasicTypeUnsignedInt) ||
            (eType == lldb::eBasicTypeLong) || (eType == lldb::eBasicTypeUnsignedLong) ||
            (eType == lldb::eBasicTypeLongLong) || (eType == lldb::eBasicTypeUnsignedLongLong) ||
            (eType == lldb::eBasicTypeInt128) || (eType == lldb::eBasicTypeUnsignedInt128));
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the flag stating whether this value object is a pointer type or some
//          other type.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = Yes is a pointer type, false = some other type.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsPointerType() const
{
    return m_rValue.GetType().IsPointerType();
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the flag stating whether this value object is an array type or some
//          other type.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = Yes is an array type, false = some other type.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsArrayType() const
{
    return m_rValue.GetType().IsArrayType();
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the C string data of value object by read the memory where the
//          variable is held.
// Type:    Method.
// Args:    vrValue         - (R) LLDB SBValue variable object.
// Return:  CMIUtilString   - Text description of the variable's value.
// Throws:  None.
//--
template <typename charT>
CMIUtilString
CMICmnLLDBUtilSBValue::ReadCStringFromHostMemory(lldb::SBValue &vrValue, const MIuint vnMaxLen) const
{
    std::string result;
    lldb::addr_t addr = vrValue.GetLoadAddress(), end_addr = addr + vnMaxLen * sizeof(charT);
    lldb::SBProcess process = CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
    lldb::SBError error;
    while (addr < end_addr)
    {
        charT ch;
        const MIuint64 nReadBytes = process.ReadMemory(addr, &ch, sizeof(ch), error);
        if (error.Fail() || nReadBytes != sizeof(ch))
            return m_pUnkwn;
        else if (ch == 0)
            break;
        result.append(CMIUtilString::ConvertToPrintableASCII(ch, true /* bEscapeQuotes */));
        addr += sizeof(ch);
    }

    return result.c_str();
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the state of the value object's name.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = yes name is indeterminate, false = name is valid.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsNameUnknown() const
{
    const CMIUtilString name(GetName());
    return (name == m_pUnkwn);
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the state of the value object's value data.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = yes value is indeterminate, false = value valid.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsValueUnknown() const
{
    const CMIUtilString value(GetValue());
    return (value == m_pUnkwn);
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the value object's type name if valid.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString   - The type name or "??".
// Throws:  None.
//--
CMIUtilString
CMICmnLLDBUtilSBValue::GetTypeName() const
{
    const char *pName = m_bValidSBValue ? m_rValue.GetTypeName() : nullptr;
    const CMIUtilString text((pName != nullptr) ? pName : m_pUnkwn);

    return text;
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the value object's display type name if valid.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString   - The type name or "??".
// Throws:  None.
//--
CMIUtilString
CMICmnLLDBUtilSBValue::GetTypeNameDisplay() const
{
    const char *pName = m_bValidSBValue ? m_rValue.GetDisplayTypeName() : nullptr;
    const CMIUtilString text((pName != nullptr) ? pName : m_pUnkwn);

    return text;
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve whether the value object's is valid or not.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = valid, false = not valid.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsValid() const
{
    return m_bValidSBValue;
}

//++ ------------------------------------------------------------------------------------
// Details: Retrieve the value object' has a name. A value object can be valid but still
//          have no name which suggest it is not a variable.
// Type:    Method.
// Args:    None.
// Return:  bool    - True = valid, false = not valid.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::HasName() const
{
    bool bHasAName = false;

    const char *pName = m_bValidSBValue ? m_rValue.GetDisplayTypeName() : nullptr;
    if (pName != nullptr)
    {
        bHasAName = (CMIUtilString(pName).length() > 0);
    }

    return bHasAName;
}

//++ ------------------------------------------------------------------------------------
// Details: Determine if the value object' represents a LLDB variable i.e. "$0".
// Type:    Method.
// Args:    None.
// Return:  bool    - True = Yes LLDB variable, false = no.
// Throws:  None.
//--
bool
CMICmnLLDBUtilSBValue::IsLLDBVariable() const
{
    return (GetName().at(0) == '$');
}
