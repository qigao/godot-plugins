cc_library(
    name = "asset_util",
    srcs = ["asset_util.cc"],
    hdrs = ["asset_util.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@mediapipe//mediapipe/util/android:asset_manager_util",
    ],
    alwayslink = 1,
)

cc_library(
    name = "jni",
    srcs = ["jni.cc"],
    hdrs = ["jni.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":asset_util",
        "@godot-cpp//:godot",
    ],
    alwayslink = 1,
)

cc_binary(
    name = "GDMP",
    additional_linker_inputs = ["//GDMP:exports.lds"],
    linkopts = [
        "-Wl,--version-script=$(location //GDMP:exports.lds)",
        "-Wl,-z,defs",
        "-Wl,-z,max-page-size=16384",
        "-Wl,-z,common-page-size=16384",
        "-s",
    ],
    linkshared = 1,
    visibility = ["//visibility:public"],
    deps = [
        ":jni",
        "//GDMP",
    ],
)
