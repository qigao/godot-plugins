#ifndef GDMP_TASK_VISION_INTERACTIVE_SEGMENTER
#define GDMP_TASK_VISION_INTERACTIVE_SEGMENTER

#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "godot_cpp/variant/rect2.hpp"
#include "godot_cpp/variant/vector2.hpp"

#if GDMP_MEDIAPIPE_SDK_1_2_2
#include "mediapipe/tasks/cc/vision/interactive_segmenter_legacy/interactive_segmenter_legacy.h"
#else
#include "mediapipe/tasks/cc/vision/interactive_segmenter/interactive_segmenter.h"
#endif

#include "GDMP/framework/image.h"
#include "GDMP/tasks/vision/image_segmenter.h"
#include "GDMP/tasks/vision/vision_task.h"

using namespace godot;
#if GDMP_MEDIAPIPE_SDK_1_2_2
using namespace mediapipe::tasks::vision::interactive_segmenter_legacy;
using InteractiveSegmenter = InteractiveSegmenterLegacy;
using InteractiveSegmenterOptions = InteractiveSegmenterLegacyOptions;
#else
using namespace mediapipe::tasks::vision::interactive_segmenter;
#endif

class MediaPipeInteractiveSegmenter : public MediaPipeVisionTask {
		GDMP_VISION_TASK_CLASS(MediaPipeInteractiveSegmenter, InteractiveSegmenter)

	protected:
		static void _bind_methods();

	public:
		bool initialize(
				Ref<MediaPipeTaskBaseOptions> base_options,
				bool output_confidence_masks, bool output_category_mask);
		Ref<MediaPipeImageSegmenterResult> segment(
				Ref<MediaPipeImage> image, Vector2 keypoint, PackedVector2Array scribble,
				Rect2 region_of_interest, int rotation_degrees);
};

#endif
