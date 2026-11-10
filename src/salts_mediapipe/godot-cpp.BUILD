# Adapted from GDMP v0.6. See the add-on LICENSE.
cc_library(
    name = "godot",
    srcs = glob(["src/**/*.cpp", "gen/src/**/*.cpp"]),
    hdrs = glob(["gdextension/*.h", "include/**/*.hpp", "include/**/*.inc", "gen/include/**/*.hpp"]),
    copts = ["-fno-exceptions", "-fvisibility=hidden", "-include", "cstdlib"],
    includes = ["gdextension", "include", "gen/include"],
    visibility = ["//visibility:public"],
)
