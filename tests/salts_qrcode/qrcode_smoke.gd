extends SceneTree

const EXTERNAL_QR_PNG := (
    "iVBORw0KGgoAAAANSUhEUgAAACEAAAAhAQAAAAB/n//CAAAAtklEQVR4nF3OsU7CUACG0a8/l7TEmLaTMn"
     + "FjmBrdWSRx4jU0OvgCDGx18wkIrkwmhMRRE1eegIWwktgODMVoWts0OMNy5uPsgScBcOCufeUh932Uofr5"
     + "zWIwTRBVAOSr0zzW3/UgQP7XfYHz+7juhWRlmMTywrtb+L5sJbFM99VD7nhboLLzY1FjFhWYcr70b3Ty4q"
     + "Zod9G3yF1MLapHHyAUnSOqzwlOvrHDkOzsIYmNnx4/AfgHoN0yxR58njoAAAAASUVORK5CYII="
)


func _initialize() -> void:
    call_deferred("run")


func require(condition: bool, message: String) -> bool:
    if not condition:
        printerr(message)
        quit(1)
    return condition


func run() -> void:
    var text := "https://example.org/二维码/日本語?tag=🧪"
    var qr := SaltsQRCode.render_text(text, 5)
    if not require(qr != null and qr.get_format() == Image.FORMAT_L8, "Expected a grayscale QR image"):
        return
    if not require(qr.get_width() == qr.get_height() and qr.get_width() % 5 == 0 and
            qr.get_pixel(19, 20) == Color.WHITE and qr.get_pixel(20, 20) == Color.BLACK,
            "Expected five-pixel modules and a four-module quiet zone"):
        return
    var decoded := SaltsQRCode.decode(qr)
    if not require(decoded.get("text") == text and decoded.get("bytes") == text.to_utf8_buffer(),
            "UTF-8 round-trip failed"):
        return

    var rgba := qr.duplicate() as Image
    rgba.convert(Image.FORMAT_RGBA8)
    var canvas := Image.create_empty(qr.get_width() + 47, qr.get_height() + 30, false, Image.FORMAT_RGBA8)
    canvas.fill(Color.WHITE)
    canvas.blit_rect(rgba, Rect2i(Vector2i.ZERO, rgba.get_size()), Vector2i(9, 17))
    canvas.rotate_90(CLOCKWISE)
    if not require(SaltsQRCode.decode(canvas).get("text") == text, "Rotated RGBA image decode failed"):
        return

    rgba.convert(Image.FORMAT_LA8)
    var original := rgba.get_data()
    if not require(SaltsQRCode.decode(rgba).get("text") == text and
            rgba.get_format() == Image.FORMAT_LA8 and rgba.get_data() == original,
            "Decoding a converted image changed the source"):
        return

    var binary := PackedByteArray([0, 255, 1, 200, 128, 65, 0, 253])
    var binary_qr := SaltsQRCode.render_bytes(binary)
    if not require(binary_qr != null, "Binary QR generation failed"):
        return
    var inverted := binary_qr.get_data()
    for i in range(inverted.size()):
        inverted[i] = 255 - inverted[i]
    var inverted_qr := Image.create_from_data(
        binary_qr.get_width(), binary_qr.get_height(), false, Image.FORMAT_L8, inverted)
    if not require(SaltsQRCode.decode(inverted_qr).get("bytes") == binary,
            "Inverted QR binary round-trip failed"):
        return

    var blank := Image.create_empty(64, 64, false, Image.FORMAT_RGB8)
    blank.fill(Color.WHITE)
    if not require(SaltsQRCode.decode(blank).is_empty() and SaltsQRCode.decode(null).is_empty(),
            "An image without a code must return an empty dictionary"):
        return

    var external := Image.new()
    if not require(external.load_png_from_buffer(Marshalls.base64_to_raw(EXTERNAL_QR_PNG)) == OK,
            "Independent QR fixture failed to load"):
        return
    external.resize(132, 132, Image.INTERPOLATE_NEAREST)
    if not require(SaltsQRCode.decode(external).get("text") == "independent QR fixture",
            "Independent QR image decode failed"):
        return

    print("Salts QR Code passed text, binary, rotated, inverted, PNG and no-code cases")
    quit(0)
