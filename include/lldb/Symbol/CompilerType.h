//===-- CompilerType.h ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_CompilerType_h_
#define liblldb_CompilerType_h_

#include <string>
#include <vector>
#include <functional>
#include "lldb/lldb-private.h"
#include "lldb/Core/ClangForward.h"

namespace lldb_private {

//----------------------------------------------------------------------
// A class that can carry around a clang ASTContext and a opaque clang 
// QualType. A clang::QualType can be easily reconstructed from an
// opaque clang type and often the ASTContext is needed when doing 
// various type related tasks, so this class allows both items to travel
// in a single very lightweight class that can be used. There are many
// static equivalents of the member functions that allow the ASTContext
// and the opaque clang QualType to be specified for ease of use and
// to avoid code duplication.
//----------------------------------------------------------------------
class CompilerType
{
public:
    //----------------------------------------------------------------------
    // Constructors and Destructors
    //----------------------------------------------------------------------
    CompilerType (TypeSystem *type_system, void *type);
    CompilerType (clang::ASTContext *ast_context, clang::QualType qual_type);

    CompilerType (const CompilerType &rhs) :
        m_type (rhs.m_type),
        m_type_system  (rhs.m_type_system)
    {
    }
    
    CompilerType () :
        m_type (0),
        m_type_system  (0)
    {
    }
    
    ~CompilerType();
    
    //----------------------------------------------------------------------
    // Operators
    //----------------------------------------------------------------------

    const CompilerType &
    operator= (const CompilerType &rhs)
    {
        m_type = rhs.m_type;
        m_type_system = rhs.m_type_system;
        return *this;
    }
    

    //----------------------------------------------------------------------
    // Tests
    //----------------------------------------------------------------------

    explicit operator bool () const
    {
        return m_type != NULL && m_type_system != NULL;
    }
    
    bool
    operator < (const CompilerType &rhs) const
    {
        if (m_type_system == rhs.m_type_system)
            return m_type < rhs.m_type;
        return m_type_system < rhs.m_type_system;
    }

    bool
    IsValid () const
    {
        return m_type != NULL && m_type_system != NULL;
    }
    
    bool
    IsArrayType (CompilerType *element_type,
                 uint64_t *size,
                 bool *is_incomplete) const;

    bool
    IsVectorType (CompilerType *element_type,
                  uint64_t *size) const;
    
    bool
    IsArrayOfScalarType () const;

    bool
    IsAggregateType () const;
    
    bool
    IsBeingDefined () const;

    bool
    IsCharType () const;

    bool
    IsCompleteType () const;
    
    bool
    IsConst() const;
    
    bool
    IsCStringType (uint32_t &length) const;

    bool
    IsDefined() const;
    
    bool
    IsFloatingPointType (uint32_t &count, bool &is_complex) const;

    bool
    IsFunctionType (bool *is_variadic_ptr = NULL) const;

    uint32_t
    IsHomogeneousAggregate (CompilerType* base_type_ptr) const;

    size_t
    GetNumberOfFunctionArguments () const;
    
    CompilerType
    GetFunctionArgumentAtIndex (const size_t index) const;
    
    bool
    IsVariadicFunctionType () const;

    bool
    IsFunctionPointerType () const;
    
    bool
    IsIntegerType (bool &is_signed) const;
    
    bool
    IsPolymorphicClass () const;

    bool
    IsPossibleCPlusPlusDynamicType (CompilerType *target_type = NULL) const
    {
        return IsPossibleDynamicType (target_type, true, false);
    }
    
    bool
    IsPossibleDynamicType (CompilerType *target_type, // Can pass NULL
                           bool check_cplusplus,
                           bool check_objc) const;


    bool
    IsPointerToScalarType () const;
    
    bool
    IsRuntimeGeneratedType () const;
    
    bool
    IsPointerType (CompilerType *pointee_type = NULL) const;
    
    bool
    IsPointerOrReferenceType (CompilerType *pointee_type = NULL) const;
    
    bool
    IsReferenceType (CompilerType *pointee_type = nullptr, bool* is_rvalue = nullptr) const;
    
    bool
    IsScalarType () const;
    
    bool
    IsTypedefType () const;

    bool
    IsVoidType () const;

    //----------------------------------------------------------------------
    // Type Completion
    //----------------------------------------------------------------------
    
    bool
    GetCompleteType () const;

    //----------------------------------------------------------------------
    // AST related queries
    //----------------------------------------------------------------------

    size_t
    GetPointerByteSize () const;
    
    //----------------------------------------------------------------------
    // Accessors
    //----------------------------------------------------------------------
    
    TypeSystem *
    GetTypeSystem() const
    {
        return m_type_system;
    }
    
    ConstString
    GetConstQualifiedTypeName () const;

    ConstString
    GetConstTypeName () const;
    
    ConstString
    GetTypeName () const;

    ConstString
    GetDisplayTypeName () const;

    uint32_t
    GetTypeInfo (CompilerType *pointee_or_element_clang_type = NULL) const;
    
    lldb::LanguageType
    GetMinimumLanguage ();

    void *
    GetOpaqueQualType() const
    {
        return m_type;
    }

    lldb::TypeClass
    GetTypeClass () const;
    
    void
    SetCompilerType (TypeSystem* type_system, void* type);
    void
    SetCompilerType (clang::ASTContext *ast, clang::QualType qual_type);

    unsigned
    GetTypeQualifiers() const;
    
    //----------------------------------------------------------------------
    // Creating related types
    //----------------------------------------------------------------------
    
    CompilerType
    GetArrayElementType (uint64_t *stride = nullptr) const;
    
    CompilerType
    GetCanonicalType () const;
    
    CompilerType
    GetFullyUnqualifiedType () const;
    
    // Returns -1 if this isn't a function of if the function doesn't have a prototype
    // Returns a value >= 0 if there is a prototype.
    int
    GetFunctionArgumentCount () const;

    CompilerType
    GetFunctionArgumentTypeAtIndex (size_t idx) const;

    CompilerType
    GetFunctionReturnType () const;
    
    size_t
    GetNumMemberFunctions () const;
    
    TypeMemberFunctionImpl
    GetMemberFunctionAtIndex (size_t idx);
    
    CompilerType
    GetNonReferenceType () const;

    CompilerType
    GetPointeeType () const;
    
    CompilerType
    GetPointerType () const;

    // If the current object represents a typedef type, get the underlying type
    CompilerType
    GetTypedefedType () const;
    
    //----------------------------------------------------------------------
    // Create related types using the current type's AST
    //----------------------------------------------------------------------
    CompilerType
    GetBasicTypeFromAST (lldb::BasicType basic_type) const;

    //----------------------------------------------------------------------
    // Exploring the type
    //----------------------------------------------------------------------

    uint64_t
    GetByteSize (ExecutionContextScope *exe_scope) const;

    uint64_t
    GetBitSize (ExecutionContextScope *exe_scope) const;

    lldb::Encoding
    GetEncoding (uint64_t &count) const;
    
    lldb::Format
    GetFormat () const;
    
    size_t
    GetTypeBitAlign () const;

    uint32_t
    GetNumChildren (bool omit_empty_base_classes) const;

    lldb::BasicType
    GetBasicTypeEnumeration () const;

    static lldb::BasicType
    GetBasicTypeEnumeration (const ConstString &name);

    //----------------------------------------------------------------------
    // If this type is an enumeration, iterate through all of its enumerators
    // using a callback. If the callback returns true, keep iterating, else
    // abort the iteration.
    //----------------------------------------------------------------------
    void
    ForEachEnumerator (std::function <bool (const CompilerType &integer_type,
                                            const ConstString &name,
                                            const llvm::APSInt &value)> const &callback) const;
    uint32_t
    GetNumFields () const;
    
    CompilerType
    GetFieldAtIndex (size_t idx,
                     std::string& name,
                     uint64_t *bit_offset_ptr,
                     uint32_t *bitfield_bit_size_ptr,
                     bool *is_bitfield_ptr) const;

    uint32_t
    GetNumDirectBaseClasses () const;

    uint32_t
    GetNumVirtualBaseClasses () const;

    CompilerType
    GetDirectBaseClassAtIndex (size_t idx,
                               uint32_t *bit_offset_ptr) const;

    CompilerType
    GetVirtualBaseClassAtIndex (size_t idx,
                                uint32_t *bit_offset_ptr) const;

    uint32_t
    GetIndexOfFieldWithName (const char* name,
                             CompilerType* field_clang_type = NULL,
                             uint64_t *bit_offset_ptr = NULL,
                             uint32_t *bitfield_bit_size_ptr = NULL,
                             bool *is_bitfield_ptr = NULL) const;
    
    CompilerType
    GetChildClangTypeAtIndex (ExecutionContext *exe_ctx,
                              size_t idx,
                              bool transparent_pointers,
                              bool omit_empty_base_classes,
                              bool ignore_array_bounds,
                              std::string& child_name,
                              uint32_t &child_byte_size,
                              int32_t &child_byte_offset,
                              uint32_t &child_bitfield_bit_size,
                              uint32_t &child_bitfield_bit_offset,
                              bool &child_is_base_class,
                              bool &child_is_deref_of_parent,
                              ValueObject *valobj) const;
    
    // Lookup a child given a name. This function will match base class names
    // and member member names in "clang_type" only, not descendants.
    uint32_t
    GetIndexOfChildWithName (const char *name,
                             bool omit_empty_base_classes) const;
    
    // Lookup a child member given a name. This function will match member names
    // only and will descend into "clang_type" children in search for the first
    // member in this class, or any base class that matches "name".
    // TODO: Return all matches for a given name by returning a vector<vector<uint32_t>>
    // so we catch all names that match a given child name, not just the first.
    size_t
    GetIndexOfChildMemberWithName (const char *name,
                                   bool omit_empty_base_classes,
                                   std::vector<uint32_t>& child_indexes) const;
    
    size_t
    GetNumTemplateArguments () const;
    
    CompilerType
    GetTemplateArgument (size_t idx,
                         lldb::TemplateArgumentKind &kind) const;
    
    //------------------------------------------------------------------
    // Pointers & References
    //------------------------------------------------------------------

    // Converts "s" to a floating point value and place resulting floating
    // point bytes in the "dst" buffer.
    size_t
    ConvertStringToFloatValue (const char *s,
                               uint8_t *dst,
                               size_t dst_size) const;
    //----------------------------------------------------------------------
    // Dumping types
    //----------------------------------------------------------------------
    void
    DumpValue (ExecutionContext *exe_ctx,
               Stream *s,
               lldb::Format format,
               const DataExtractor &data,
               lldb::offset_t data_offset,
               size_t data_byte_size,
               uint32_t bitfield_bit_size,
               uint32_t bitfield_bit_offset,
               bool show_types,
               bool show_summary,
               bool verbose,
               uint32_t depth);

    bool
    DumpTypeValue (Stream *s,
                   lldb::Format format,
                   const DataExtractor &data,
                   lldb::offset_t data_offset,
                   size_t data_byte_size,
                   uint32_t bitfield_bit_size,
                   uint32_t bitfield_bit_offset,
                   ExecutionContextScope *exe_scope);
    
    void
    DumpSummary (ExecutionContext *exe_ctx,
                 Stream *s,
                 const DataExtractor &data,
                 lldb::offset_t data_offset,
                 size_t data_byte_size);

    void
    DumpTypeDescription () const; // Dump to stdout

    void
    DumpTypeDescription (Stream *s) const;
    
    bool
    GetValueAsScalar (const DataExtractor &data,
                      lldb::offset_t data_offset,
                      size_t data_byte_size,
                      Scalar &value) const;

    bool
    SetValueFromScalar (const Scalar &value,
                        Stream &strm);

    bool
    ReadFromMemory (ExecutionContext *exe_ctx,
                    lldb::addr_t addr,
                    AddressType address_type,
                    DataExtractor &data);

    bool
    WriteToMemory (ExecutionContext *exe_ctx,
                   lldb::addr_t addr,
                   AddressType address_type,
                   StreamString &new_value);
    
    void
    Clear()
    {
        m_type = NULL;
        m_type_system = NULL;
    }
private:
    void* m_type;
    TypeSystem *m_type_system;
    
};
    
bool operator == (const CompilerType &lhs, const CompilerType &rhs);
bool operator != (const CompilerType &lhs, const CompilerType &rhs);

    
} // namespace lldb_private

#endif // #ifndef liblldb_CompilerType_h_
