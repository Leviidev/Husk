// Camera compatibility implementation; remaining uncommon platform members retain their stubs.
package android.hardware.camera2;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public final class CameraCharacteristics extends android.hardware.camera2.CameraMetadata {
    public final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    static {
        AUTOMOTIVE_LENS_FACING = new Key(husk.CameraKeys.name("AUTOMOTIVE_LENS_FACING"), Object.class);
        AUTOMOTIVE_LOCATION = new Key(husk.CameraKeys.name("AUTOMOTIVE_LOCATION"), Object.class);
        COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES = new Key(husk.CameraKeys.name("COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES"), Object.class);
        COLOR_CORRECTION_AVAILABLE_MODES = new Key(husk.CameraKeys.name("COLOR_CORRECTION_AVAILABLE_MODES"), Object.class);
        COLOR_CORRECTION_COLOR_TEMPERATURE_RANGE = new Key(husk.CameraKeys.name("COLOR_CORRECTION_COLOR_TEMPERATURE_RANGE"), Object.class);
        CONTROL_AE_AVAILABLE_ANTIBANDING_MODES = new Key(husk.CameraKeys.name("CONTROL_AE_AVAILABLE_ANTIBANDING_MODES"), Object.class);
        CONTROL_AE_AVAILABLE_MODES = new Key(husk.CameraKeys.name("CONTROL_AE_AVAILABLE_MODES"), Object.class);
        CONTROL_AE_AVAILABLE_PRIORITY_MODES = new Key(husk.CameraKeys.name("CONTROL_AE_AVAILABLE_PRIORITY_MODES"), Object.class);
        CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES = new Key(husk.CameraKeys.name("CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES"), Object.class);
        CONTROL_AE_COMPENSATION_RANGE = new Key(husk.CameraKeys.name("CONTROL_AE_COMPENSATION_RANGE"), Object.class);
        CONTROL_AE_COMPENSATION_STEP = new Key(husk.CameraKeys.name("CONTROL_AE_COMPENSATION_STEP"), Object.class);
        CONTROL_AE_LOCK_AVAILABLE = new Key(husk.CameraKeys.name("CONTROL_AE_LOCK_AVAILABLE"), Object.class);
        CONTROL_AF_AVAILABLE_MODES = new Key(husk.CameraKeys.name("CONTROL_AF_AVAILABLE_MODES"), Object.class);
        CONTROL_AUTOFRAMING_AVAILABLE = new Key(husk.CameraKeys.name("CONTROL_AUTOFRAMING_AVAILABLE"), Object.class);
        CONTROL_AVAILABLE_EFFECTS = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_EFFECTS"), Object.class);
        CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_CAPABILITIES = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_CAPABILITIES"), Object.class);
        CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_MAX_SIZES = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_MAX_SIZES"), Object.class);
        CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_ZOOM_RATIO_RANGES = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_ZOOM_RATIO_RANGES"), Object.class);
        CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS"), Object.class);
        CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        CONTROL_AVAILABLE_MODES = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_MODES"), Object.class);
        CONTROL_AVAILABLE_SCENE_MODES = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_SCENE_MODES"), Object.class);
        CONTROL_AVAILABLE_SETTINGS_OVERRIDES = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_SETTINGS_OVERRIDES"), Object.class);
        CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES = new Key(husk.CameraKeys.name("CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES"), Object.class);
        CONTROL_AWB_AVAILABLE_MODES = new Key(husk.CameraKeys.name("CONTROL_AWB_AVAILABLE_MODES"), Object.class);
        CONTROL_AWB_LOCK_AVAILABLE = new Key(husk.CameraKeys.name("CONTROL_AWB_LOCK_AVAILABLE"), Object.class);
        CONTROL_LOW_LIGHT_BOOST_INFO_LUMINANCE_RANGE = new Key(husk.CameraKeys.name("CONTROL_LOW_LIGHT_BOOST_INFO_LUMINANCE_RANGE"), Object.class);
        CONTROL_MAX_REGIONS = new Key(husk.CameraKeys.name("CONTROL_MAX_REGIONS"), Object.class);
        CONTROL_MAX_REGIONS_AE = new Key(husk.CameraKeys.name("CONTROL_MAX_REGIONS_AE"), Object.class);
        CONTROL_MAX_REGIONS_AF = new Key(husk.CameraKeys.name("CONTROL_MAX_REGIONS_AF"), Object.class);
        CONTROL_MAX_REGIONS_AWB = new Key(husk.CameraKeys.name("CONTROL_MAX_REGIONS_AWB"), Object.class);
        CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE = new Key(husk.CameraKeys.name("CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE"), Object.class);
        CONTROL_ZOOM_RATIO_RANGE = new Key(husk.CameraKeys.name("CONTROL_ZOOM_RATIO_RANGE"), Object.class);
        DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS"), Object.class);
        DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS"), Object.class);
        DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS"), Object.class);
        DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS"), Object.class);
        DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS"), Object.class);
        DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS"), Object.class);
        DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        DEPTH_AVAILABLE_RECOMMENDED_DEPTH_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("DEPTH_AVAILABLE_RECOMMENDED_DEPTH_STREAM_CONFIGURATIONS"), Object.class);
        DEPTH_DEPTH_IS_EXCLUSIVE = new Key(husk.CameraKeys.name("DEPTH_DEPTH_IS_EXCLUSIVE"), Object.class);
        DISTORTION_CORRECTION_AVAILABLE_MODES = new Key(husk.CameraKeys.name("DISTORTION_CORRECTION_AVAILABLE_MODES"), Object.class);
        EDGE_AVAILABLE_EDGE_MODES = new Key(husk.CameraKeys.name("EDGE_AVAILABLE_EDGE_MODES"), Object.class);
        FLASH_INFO_AVAILABLE = new Key(husk.CameraKeys.name("FLASH_INFO_AVAILABLE"), Object.class);
        FLASH_INFO_STRENGTH_DEFAULT_LEVEL = new Key(husk.CameraKeys.name("FLASH_INFO_STRENGTH_DEFAULT_LEVEL"), Object.class);
        FLASH_INFO_STRENGTH_MAXIMUM_LEVEL = new Key(husk.CameraKeys.name("FLASH_INFO_STRENGTH_MAXIMUM_LEVEL"), Object.class);
        FLASH_SINGLE_STRENGTH_DEFAULT_LEVEL = new Key(husk.CameraKeys.name("FLASH_SINGLE_STRENGTH_DEFAULT_LEVEL"), Object.class);
        FLASH_SINGLE_STRENGTH_MAX_LEVEL = new Key(husk.CameraKeys.name("FLASH_SINGLE_STRENGTH_MAX_LEVEL"), Object.class);
        FLASH_TORCH_STRENGTH_DEFAULT_LEVEL = new Key(husk.CameraKeys.name("FLASH_TORCH_STRENGTH_DEFAULT_LEVEL"), Object.class);
        FLASH_TORCH_STRENGTH_MAX_LEVEL = new Key(husk.CameraKeys.name("FLASH_TORCH_STRENGTH_MAX_LEVEL"), Object.class);
        HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS"), Object.class);
        HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        HEIC_AVAILABLE_HEIC_STALL_DURATIONS = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_STALL_DURATIONS"), Object.class);
        HEIC_AVAILABLE_HEIC_STALL_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_STALL_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS"), Object.class);
        HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        HEIC_AVAILABLE_HEIC_ULTRA_HDR_MIN_FRAME_DURATIONS = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_ULTRA_HDR_MIN_FRAME_DURATIONS"), Object.class);
        HEIC_AVAILABLE_HEIC_ULTRA_HDR_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_ULTRA_HDR_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        HEIC_AVAILABLE_HEIC_ULTRA_HDR_STALL_DURATIONS = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_ULTRA_HDR_STALL_DURATIONS"), Object.class);
        HEIC_AVAILABLE_HEIC_ULTRA_HDR_STALL_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_ULTRA_HDR_STALL_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        HEIC_AVAILABLE_HEIC_ULTRA_HDR_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_ULTRA_HDR_STREAM_CONFIGURATIONS"), Object.class);
        HEIC_AVAILABLE_HEIC_ULTRA_HDR_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("HEIC_AVAILABLE_HEIC_ULTRA_HDR_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES = new Key(husk.CameraKeys.name("HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES"), Object.class);
        INFO_DEVICE_ID = new Key(husk.CameraKeys.name("INFO_DEVICE_ID"), Object.class);
        INFO_DEVICE_STATE_ORIENTATIONS = new Key(husk.CameraKeys.name("INFO_DEVICE_STATE_ORIENTATIONS"), Object.class);
        INFO_DEVICE_STATE_SENSOR_ORIENTATION_MAP = new Key(husk.CameraKeys.name("INFO_DEVICE_STATE_SENSOR_ORIENTATION_MAP"), Object.class);
        INFO_SESSION_CONFIGURATION_QUERY_VERSION = new Key(husk.CameraKeys.name("INFO_SESSION_CONFIGURATION_QUERY_VERSION"), Object.class);
        INFO_SUPPORTED_HARDWARE_LEVEL = new Key(husk.CameraKeys.name("INFO_SUPPORTED_HARDWARE_LEVEL"), Object.class);
        INFO_VERSION = new Key(husk.CameraKeys.name("INFO_VERSION"), Object.class);
        JPEGR_AVAILABLE_JPEG_R_MIN_FRAME_DURATIONS = new Key(husk.CameraKeys.name("JPEGR_AVAILABLE_JPEG_R_MIN_FRAME_DURATIONS"), Object.class);
        JPEGR_AVAILABLE_JPEG_R_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("JPEGR_AVAILABLE_JPEG_R_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        JPEGR_AVAILABLE_JPEG_R_STALL_DURATIONS = new Key(husk.CameraKeys.name("JPEGR_AVAILABLE_JPEG_R_STALL_DURATIONS"), Object.class);
        JPEGR_AVAILABLE_JPEG_R_STALL_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("JPEGR_AVAILABLE_JPEG_R_STALL_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        JPEGR_AVAILABLE_JPEG_R_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("JPEGR_AVAILABLE_JPEG_R_STREAM_CONFIGURATIONS"), Object.class);
        JPEGR_AVAILABLE_JPEG_R_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("JPEGR_AVAILABLE_JPEG_R_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        JPEG_AVAILABLE_THUMBNAIL_SIZES = new Key(husk.CameraKeys.name("JPEG_AVAILABLE_THUMBNAIL_SIZES"), Object.class);
        LED_AVAILABLE_LEDS = new Key(husk.CameraKeys.name("LED_AVAILABLE_LEDS"), Object.class);
        LENS_DISTORTION = new Key(husk.CameraKeys.name("LENS_DISTORTION"), Object.class);
        LENS_DISTORTION_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("LENS_DISTORTION_MAXIMUM_RESOLUTION"), Object.class);
        LENS_FACING = new Key(husk.CameraKeys.name("LENS_FACING"), Object.class);
        LENS_INFO_AVAILABLE_APERTURES = new Key(husk.CameraKeys.name("LENS_INFO_AVAILABLE_APERTURES"), Object.class);
        LENS_INFO_AVAILABLE_FILTER_DENSITIES = new Key(husk.CameraKeys.name("LENS_INFO_AVAILABLE_FILTER_DENSITIES"), Object.class);
        LENS_INFO_AVAILABLE_FOCAL_LENGTHS = new Key(husk.CameraKeys.name("LENS_INFO_AVAILABLE_FOCAL_LENGTHS"), Object.class);
        LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION = new Key(husk.CameraKeys.name("LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION"), Object.class);
        LENS_INFO_FOCUS_DISTANCE_CALIBRATION = new Key(husk.CameraKeys.name("LENS_INFO_FOCUS_DISTANCE_CALIBRATION"), Object.class);
        LENS_INFO_HYPERFOCAL_DISTANCE = new Key(husk.CameraKeys.name("LENS_INFO_HYPERFOCAL_DISTANCE"), Object.class);
        LENS_INFO_MINIMUM_FOCUS_DISTANCE = new Key(husk.CameraKeys.name("LENS_INFO_MINIMUM_FOCUS_DISTANCE"), Object.class);
        LENS_INFO_SHADING_MAP_SIZE = new Key(husk.CameraKeys.name("LENS_INFO_SHADING_MAP_SIZE"), Object.class);
        LENS_INTRINSIC_CALIBRATION = new Key(husk.CameraKeys.name("LENS_INTRINSIC_CALIBRATION"), Object.class);
        LENS_INTRINSIC_CALIBRATION_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("LENS_INTRINSIC_CALIBRATION_MAXIMUM_RESOLUTION"), Object.class);
        LENS_POSE_REFERENCE = new Key(husk.CameraKeys.name("LENS_POSE_REFERENCE"), Object.class);
        LENS_POSE_ROTATION = new Key(husk.CameraKeys.name("LENS_POSE_ROTATION"), Object.class);
        LENS_POSE_TRANSLATION = new Key(husk.CameraKeys.name("LENS_POSE_TRANSLATION"), Object.class);
        LENS_RADIAL_DISTORTION = new Key(husk.CameraKeys.name("LENS_RADIAL_DISTORTION"), Object.class);
        LOGICAL_MULTI_CAMERA_PHYSICAL_IDS = new Key(husk.CameraKeys.name("LOGICAL_MULTI_CAMERA_PHYSICAL_IDS"), Object.class);
        LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE = new Key(husk.CameraKeys.name("LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE"), Object.class);
        NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES = new Key(husk.CameraKeys.name("NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES"), Object.class);
        QUIRKS_USE_PARTIAL_RESULT = new Key(husk.CameraKeys.name("QUIRKS_USE_PARTIAL_RESULT"), Object.class);
        REPROCESS_MAX_CAPTURE_STALL = new Key(husk.CameraKeys.name("REPROCESS_MAX_CAPTURE_STALL"), Object.class);
        REQUEST_AVAILABLE_CAPABILITIES = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_CAPABILITIES"), Object.class);
        REQUEST_AVAILABLE_CHARACTERISTICS_KEYS = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_CHARACTERISTICS_KEYS"), Object.class);
        REQUEST_AVAILABLE_COLOR_SPACE_PROFILES = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_COLOR_SPACE_PROFILES"), Object.class);
        REQUEST_AVAILABLE_COLOR_SPACE_PROFILES_MAP = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_COLOR_SPACE_PROFILES_MAP"), Object.class);
        REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES"), Object.class);
        REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP"), Object.class);
        REQUEST_AVAILABLE_PHYSICAL_CAMERA_REQUEST_KEYS = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_PHYSICAL_CAMERA_REQUEST_KEYS"), Object.class);
        REQUEST_AVAILABLE_REQUEST_KEYS = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_REQUEST_KEYS"), Object.class);
        REQUEST_AVAILABLE_RESULT_KEYS = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_RESULT_KEYS"), Object.class);
        REQUEST_AVAILABLE_SESSION_KEYS = new Key(husk.CameraKeys.name("REQUEST_AVAILABLE_SESSION_KEYS"), Object.class);
        REQUEST_CHARACTERISTIC_KEYS_NEEDING_PERMISSION = new Key(husk.CameraKeys.name("REQUEST_CHARACTERISTIC_KEYS_NEEDING_PERMISSION"), Object.class);
        REQUEST_MAX_NUM_INPUT_STREAMS = new Key(husk.CameraKeys.name("REQUEST_MAX_NUM_INPUT_STREAMS"), Object.class);
        REQUEST_MAX_NUM_OUTPUT_PROC = new Key(husk.CameraKeys.name("REQUEST_MAX_NUM_OUTPUT_PROC"), Object.class);
        REQUEST_MAX_NUM_OUTPUT_PROC_STALLING = new Key(husk.CameraKeys.name("REQUEST_MAX_NUM_OUTPUT_PROC_STALLING"), Object.class);
        REQUEST_MAX_NUM_OUTPUT_RAW = new Key(husk.CameraKeys.name("REQUEST_MAX_NUM_OUTPUT_RAW"), Object.class);
        REQUEST_MAX_NUM_OUTPUT_STREAMS = new Key(husk.CameraKeys.name("REQUEST_MAX_NUM_OUTPUT_STREAMS"), Object.class);
        REQUEST_PARTIAL_RESULT_COUNT = new Key(husk.CameraKeys.name("REQUEST_PARTIAL_RESULT_COUNT"), Object.class);
        REQUEST_PIPELINE_MAX_DEPTH = new Key(husk.CameraKeys.name("REQUEST_PIPELINE_MAX_DEPTH"), Object.class);
        REQUEST_RECOMMENDED_TEN_BIT_DYNAMIC_RANGE_PROFILE = new Key(husk.CameraKeys.name("REQUEST_RECOMMENDED_TEN_BIT_DYNAMIC_RANGE_PROFILE"), Object.class);
        SCALER_AVAILABLE_FORMATS = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_FORMATS"), Object.class);
        SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP"), Object.class);
        SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP_MAXIMUM_RESOLUTION"), Object.class);
        SCALER_AVAILABLE_JPEG_MIN_DURATIONS = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_JPEG_MIN_DURATIONS"), Object.class);
        SCALER_AVAILABLE_JPEG_SIZES = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_JPEG_SIZES"), Object.class);
        SCALER_AVAILABLE_MAX_DIGITAL_ZOOM = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_MAX_DIGITAL_ZOOM"), Object.class);
        SCALER_AVAILABLE_MIN_FRAME_DURATIONS = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_MIN_FRAME_DURATIONS"), Object.class);
        SCALER_AVAILABLE_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS"), Object.class);
        SCALER_AVAILABLE_PROCESSED_SIZES = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_PROCESSED_SIZES"), Object.class);
        SCALER_AVAILABLE_RECOMMENDED_INPUT_OUTPUT_FORMATS_MAP = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_RECOMMENDED_INPUT_OUTPUT_FORMATS_MAP"), Object.class);
        SCALER_AVAILABLE_RECOMMENDED_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_RECOMMENDED_STREAM_CONFIGURATIONS"), Object.class);
        SCALER_AVAILABLE_ROTATE_AND_CROP_MODES = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_ROTATE_AND_CROP_MODES"), Object.class);
        SCALER_AVAILABLE_STALL_DURATIONS = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_STALL_DURATIONS"), Object.class);
        SCALER_AVAILABLE_STALL_DURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_STALL_DURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        SCALER_AVAILABLE_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_STREAM_CONFIGURATIONS"), Object.class);
        SCALER_AVAILABLE_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION"), Object.class);
        SCALER_AVAILABLE_STREAM_USE_CASES = new Key(husk.CameraKeys.name("SCALER_AVAILABLE_STREAM_USE_CASES"), Object.class);
        SCALER_CROPPING_TYPE = new Key(husk.CameraKeys.name("SCALER_CROPPING_TYPE"), Object.class);
        SCALER_DEFAULT_SECURE_IMAGE_SIZE = new Key(husk.CameraKeys.name("SCALER_DEFAULT_SECURE_IMAGE_SIZE"), Object.class);
        SCALER_MANDATORY_CONCURRENT_STREAM_COMBINATIONS = new Key(husk.CameraKeys.name("SCALER_MANDATORY_CONCURRENT_STREAM_COMBINATIONS"), Object.class);
        SCALER_MANDATORY_MAXIMUM_RESOLUTION_STREAM_COMBINATIONS = new Key(husk.CameraKeys.name("SCALER_MANDATORY_MAXIMUM_RESOLUTION_STREAM_COMBINATIONS"), Object.class);
        SCALER_MANDATORY_PREVIEW_STABILIZATION_OUTPUT_STREAM_COMBINATIONS = new Key(husk.CameraKeys.name("SCALER_MANDATORY_PREVIEW_STABILIZATION_OUTPUT_STREAM_COMBINATIONS"), Object.class);
        SCALER_MANDATORY_STREAM_COMBINATIONS = new Key(husk.CameraKeys.name("SCALER_MANDATORY_STREAM_COMBINATIONS"), Object.class);
        SCALER_MANDATORY_TEN_BIT_OUTPUT_STREAM_COMBINATIONS = new Key(husk.CameraKeys.name("SCALER_MANDATORY_TEN_BIT_OUTPUT_STREAM_COMBINATIONS"), Object.class);
        SCALER_MANDATORY_USE_CASE_STREAM_COMBINATIONS = new Key(husk.CameraKeys.name("SCALER_MANDATORY_USE_CASE_STREAM_COMBINATIONS"), Object.class);
        SCALER_MULTI_RESOLUTION_STREAM_CONFIGURATION_MAP = new Key(husk.CameraKeys.name("SCALER_MULTI_RESOLUTION_STREAM_CONFIGURATION_MAP"), Object.class);
        SCALER_MULTI_RESOLUTION_STREAM_SUPPORTED = new Key(husk.CameraKeys.name("SCALER_MULTI_RESOLUTION_STREAM_SUPPORTED"), Object.class);
        SCALER_PHYSICAL_CAMERA_MULTI_RESOLUTION_STREAM_CONFIGURATIONS = new Key(husk.CameraKeys.name("SCALER_PHYSICAL_CAMERA_MULTI_RESOLUTION_STREAM_CONFIGURATIONS"), Object.class);
        SCALER_STREAM_CONFIGURATION_MAP = new Key(husk.CameraKeys.name("SCALER_STREAM_CONFIGURATION_MAP"), Object.class);
        SCALER_STREAM_CONFIGURATION_MAP_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SCALER_STREAM_CONFIGURATION_MAP_MAXIMUM_RESOLUTION"), Object.class);
        SENSOR_AVAILABLE_TEST_PATTERN_MODES = new Key(husk.CameraKeys.name("SENSOR_AVAILABLE_TEST_PATTERN_MODES"), Object.class);
        SENSOR_BLACK_LEVEL_PATTERN = new Key(husk.CameraKeys.name("SENSOR_BLACK_LEVEL_PATTERN"), Object.class);
        SENSOR_CALIBRATION_TRANSFORM1 = new Key(husk.CameraKeys.name("SENSOR_CALIBRATION_TRANSFORM1"), Object.class);
        SENSOR_CALIBRATION_TRANSFORM2 = new Key(husk.CameraKeys.name("SENSOR_CALIBRATION_TRANSFORM2"), Object.class);
        SENSOR_COLOR_TRANSFORM1 = new Key(husk.CameraKeys.name("SENSOR_COLOR_TRANSFORM1"), Object.class);
        SENSOR_COLOR_TRANSFORM2 = new Key(husk.CameraKeys.name("SENSOR_COLOR_TRANSFORM2"), Object.class);
        SENSOR_FORWARD_MATRIX1 = new Key(husk.CameraKeys.name("SENSOR_FORWARD_MATRIX1"), Object.class);
        SENSOR_FORWARD_MATRIX2 = new Key(husk.CameraKeys.name("SENSOR_FORWARD_MATRIX2"), Object.class);
        SENSOR_INFO_ACTIVE_ARRAY_SIZE = new Key(husk.CameraKeys.name("SENSOR_INFO_ACTIVE_ARRAY_SIZE"), Object.class);
        SENSOR_INFO_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SENSOR_INFO_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION"), Object.class);
        SENSOR_INFO_BINNING_FACTOR = new Key(husk.CameraKeys.name("SENSOR_INFO_BINNING_FACTOR"), Object.class);
        SENSOR_INFO_COLOR_FILTER_ARRANGEMENT = new Key(husk.CameraKeys.name("SENSOR_INFO_COLOR_FILTER_ARRANGEMENT"), Object.class);
        SENSOR_INFO_EXPOSURE_TIME_RANGE = new Key(husk.CameraKeys.name("SENSOR_INFO_EXPOSURE_TIME_RANGE"), Object.class);
        SENSOR_INFO_LENS_SHADING_APPLIED = new Key(husk.CameraKeys.name("SENSOR_INFO_LENS_SHADING_APPLIED"), Object.class);
        SENSOR_INFO_MAX_FRAME_DURATION = new Key(husk.CameraKeys.name("SENSOR_INFO_MAX_FRAME_DURATION"), Object.class);
        SENSOR_INFO_PHYSICAL_SIZE = new Key(husk.CameraKeys.name("SENSOR_INFO_PHYSICAL_SIZE"), Object.class);
        SENSOR_INFO_PIXEL_ARRAY_SIZE = new Key(husk.CameraKeys.name("SENSOR_INFO_PIXEL_ARRAY_SIZE"), Object.class);
        SENSOR_INFO_PIXEL_ARRAY_SIZE_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SENSOR_INFO_PIXEL_ARRAY_SIZE_MAXIMUM_RESOLUTION"), Object.class);
        SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE = new Key(husk.CameraKeys.name("SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE"), Object.class);
        SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION = new Key(husk.CameraKeys.name("SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION"), Object.class);
        SENSOR_INFO_SENSITIVITY_RANGE = new Key(husk.CameraKeys.name("SENSOR_INFO_SENSITIVITY_RANGE"), Object.class);
        SENSOR_INFO_TIMESTAMP_SOURCE = new Key(husk.CameraKeys.name("SENSOR_INFO_TIMESTAMP_SOURCE"), Object.class);
        SENSOR_INFO_WHITE_LEVEL = new Key(husk.CameraKeys.name("SENSOR_INFO_WHITE_LEVEL"), Object.class);
        SENSOR_MAX_ANALOG_SENSITIVITY = new Key(husk.CameraKeys.name("SENSOR_MAX_ANALOG_SENSITIVITY"), Object.class);
        SENSOR_OPTICAL_BLACK_REGIONS = new Key(husk.CameraKeys.name("SENSOR_OPTICAL_BLACK_REGIONS"), Object.class);
        SENSOR_ORIENTATION = new Key(husk.CameraKeys.name("SENSOR_ORIENTATION"), Object.class);
        SENSOR_READOUT_TIMESTAMP = new Key(husk.CameraKeys.name("SENSOR_READOUT_TIMESTAMP"), Object.class);
        SENSOR_REFERENCE_ILLUMINANT1 = new Key(husk.CameraKeys.name("SENSOR_REFERENCE_ILLUMINANT1"), Object.class);
        SENSOR_REFERENCE_ILLUMINANT2 = new Key(husk.CameraKeys.name("SENSOR_REFERENCE_ILLUMINANT2"), Object.class);
        SHADING_AVAILABLE_MODES = new Key(husk.CameraKeys.name("SHADING_AVAILABLE_MODES"), Object.class);
        SHARED_SESSION_COLOR_SPACE = new Key(husk.CameraKeys.name("SHARED_SESSION_COLOR_SPACE"), Object.class);
        SHARED_SESSION_CONFIGURATION = new Key(husk.CameraKeys.name("SHARED_SESSION_CONFIGURATION"), Object.class);
        SHARED_SESSION_OUTPUT_CONFIGURATIONS = new Key(husk.CameraKeys.name("SHARED_SESSION_OUTPUT_CONFIGURATIONS"), Object.class);
        STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES = new Key(husk.CameraKeys.name("STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES"), Object.class);
        STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES = new Key(husk.CameraKeys.name("STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES"), Object.class);
        STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES = new Key(husk.CameraKeys.name("STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES"), Object.class);
        STATISTICS_INFO_AVAILABLE_OIS_DATA_MODES = new Key(husk.CameraKeys.name("STATISTICS_INFO_AVAILABLE_OIS_DATA_MODES"), Object.class);
        STATISTICS_INFO_MAX_FACE_COUNT = new Key(husk.CameraKeys.name("STATISTICS_INFO_MAX_FACE_COUNT"), Object.class);
        SYNC_MAX_LATENCY = new Key(husk.CameraKeys.name("SYNC_MAX_LATENCY"), Object.class);
        TONEMAP_AVAILABLE_TONE_MAP_MODES = new Key(husk.CameraKeys.name("TONEMAP_AVAILABLE_TONE_MAP_MODES"), Object.class);
        TONEMAP_MAX_CURVE_POINTS = new Key(husk.CameraKeys.name("TONEMAP_MAX_CURVE_POINTS"), Object.class);
    }
    public static android.hardware.camera2.CameraCharacteristics.Key AUTOMOTIVE_LENS_FACING;
    public static android.hardware.camera2.CameraCharacteristics.Key AUTOMOTIVE_LOCATION;
    public static android.hardware.camera2.CameraCharacteristics.Key COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key COLOR_CORRECTION_AVAILABLE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key COLOR_CORRECTION_COLOR_TEMPERATURE_RANGE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AE_AVAILABLE_ANTIBANDING_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AE_AVAILABLE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AE_AVAILABLE_PRIORITY_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AE_COMPENSATION_RANGE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AE_COMPENSATION_STEP;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AE_LOCK_AVAILABLE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AF_AVAILABLE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AUTOFRAMING_AVAILABLE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_EFFECTS;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_CAPABILITIES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_MAX_SIZES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_ZOOM_RATIO_RANGES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_SCENE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_SETTINGS_OVERRIDES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AWB_AVAILABLE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_AWB_LOCK_AVAILABLE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_LOW_LIGHT_BOOST_INFO_LUMINANCE_RANGE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_MAX_REGIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_MAX_REGIONS_AE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_MAX_REGIONS_AF;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_MAX_REGIONS_AWB;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE;
    public static android.hardware.camera2.CameraCharacteristics.Key CONTROL_ZOOM_RATIO_RANGE;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_AVAILABLE_RECOMMENDED_DEPTH_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key DEPTH_DEPTH_IS_EXCLUSIVE;
    public static android.hardware.camera2.CameraCharacteristics.Key DISTORTION_CORRECTION_AVAILABLE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key EDGE_AVAILABLE_EDGE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key FLASH_INFO_AVAILABLE;
    public static android.hardware.camera2.CameraCharacteristics.Key FLASH_INFO_STRENGTH_DEFAULT_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key FLASH_INFO_STRENGTH_MAXIMUM_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key FLASH_SINGLE_STRENGTH_DEFAULT_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key FLASH_SINGLE_STRENGTH_MAX_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key FLASH_TORCH_STRENGTH_DEFAULT_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key FLASH_TORCH_STRENGTH_MAX_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_STALL_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_STALL_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_ULTRA_HDR_MIN_FRAME_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_ULTRA_HDR_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_ULTRA_HDR_STALL_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_ULTRA_HDR_STALL_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_ULTRA_HDR_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key HEIC_AVAILABLE_HEIC_ULTRA_HDR_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key INFO_DEVICE_ID;
    public static android.hardware.camera2.CameraCharacteristics.Key INFO_DEVICE_STATE_ORIENTATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key INFO_DEVICE_STATE_SENSOR_ORIENTATION_MAP;
    public static android.hardware.camera2.CameraCharacteristics.Key INFO_SESSION_CONFIGURATION_QUERY_VERSION;
    public static android.hardware.camera2.CameraCharacteristics.Key INFO_SUPPORTED_HARDWARE_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key INFO_VERSION;
    public static android.hardware.camera2.CameraCharacteristics.Key JPEGR_AVAILABLE_JPEG_R_MIN_FRAME_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key JPEGR_AVAILABLE_JPEG_R_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key JPEGR_AVAILABLE_JPEG_R_STALL_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key JPEGR_AVAILABLE_JPEG_R_STALL_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key JPEGR_AVAILABLE_JPEG_R_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key JPEGR_AVAILABLE_JPEG_R_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key JPEG_AVAILABLE_THUMBNAIL_SIZES;
    public static android.hardware.camera2.CameraCharacteristics.Key LED_AVAILABLE_LEDS;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_DISTORTION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_DISTORTION_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_FACING;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_AVAILABLE_APERTURES;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_AVAILABLE_FILTER_DENSITIES;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_AVAILABLE_FOCAL_LENGTHS;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_FOCUS_DISTANCE_CALIBRATION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_HYPERFOCAL_DISTANCE;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_MINIMUM_FOCUS_DISTANCE;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INFO_SHADING_MAP_SIZE;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INTRINSIC_CALIBRATION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_INTRINSIC_CALIBRATION_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_POSE_REFERENCE;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_POSE_ROTATION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_POSE_TRANSLATION;
    public static android.hardware.camera2.CameraCharacteristics.Key LENS_RADIAL_DISTORTION;
    public static android.hardware.camera2.CameraCharacteristics.Key LOGICAL_MULTI_CAMERA_PHYSICAL_IDS;
    public static android.hardware.camera2.CameraCharacteristics.Key LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE;
    public static android.hardware.camera2.CameraCharacteristics.Key NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key QUIRKS_USE_PARTIAL_RESULT;
    public static android.hardware.camera2.CameraCharacteristics.Key REPROCESS_MAX_CAPTURE_STALL;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_CAPABILITIES;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_CHARACTERISTICS_KEYS;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_COLOR_SPACE_PROFILES;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_COLOR_SPACE_PROFILES_MAP;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_PHYSICAL_CAMERA_REQUEST_KEYS;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_REQUEST_KEYS;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_RESULT_KEYS;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_AVAILABLE_SESSION_KEYS;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_CHARACTERISTIC_KEYS_NEEDING_PERMISSION;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_MAX_NUM_INPUT_STREAMS;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_MAX_NUM_OUTPUT_PROC;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_MAX_NUM_OUTPUT_PROC_STALLING;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_MAX_NUM_OUTPUT_RAW;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_MAX_NUM_OUTPUT_STREAMS;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_PARTIAL_RESULT_COUNT;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_PIPELINE_MAX_DEPTH;
    public static android.hardware.camera2.CameraCharacteristics.Key REQUEST_RECOMMENDED_TEN_BIT_DYNAMIC_RANGE_PROFILE;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_FORMATS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_JPEG_MIN_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_JPEG_SIZES;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_MAX_DIGITAL_ZOOM;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_MIN_FRAME_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_MIN_FRAME_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_PROCESSED_SIZES;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_RECOMMENDED_INPUT_OUTPUT_FORMATS_MAP;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_RECOMMENDED_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_ROTATE_AND_CROP_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_STALL_DURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_STALL_DURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_STREAM_CONFIGURATIONS_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_AVAILABLE_STREAM_USE_CASES;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_CROPPING_TYPE;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_DEFAULT_SECURE_IMAGE_SIZE;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MANDATORY_CONCURRENT_STREAM_COMBINATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MANDATORY_MAXIMUM_RESOLUTION_STREAM_COMBINATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MANDATORY_PREVIEW_STABILIZATION_OUTPUT_STREAM_COMBINATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MANDATORY_STREAM_COMBINATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MANDATORY_TEN_BIT_OUTPUT_STREAM_COMBINATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MANDATORY_USE_CASE_STREAM_COMBINATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MULTI_RESOLUTION_STREAM_CONFIGURATION_MAP;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_MULTI_RESOLUTION_STREAM_SUPPORTED;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_PHYSICAL_CAMERA_MULTI_RESOLUTION_STREAM_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_STREAM_CONFIGURATION_MAP;
    public static android.hardware.camera2.CameraCharacteristics.Key SCALER_STREAM_CONFIGURATION_MAP_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_AVAILABLE_TEST_PATTERN_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_BLACK_LEVEL_PATTERN;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_CALIBRATION_TRANSFORM1;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_CALIBRATION_TRANSFORM2;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_COLOR_TRANSFORM1;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_COLOR_TRANSFORM2;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_FORWARD_MATRIX1;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_FORWARD_MATRIX2;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_ACTIVE_ARRAY_SIZE;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_BINNING_FACTOR;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_COLOR_FILTER_ARRANGEMENT;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_EXPOSURE_TIME_RANGE;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_LENS_SHADING_APPLIED;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_MAX_FRAME_DURATION;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_PHYSICAL_SIZE;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_PIXEL_ARRAY_SIZE;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_PIXEL_ARRAY_SIZE_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE_MAXIMUM_RESOLUTION;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_SENSITIVITY_RANGE;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_TIMESTAMP_SOURCE;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_INFO_WHITE_LEVEL;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_MAX_ANALOG_SENSITIVITY;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_OPTICAL_BLACK_REGIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_ORIENTATION;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_READOUT_TIMESTAMP;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_REFERENCE_ILLUMINANT1;
    public static android.hardware.camera2.CameraCharacteristics.Key SENSOR_REFERENCE_ILLUMINANT2;
    public static android.hardware.camera2.CameraCharacteristics.Key SHADING_AVAILABLE_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key SHARED_SESSION_COLOR_SPACE;
    public static android.hardware.camera2.CameraCharacteristics.Key SHARED_SESSION_CONFIGURATION;
    public static android.hardware.camera2.CameraCharacteristics.Key SHARED_SESSION_OUTPUT_CONFIGURATIONS;
    public static android.hardware.camera2.CameraCharacteristics.Key STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key STATISTICS_INFO_AVAILABLE_OIS_DATA_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key STATISTICS_INFO_MAX_FACE_COUNT;
    public static android.hardware.camera2.CameraCharacteristics.Key SYNC_MAX_LATENCY;
    public static android.hardware.camera2.CameraCharacteristics.Key TONEMAP_AVAILABLE_TONE_MAP_MODES;
    public static android.hardware.camera2.CameraCharacteristics.Key TONEMAP_MAX_CURVE_POINTS;
    public CameraCharacteristics(android.hardware.camera2.impl.CameraMetadataNative p0) { super(); }
    public CameraCharacteristics(java.util.Map<String,Object> values) { huskProps.putAll(values); }
    public java.lang.Object get(android.hardware.camera2.CameraCharacteristics.Key p0) { return huskProps.get(p0.getName()); }
    public java.util.List getAvailableCaptureRequestKeys() { return (huskProps.get("AvailableCaptureRequestKeys") != null ? (java.util.List) huskProps.get("AvailableCaptureRequestKeys") : new java.util.ArrayList()); }
    public java.util.List getAvailableCaptureResultKeys() { return (huskProps.get("AvailableCaptureResultKeys") != null ? (java.util.List) huskProps.get("AvailableCaptureResultKeys") : new java.util.ArrayList()); }
    public java.util.List getAvailablePhysicalCameraRequestKeys() { return (huskProps.get("AvailablePhysicalCameraRequestKeys") != null ? (java.util.List) huskProps.get("AvailablePhysicalCameraRequestKeys") : new java.util.ArrayList()); }
    public java.util.List getAvailableSessionCharacteristicsKeys() { return (huskProps.get("AvailableSessionCharacteristicsKeys") != null ? (java.util.List) huskProps.get("AvailableSessionCharacteristicsKeys") : new java.util.ArrayList()); }
    public java.util.List getAvailableSessionKeys() { return (huskProps.get("AvailableSessionKeys") != null ? (java.util.List) huskProps.get("AvailableSessionKeys") : new java.util.ArrayList()); }
    public java.lang.Class getKeyClass() { return (java.lang.Class) huskProps.get("KeyClass"); }
    public java.util.List getKeys() { return (huskProps.get("Keys") != null ? (java.util.List) huskProps.get("Keys") : new java.util.ArrayList()); }
    public java.util.List getKeysNeedingPermission() { return (huskProps.get("KeysNeedingPermission") != null ? (java.util.List) huskProps.get("KeysNeedingPermission") : new java.util.ArrayList()); }
    public android.hardware.camera2.impl.CameraMetadataNative getNativeCopy() { return (android.hardware.camera2.impl.CameraMetadataNative) huskProps.get("NativeCopy"); }
    public java.util.Set getPhysicalCameraIds() { return (huskProps.get("PhysicalCameraIds") != null ? (java.util.Set) huskProps.get("PhysicalCameraIds") : new java.util.HashSet()); }
    protected java.lang.Object getProtected(android.hardware.camera2.CameraCharacteristics.Key p0) { return null; }
    public java.lang.Object getProtected(java.lang.Object p0) { return null; }
    public android.hardware.camera2.params.RecommendedStreamConfigurationMap getRecommendedStreamConfigurationMap(int p0) { return null; }
    CameraCharacteristics() { this((android.hardware.camera2.impl.CameraMetadataNative) null); }
    public static final class Builder {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Builder() {}
        public Builder(android.hardware.camera2.CameraCharacteristics p0) {}
        public android.hardware.camera2.CameraCharacteristics build() { android.hardware.camera2.CameraCharacteristics x = new android.hardware.camera2.CameraCharacteristics(); x.huskProps.putAll(huskProps); return x; }
        public android.hardware.camera2.CameraCharacteristics.Builder set(android.hardware.camera2.CameraCharacteristics.Key p0, java.lang.Object p1) { return this; }
        public android.hardware.camera2.CameraCharacteristics.Builder setAvailableCaptureRequestKeys(java.util.List p0) { huskProps.put("AvailableCaptureRequestKeys", p0); return this; }
        public android.hardware.camera2.CameraCharacteristics.Builder setAvailableCaptureResultKeys(java.util.List p0) { huskProps.put("AvailableCaptureResultKeys", p0); return this; }
        public android.hardware.camera2.CameraCharacteristics.Builder setAvailableCharacteristicsKeys(java.util.List p0) { huskProps.put("AvailableCharacteristicsKeys", p0); return this; }
        public android.hardware.camera2.CameraCharacteristics.Builder setAvailableSessionKeys(java.util.List p0) { huskProps.put("AvailableSessionKeys", p0); return this; }
    }
    public static final class Key {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Key(java.lang.String p0, android.hardware.camera2.utils.TypeReference p1) { huskProps.put("Name", p0); }
        public Key(java.lang.String p0, java.lang.Class p1) { huskProps.put("Name", p0); }
        public Key(java.lang.String p0, java.lang.Class p1, long p2) { huskProps.put("Name", p0); }
        public Key(java.lang.String p0, java.lang.String p1, java.lang.Class p2) { huskProps.put("Name", p0); }
        @Override public boolean equals(Object other) { return other instanceof Key && java.util.Objects.equals(getName(), ((Key)other).getName()); }
        @Override public int hashCode() { return java.util.Objects.hashCode(getName()); }
        public java.lang.String getName() { return (java.lang.String) huskProps.get("Name"); }
        public android.hardware.camera2.impl.CameraMetadataNative.Key getNativeKey() { return (android.hardware.camera2.impl.CameraMetadataNative.Key) huskProps.get("NativeKey"); }
        public long getVendorId() { return (huskProps.get("VendorId") instanceof Long ? (Long) huskProps.get("VendorId") : 0L); }
        Key() { this((java.lang.String) null, (android.hardware.camera2.utils.TypeReference) null); }
    }
}
