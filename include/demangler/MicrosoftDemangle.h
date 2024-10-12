//===------------------------- MicrosoftDemangle.h --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEMANGLE_MICROSOFTDEMANGLE_H
#define LLVM_DEMANGLE_MICROSOFTDEMANGLE_H

#include "demangler/MicrosoftDemangleNodes.h"

#include <string_view>
#include <utility>

namespace demangler {
namespace ms_demangle {
// This memory allocator is extremely fast, but it doesn't call dtors
// for allocated objects. That means you can't use STL containers
// (such as std::vector) with this allocator. But it pays off --
// the demangler is 3x faster with this allocator compared to one with
// STL containers.
constexpr size_t AllocUnit = 4096;

class ArenaAllocator {
    struct AllocatorNode {
        uint8_t*       Buf      = nullptr;
        size_t         Used     = 0;
        size_t         Capacity = 0;
        AllocatorNode* Next     = nullptr;
    };

    void addNode(size_t Capacity) {
        AllocatorNode* NewHead = new AllocatorNode;
        NewHead->Buf           = new uint8_t[Capacity];
        NewHead->Next          = Head;
        NewHead->Capacity      = Capacity;
        Head                   = NewHead;
        NewHead->Used          = 0;
    }

public:
    ArenaAllocator() { addNode(AllocUnit); }

    ~ArenaAllocator() {
        while (Head) {
            DEMANGLE_ASSERT(Head->Buf, "ArenaAllocator::~ArenaAllocator");
            delete[] Head->Buf;
            AllocatorNode* Next = Head->Next;
            delete Head;
            Head = Next;
        }
    }

    // Delete the copy constructor and the copy assignment operator.
    ArenaAllocator(const ArenaAllocator&)            = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    char* allocUnalignedBuffer(size_t Size) {
        DEMANGLE_ASSERT(Head && Head->Buf, "ArenaAllocator::allocUnalignedBuffer");

        uint8_t* P = Head->Buf + Head->Used;

        Head->Used += Size;
        if (Head->Used <= Head->Capacity) return reinterpret_cast<char*>(P);

        addNode(std::max(AllocUnit, Size));
        Head->Used = Size;
        return reinterpret_cast<char*>(Head->Buf);
    }

    template <typename T, typename... Args>
    T* allocArray(size_t Count) {
        size_t Size = Count * sizeof(T);
        DEMANGLE_ASSERT(Head && Head->Buf, "ArenaAllocator::allocArray");

        size_t    P          = (size_t)Head->Buf + Head->Used;
        uintptr_t AlignedP   = (((size_t)P + alignof(T) - 1) & ~(size_t)(alignof(T) - 1));
        uint8_t*  PP         = (uint8_t*)AlignedP;
        size_t    Adjustment = AlignedP - P;

        Head->Used += Size + Adjustment;
        if (Head->Used <= Head->Capacity) return new (PP) T[Count]();

        addNode(std::max(AllocUnit, Size));
        Head->Used = Size;
        return new (Head->Buf) T[Count]();
    }

    template <typename T, typename... Args>
    T* alloc(Args&&... ConstructorArgs) {
        constexpr size_t Size = sizeof(T);
        DEMANGLE_ASSERT(Head && Head->Buf, "ArenaAllocator::alloc");

        size_t    P          = (size_t)Head->Buf + Head->Used;
        uintptr_t AlignedP   = (((size_t)P + alignof(T) - 1) & ~(size_t)(alignof(T) - 1));
        uint8_t*  PP         = (uint8_t*)AlignedP;
        size_t    Adjustment = AlignedP - P;

        Head->Used += Size + Adjustment;
        if (Head->Used <= Head->Capacity) return new (PP) T(std::forward<Args>(ConstructorArgs)...);

        static_assert(Size < AllocUnit);
        addNode(AllocUnit);
        Head->Used = Size;
        return new (Head->Buf) T(std::forward<Args>(ConstructorArgs)...);
    }

private:
    AllocatorNode* Head = nullptr;
};

struct BackrefContext {
    static constexpr size_t Max = 10;

    TypeNode* FunctionParams[Max];
    size_t    FunctionParamCount = 0;

    // The first 10 BackReferences in a mangled name can be back-referenced by
    // special name @[0-9]. This is a storage for the first 10 BackReferences.
    NamedIdentifierNode* Names[Max];
    size_t               NamesCount = 0;
};

enum class QualifierMangleMode { Drop, Mangle, Result };

enum NameBackrefBehavior : uint8_t {
    NBB_None     = 0,      // don't save any names as backrefs.
    NBB_Template = 1 << 0, // save template instanations.
    NBB_Simple   = 1 << 1, // save simple names.
};

enum class FunctionIdentifierCodeGroup { Basic, Under, DoubleUnder };

// Demangler class takes the main role in demangling symbols.
// It has a set of functions to parse mangled symbols into Type instances.
// It also has a set of functions to convert Type instances to strings.
class Demangler {
public:
    Demangler()          = default;
    virtual ~Demangler() = default;

    // You are supposed to call parse() first and then check if error is true.  If
    // it is false, call output() to write the formatted name to the given stream.
    virtual SymbolNode* parse(std::string_view& MangledName);

    virtual TagTypeNode* parseTagUniqueName(std::string_view& MangledName);

    // True if an error occurred.
    bool Error = false;

    virtual void dumpBackReferences();

protected:
    virtual SymbolNode* demangleEncodedSymbol(std::string_view& MangledName, QualifiedNameNode* QN);
    virtual SymbolNode* demangleDeclarator(std::string_view& MangledName);
    virtual SymbolNode* demangleMD5Name(std::string_view& MangledName);
    virtual SymbolNode* demangleTypeinfoName(std::string_view& MangledName);

    virtual VariableSymbolNode* demangleVariableEncoding(std::string_view& MangledName, StorageClass SC);
    virtual FunctionSymbolNode* demangleFunctionEncoding(std::string_view& MangledName);

    virtual Qualifiers demanglePointerExtQualifiers(std::string_view& MangledName);

    // Parser functions. This is a recursive-descent parser.
    virtual TypeNode*              demangleType(std::string_view& MangledName, QualifierMangleMode QMM);
    virtual PrimitiveTypeNode*     demanglePrimitiveType(std::string_view& MangledName);
    virtual CustomTypeNode*        demangleCustomType(std::string_view& MangledName);
    virtual TagTypeNode*           demangleClassType(std::string_view& MangledName);
    virtual PointerTypeNode*       demanglePointerType(std::string_view& MangledName);
    virtual PointerTypeNode*       demangleMemberPointerType(std::string_view& MangledName);
    virtual FunctionSignatureNode* demangleFunctionType(std::string_view& MangledName, bool HasThisQuals);

    virtual ArrayTypeNode* demangleArrayType(std::string_view& MangledName);

    virtual NodeArrayNode* demangleFunctionParameterList(std::string_view& MangledName, bool& IsVariadic);
    virtual NodeArrayNode* demangleTemplateParameterList(std::string_view& MangledName);

    virtual std::pair<uint64_t, bool> demangleNumber(std::string_view& MangledName);
    virtual uint64_t                  demangleUnsigned(std::string_view& MangledName);
    virtual int64_t                   demangleSigned(std::string_view& MangledName);

    virtual void memorizeString(std::string_view s);
    virtual void memorizeIdentifier(IdentifierNode* Identifier);

    /// Allocate a copy of \p Borrowed into memory that we own.
    virtual std::string_view copyString(std::string_view Borrowed);

    virtual QualifiedNameNode* demangleFullyQualifiedTypeName(std::string_view& MangledName);
    virtual QualifiedNameNode* demangleFullyQualifiedSymbolName(std::string_view& MangledName);

    virtual IdentifierNode* demangleUnqualifiedTypeName(std::string_view& MangledName, bool Memorize);
    virtual IdentifierNode* demangleUnqualifiedSymbolName(std::string_view& MangledName, NameBackrefBehavior NBB);

    virtual QualifiedNameNode* demangleNameScopeChain(std::string_view& MangledName, IdentifierNode* UnqualifiedName);
    virtual IdentifierNode*    demangleNameScopePiece(std::string_view& MangledName);

    virtual NamedIdentifierNode* demangleBackRefName(std::string_view& MangledName);
    virtual IdentifierNode* demangleTemplateInstantiationName(std::string_view& MangledName, NameBackrefBehavior NBB);
    virtual IntrinsicFunctionKind translateIntrinsicFunctionCode(char CH, FunctionIdentifierCodeGroup Group);
    virtual IdentifierNode*       demangleFunctionIdentifierCode(std::string_view& MangledName);
    virtual IdentifierNode*
    demangleFunctionIdentifierCode(std::string_view& MangledName, FunctionIdentifierCodeGroup Group);
    virtual StructorIdentifierNode* demangleStructorIdentifier(std::string_view& MangledName, bool IsDestructor);
    virtual ConversionOperatorIdentifierNode* demangleConversionOperatorIdentifier(std::string_view& MangledName);
    virtual LiteralOperatorIdentifierNode*    demangleLiteralOperatorIdentifier(std::string_view& MangledName);

    virtual SymbolNode* demangleSpecialIntrinsic(std::string_view& MangledName);
    virtual SpecialTableSymbolNode*
    demangleSpecialTableSymbolNode(std::string_view& MangledName, SpecialIntrinsicKind SIK);
    virtual LocalStaticGuardVariableNode* demangleLocalStaticGuard(std::string_view& MangledName, bool IsThread);
    virtual VariableSymbolNode* demangleUntypedVariable(std::string_view& MangledName, std::string_view VariableName);
    virtual VariableSymbolNode* demangleRttiBaseClassDescriptorNode(std::string_view& MangledName);
    virtual FunctionSymbolNode* demangleInitFiniStub(std::string_view& MangledName, bool IsDestructor);

    virtual NamedIdentifierNode*      demangleSimpleName(std::string_view& MangledName, bool Memorize);
    virtual NamedIdentifierNode*      demangleAnonymousNamespaceName(std::string_view& MangledName);
    virtual NamedIdentifierNode*      demangleLocallyScopedNamePiece(std::string_view& MangledName);
    virtual EncodedStringLiteralNode* demangleStringLiteral(std::string_view& MangledName);
    virtual FunctionSymbolNode*       demangleVcallThunkNode(std::string_view& MangledName);

    virtual std::string_view demangleSimpleString(std::string_view& MangledName, bool Memorize);

    virtual FuncClass    demangleFunctionClass(std::string_view& MangledName);
    virtual CallingConv  demangleCallingConvention(std::string_view& MangledName);
    virtual StorageClass demangleVariableStorageClass(std::string_view& MangledName);
    virtual bool         demangleThrowSpecification(std::string_view& MangledName);
    virtual wchar_t      demangleWcharLiteral(std::string_view& MangledName);
    virtual uint8_t      demangleCharLiteral(std::string_view& MangledName);

    virtual std::pair<Qualifiers, bool> demangleQualifiers(std::string_view& MangledName);

    // Memory allocator.
    ArenaAllocator Arena;

    // A single type uses one global back-ref table for all function params.
    // This means back-refs can even go "into" other types.  Examples:
    //
    //  // Second int* is a back-ref to first.
    //  void foo(int *, int*);
    //
    //  // Second int* is not a back-ref to first (first is not a function param).
    //  int* foo(int*);
    //
    //  // Second int* is a back-ref to first (ALL function types share the same
    //  // back-ref map.
    //  using F = void(*)(int*);
    //  F G(int *);
    BackrefContext Backrefs;
};

} // namespace ms_demangle
} // namespace demangler

#endif // LLVM_DEMANGLE_MICROSOFTDEMANGLE_H
