licenses(["notice"])  # MIT

exports_files(["LICENSE"])

cc_library(
    name = "godot",
    srcs = glob([
        "src/*.cpp",
        "src/*/*.cpp",
        "gen/src/*.cpp",
        "gen/src/*/*.cpp",
    ]),
    hdrs = glob([
        "gen/include/gdextension_interface.h",
        "include/**/*.inc",
        "include/godot_cpp/*.hpp",
        "include/godot_cpp/*/*.hpp",
        "gen/include/godot_cpp/*/*.hpp",
    ]),
    copts = select({
        "@platforms//os:windows": [
            "/utf-8",
        ],
        "//conditions:default": [
            "-fno-exceptions",
        ],
    }),
    defines = select({
        "@platforms//os:windows": [
            "_HAS_EXCEPTIONS=0",
            "TYPED_METHOD_BIND",
            "NOMINMAX",
        ],
        "//conditions:default": [],
    }),
    includes = [
        "gen/include",
        "include",
    ],
    visibility = ["//visibility:public"],
)
