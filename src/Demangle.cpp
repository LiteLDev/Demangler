//===-- Demangle.cpp - Common demangling functions ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file This file contains definitions of common demangling functions.
///
//===----------------------------------------------------------------------===//

#include "demangler/Demangle.h"
#include "demangler/StringViewExtras.h"
#include <cstdlib>
#include <string_view>

using demangler::itanium_demangle::starts_with;

std::string demangler::demangle(std::string_view MangledName) {
    std::string Result;

    if (nonMicrosoftDemangle(MangledName, Result)) return Result;

    if (starts_with(MangledName, '_')
        && nonMicrosoftDemangle(
            MangledName.substr(1),
            Result,
            /*CanHaveLeadingDot=*/false
        ))
        return Result;

    if (char* Demangled = microsoftDemangle(MangledName, nullptr, nullptr)) {
        Result = Demangled;
        std::free(Demangled);
    } else {
        Result = MangledName;
    }
    return Result;
}

static bool isItaniumEncoding(std::string_view S) {
    // Itanium demangler supports prefixes with 1-4 underscores.
    const size_t Pos = S.find_first_not_of('_');
    return Pos > 0 && Pos <= 4 && S[Pos] == 'Z';
}

static bool isRustEncoding(std::string_view S) { return starts_with(S, "_R"); }

static bool isDLangEncoding(std::string_view S) { return starts_with(S, "_D"); }

bool demangler::nonMicrosoftDemangle(
    std::string_view MangledName,
    std::string&     Result,
    bool             CanHaveLeadingDot,
    bool             ParseParams
) {
    char* Demangled = nullptr;

    // Do not consider the dot prefix as part of the demangled symbol name.
    if (CanHaveLeadingDot && MangledName.size() > 0 && MangledName[0] == '.') {
        MangledName.remove_prefix(1);
        Result = ".";
    }

    if (isItaniumEncoding(MangledName)) Demangled = itaniumDemangle(MangledName, ParseParams);
    else if (isRustEncoding(MangledName)) Demangled = rustDemangle(MangledName);
    else if (isDLangEncoding(MangledName)) Demangled = dlangDemangle(MangledName);

    if (!Demangled) return false;

    Result += Demangled;
    std::free(Demangled);
    return true;
}
