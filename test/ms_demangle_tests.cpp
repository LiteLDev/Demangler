#include "demangler/Demangle.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool contains(std::string_view Haystack, std::string_view Needle) {
    return Haystack.find(Needle) != std::string_view::npos;
}

bool expectMicrosoftDemangleContains(
    std::string_view Mangled,
    std::string_view Required,
    std::string_view Forbidden
) {
    int   Status    = 0;
    char* Demangled = demangler::microsoftDemangle(Mangled, nullptr, &Status);
    if (Status != demangler::demangle_success || !Demangled) {
        std::cerr << "demangle failed with status " << Status << '\n';
        return false;
    }

    std::string Result = Demangled;
    std::free(Demangled);

    if (!contains(Result, Required)) {
        std::cerr << "missing expected text:\n" << Required << "\n\nactual:\n" << Result << '\n';
        return false;
    }

    if (contains(Result, Forbidden)) {
        std::cerr << "found forbidden text:\n" << Forbidden << "\n\nactual:\n" << Result << '\n';
        return false;
    }

    return true;
}

} // namespace

int main() {
    constexpr std::string_view Mangled =
        ".?AV?$_Func_impl_no_alloc@V<lambda_5>@?0??renderLegacyCubemap@CubeMapAnon@?A0x81341710@@"
        "YAXAEBV?$NonOwnerPointer@VFrameBuilder@framebuilder@mce@@@Bedrock@@AEAVScreenContext@@"
        "V?$span@$$CBVTexturePtr@mce@@$05@gsl@@AEBVMatrix@@AEBVMaterialPtr@mce@@"
        "MAEBUCloudParameters@CubeMap@@_N6@Z@XAEBV6@AEBVMesh@mce@@AEBVMaterialPtr@mce@@"
        "AEBV?$variant@Umonostate@std@@VTexturePtr@mce@@UClientTexture@4@UServerTexture@4@@std@@@std@@";

    return expectMicrosoftDemangleContains(
               Mangled,
               "_Func_impl_no_alloc<class `void __cdecl `anonymous namespace'::CubeMapAnon::renderLegacyCubemap"
               "(class Bedrock::NonOwnerPointer<class mce::framebuilder::FrameBuilder> const &, "
               "class ScreenContext &, class gsl::span<class mce::TexturePtr const, 6>, class Matrix const &, "
               "class mce::MaterialPtr const &, float, struct CubeMap::CloudParameters const &, bool, bool)'"
               "::`1'::<lambda_5>, void, class ScreenContext const &, class mce::Mesh const &, "
               "class mce::MaterialPtr const &, class std::variant<struct std::monostate, "
               "class mce::TexturePtr, struct mce::ClientTexture, struct mce::ServerTexture> const &>",
               "void, class Bedrock const &, class mce::Mesh"
           )
               ? 0
               : 1;
}
