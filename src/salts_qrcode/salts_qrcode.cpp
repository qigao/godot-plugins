#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <ZXingCpp.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <string_view>

namespace {

using namespace godot;

constexpr int quiet_zone_modules = 4;
constexpr int max_texture_side = 16384;
constexpr int default_pixels_per_module = 8;

Ref<Image> render_qr(std::string_view payload, bool is_text, int64_t pixels_per_module) {
    ERR_FAIL_COND_V_MSG(payload.empty(), Ref<Image>(), "QR Code requires a nonempty payload.");
    ERR_FAIL_COND_V_MSG(payload.size() > std::numeric_limits<int>::max(), Ref<Image>(), "QR Code payload is too large.");
    ERR_FAIL_COND_V_MSG(pixels_per_module < 1, Ref<Image>(), "Pixels per module must be positive.");

    try {
        const ZXing::CreatorOptions options(ZXing::BarcodeFormat::QRCode, "ecLevel=M");
        const auto code = is_text ? ZXing::CreateBarcodeFromText(payload, options)
                                  : ZXing::CreateBarcodeFromBytes(payload.data(), static_cast<int>(payload.size()), options);
        const int modules = code.symbol().width() + 2 * quiet_zone_modules;
        ERR_FAIL_COND_V_MSG(pixels_per_module > max_texture_side / modules, Ref<Image>(),
                            "Rendered QR Code exceeds 16384 pixels per side.");

        const auto bitmap = ZXing::WriteBarcodeToImage(
            code, ZXing::WriterOptions().scale(static_cast<int>(pixels_per_module)).addQuietZones(true));
        PackedByteArray pixels;
        const int64_t size = static_cast<int64_t>(bitmap.width()) * bitmap.height();
        ERR_FAIL_COND_V(pixels.resize(size) != OK, Ref<Image>());
        std::memcpy(pixels.ptrw(), bitmap.data(), static_cast<size_t>(size));
        return Image::create_from_data(bitmap.width(), bitmap.height(), false, Image::FORMAT_L8, pixels);
    } catch (const std::exception &error) {
        ERR_FAIL_V_MSG(Ref<Image>(), String::utf8(error.what()));
    }
}

class SaltsQRCode : public RefCounted {
    GDCLASS(SaltsQRCode, RefCounted)

protected:
    static void _bind_methods() {
        ClassDB::bind_static_method(get_class_static(), D_METHOD("render_text", "text", "pixels_per_module"),
                                    &SaltsQRCode::render_text, DEFVAL(default_pixels_per_module));
        ClassDB::bind_static_method(get_class_static(), D_METHOD("render_bytes", "bytes", "pixels_per_module"),
                                    &SaltsQRCode::render_bytes, DEFVAL(default_pixels_per_module));
        ClassDB::bind_static_method(get_class_static(), D_METHOD("decode", "image"), &SaltsQRCode::decode);
    }

public:
    static Ref<Image> render_text(const String &text, int64_t pixels_per_module) {
        const CharString utf8 = text.utf8();
        return render_qr({utf8.get_data(), static_cast<size_t>(utf8.length())}, true, pixels_per_module);
    }

    static Ref<Image> render_bytes(const PackedByteArray &bytes, int64_t pixels_per_module) {
        ERR_FAIL_COND_V_MSG(bytes.is_empty(), Ref<Image>(), "QR Code requires a nonempty payload.");
        return render_qr({reinterpret_cast<const char *>(bytes.ptr()), static_cast<size_t>(bytes.size())},
                         false, pixels_per_module);
    }

    static Dictionary decode(const Ref<Image> &image) {
        if (image.is_null() || image->is_empty())
            return {};

        Ref<Image> input = image;
        auto format = ZXing::ImageFormat::Lum;
        switch (input->get_format()) {
            case Image::FORMAT_L8:
                break;
            case Image::FORMAT_RGB8:
                format = ZXing::ImageFormat::RGB;
                break;
            case Image::FORMAT_RGBA8:
                format = ZXing::ImageFormat::RGBA;
                break;
            default:
                input = image->duplicate();
                if (input.is_null() || (input->is_compressed() && input->decompress() != OK))
                    return {};
                input->convert(Image::FORMAT_L8);
                ERR_FAIL_COND_V(input->get_format() != Image::FORMAT_L8, Dictionary());
        }

        const PackedByteArray pixels = input->get_data();
        ERR_FAIL_COND_V(pixels.size() > std::numeric_limits<int>::max(), Dictionary());
        try {
            const ZXing::ImageView view(pixels.ptr(), static_cast<int>(pixels.size()), input->get_width(),
                                        input->get_height(), format);
            const auto code = ZXing::ReadBarcode(
                view, ZXing::ReaderOptions().formats(ZXing::BarcodeFormat::QRCode).textMode(ZXing::TextMode::Plain));
            if (!code.isValid())
                return {};

            PackedByteArray bytes;
            const auto &payload = code.bytes();
            ERR_FAIL_COND_V(bytes.resize(payload.size()) != OK, Dictionary());
            if (!payload.empty())
                std::memcpy(bytes.ptrw(), payload.data(), payload.size());

            const auto text = code.text();
            Dictionary result;
            result["text"] = String::utf8(text.data(), text.size());
            result["bytes"] = bytes;
            return result;
        } catch (const std::exception &error) {
            ERR_FAIL_V_MSG(Dictionary(), String::utf8(error.what()));
        }
    }
};

void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE)
        GDREGISTER_CLASS(SaltsQRCode);
}

void uninitialize(ModuleInitializationLevel) {}

} // namespace

extern "C" GDExtensionBool GDE_EXPORT salts_qrcode_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address, GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(uninitialize);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
