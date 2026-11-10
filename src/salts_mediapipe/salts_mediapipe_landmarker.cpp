// Godot image conversion and task setup adapted from GDMP v0.6.
// Copyright (c) 2021-present Jason Kuo & contributors. See the add-on LICENSE.
#include "salts_mediapipe_landmarker.hpp"

#include <absl/status/status.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <mediapipe/framework/formats/image.h>
#include <mediapipe/framework/formats/image_frame.h>
#include <mediapipe/tasks/cc/vision/face_landmarker/face_landmarker.h>
#include <mediapipe/tasks/cc/vision/hand_landmarker/hand_landmarker.h>
#include <mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarker.h>

#include <limits>
#include <string>
#include <utility>
#include <variant>

namespace salts_mediapipe {

using namespace godot;

namespace {

namespace mp = mediapipe;
namespace face = mp::tasks::vision::face_landmarker;
namespace hand = mp::tasks::vision::hand_landmarker;
namespace pose = mp::tasks::vision::pose_landmarker;
using RunningMode = mp::tasks::vision::core::RunningMode;
using NativeTask = std::variant<std::unique_ptr<face::FaceLandmarker>,
                                std::unique_ptr<hand::HandLandmarker>,
                                std::unique_ptr<pose::PoseLandmarker>>;
using Points = TypedArray<PackedVector3Array>;

void report(const absl::Status &status) {
    const auto message = status.message();
    ERR_PRINT(String::utf8(message.data(), message.size()));
}

template <class Task, class Options>
Error create_task(NativeTask &target, std::unique_ptr<Options> options,
                  const PackedByteArray &model, RunningMode mode) {
    options->base_options.model_asset_buffer = std::make_unique<std::string>(
        reinterpret_cast<const char *>(model.ptr()), static_cast<size_t>(model.size()));
    options->running_mode = mode;
    auto task = Task::Create(std::move(options));
    if (!task.ok()) {
        report(task.status());
        return ERR_CANT_CREATE;
    }
    target = std::move(task.value());
    return OK;
}

const auto &landmarks(const face::FaceLandmarkerResult &result) {
    return result.face_landmarks;
}

const auto &landmarks(const hand::HandLandmarkerResult &result) {
    return result.hand_landmarks;
}

const auto &landmarks(const pose::PoseLandmarkerResult &result) {
    return result.pose_landmarks;
}

} // namespace

struct SaltsMediaPipeLandmarker::Session {
    NativeTask task;
    Mode mode = IMAGE;
    int64_t last_timestamp_ms = -1;
};

SaltsMediaPipeLandmarker::SaltsMediaPipeLandmarker() = default;

SaltsMediaPipeLandmarker::~SaltsMediaPipeLandmarker() {
    close();
}

void SaltsMediaPipeLandmarker::_bind_methods() {
    ClassDB::bind_method(D_METHOD("open", "model", "task", "mode", "max_subjects"),
                         &SaltsMediaPipeLandmarker::open, DEFVAL(IMAGE), DEFVAL(1));
    ClassDB::bind_method(D_METHOD("detect", "image", "timestamp_ms"),
                         &SaltsMediaPipeLandmarker::detect, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("close"), &SaltsMediaPipeLandmarker::close);
    ClassDB::bind_method(D_METHOD("get_last_error"), &SaltsMediaPipeLandmarker::get_last_error);
    BIND_ENUM_CONSTANT(FACE);
    BIND_ENUM_CONSTANT(HAND);
    BIND_ENUM_CONSTANT(POSE);
    BIND_ENUM_CONSTANT(IMAGE);
    BIND_ENUM_CONSTANT(VIDEO);
}

Error SaltsMediaPipeLandmarker::open(const PackedByteArray &model, Task task, Mode mode,
                                    int64_t max_subjects) {
    if (session_) {
        return ERR_BUSY;
    }
    if (model.is_empty() || max_subjects < 1 || max_subjects > std::numeric_limits<int>::max() ||
        (mode != IMAGE && mode != VIDEO)) {
        return ERR_INVALID_PARAMETER;
    }
    auto session = std::make_unique<Session>();
    session->mode = mode;
    const auto running_mode = mode == VIDEO ? RunningMode::VIDEO : RunningMode::IMAGE;
    Error error;
    switch (task) {
        case FACE: {
            auto options = std::make_unique<face::FaceLandmarkerOptions>();
            options->num_faces = static_cast<int>(max_subjects);
            error = create_task<face::FaceLandmarker>(session->task, std::move(options), model, running_mode);
            break;
        }
        case HAND: {
            auto options = std::make_unique<hand::HandLandmarkerOptions>();
            options->num_hands = static_cast<int>(max_subjects);
            error = create_task<hand::HandLandmarker>(session->task, std::move(options), model, running_mode);
            break;
        }
        case POSE: {
            auto options = std::make_unique<pose::PoseLandmarkerOptions>();
            options->num_poses = static_cast<int>(max_subjects);
            error = create_task<pose::PoseLandmarker>(session->task, std::move(options), model, running_mode);
            break;
        }
        default:
            return ERR_INVALID_PARAMETER;
    }
    if (error == OK) {
        session_ = std::move(session);
        last_error_ = OK;
    }
    return error;
}

Points SaltsMediaPipeLandmarker::detect(const Ref<Image> &image, int64_t timestamp_ms) {
    Points points;
    if (!session_) {
        last_error_ = ERR_UNCONFIGURED;
        return points;
    }
    const bool video = session_->mode == VIDEO;
    if (video ? timestamp_ms <= session_->last_timestamp_ms ||
                    timestamp_ms > std::numeric_limits<int64_t>::max() / 1000
              : timestamp_ms != -1) {
        last_error_ = ERR_INVALID_PARAMETER;
        return points;
    }
    if (image.is_null() || image->is_empty()) {
        last_error_ = ERR_INVALID_PARAMETER;
        return points;
    }

    mp::ImageFormat::Format format;
    switch (image->get_format()) {
        case Image::FORMAT_RGB8:
            format = mp::ImageFormat::SRGB;
            break;
        case Image::FORMAT_RGBA8:
            format = mp::ImageFormat::SRGBA;
            break;
        default:
            last_error_ = ERR_INVALID_PARAMETER;
            return points;
    }
    const auto bytes = image->get_data();
    auto frame = std::make_shared<mp::ImageFrame>();
    frame->CopyPixelData(format, image->get_width(), image->get_height(), bytes.ptr(),
                         mp::ImageFrame::kGlDefaultAlignmentBoundary);
    mp::Image input(frame);
    session_->last_timestamp_ms = timestamp_ms;
    last_error_ = OK;

    std::visit([&](auto &task) {
        auto result = video ? task->DetectForVideo(input, timestamp_ms) : task->Detect(input);
        if (!result.ok()) {
            report(result.status());
            last_error_ = FAILED;
            return;
        }
        for (const auto &subject : landmarks(result.value())) {
            PackedVector3Array subject_points;
            const auto error = static_cast<Error>(subject_points.resize(subject.landmarks.size()));
            if (error != OK) {
                points.clear();
                last_error_ = error;
                return;
            }
            auto *out = subject_points.ptrw();
            for (const auto &landmark : subject.landmarks) {
                *out++ = Vector3(landmark.x, landmark.y, landmark.z);
            }
            points.append(subject_points);
        }
    }, session_->task);
    return points;
}

Error SaltsMediaPipeLandmarker::close() {
    if (!session_) {
        return OK;
    }
    const auto status = std::visit([](auto &task) { return task->Close(); }, session_->task);
    session_.reset();
    if (!status.ok()) {
        report(status);
        return FAILED;
    }
    return OK;
}

} // namespace salts_mediapipe
