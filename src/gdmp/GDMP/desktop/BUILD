cc_binary(
    name = "GDMP",
    additional_linker_inputs = select({
        "@platforms//os:linux": ["//GDMP:exports.lds"],
        "//conditions:default": [],
    }),
    linkopts = select({
        "@platforms//os:linux": [
            "-Wl,--version-script=$(location //GDMP:exports.lds)",
            "-Wl,-z,defs",
            "-s",
        ],
        "@platforms//os:macos": ["-Wl,-exported_symbol,_mediapipe_library_init"],
        "//conditions:default": [],
    }),
    linkshared = 1,
    visibility = ["//visibility:public"],
    deps = ["//GDMP"],
)
