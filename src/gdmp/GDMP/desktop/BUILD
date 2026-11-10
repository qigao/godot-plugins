cc_binary(
    name = "GDMP",
    linkopts = select({
        "@platforms//os:macos": [],
        "@platforms//os:windows": [],
        "//conditions:default": [
            "-Wl,--exclude-libs,ALL",
            "-Wl,-z,defs",
            "-s",
        ],
    }),
    linkshared = 1,
    visibility = ["//visibility:public"],
    deps = ["//GDMP"],
)
