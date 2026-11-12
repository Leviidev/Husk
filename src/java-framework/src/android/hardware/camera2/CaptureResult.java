// Camera compatibility implementation; remaining uncommon platform members retain their stubs.
package android.hardware.camera2;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class CaptureResult extends android.hardware.camera2.CameraMetadata {
    public final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    static {
        BLACK_LEVEL_LOCK = new Key(husk.CameraKeys.name("BLACK_LEVEL_LOCK"), Object.class);
        COLOR_CORRECTION_ABERRATION_MODE = new Key(husk.CameraKeys.name("COLOR_CORRECTION_ABERRATION_MODE"), Object.class);
        COLOR_CORRECTION_COLOR_TEMPERATURE = new Key(husk.CameraKeys.name("COLOR_CORRECTION_COLOR_TEMPERATURE"), Object.class);
        COLOR_CORRECTION_COLOR_TINT = new Key(husk.CameraKeys.name("COLOR_CORRECTION_COLOR_TINT"), Object.class);
        COLOR_CORRECTION_GAINS = new Key(husk.CameraKeys.name("COLOR_CORRECTION_GAINS"), Object.class);
        COLOR_CORRECTION_MODE = new Key(husk.CameraKeys.name("COLOR_CORRECTION_MODE"), Object.class);
        COLOR_CORRECTION_TRANSFORM = new Key(husk.CameraKeys.name("COLOR_CORRECTION_TRANSFORM"), Object.class);
        CONTROL_AE_ANTIBANDING_MODE = new Key(husk.CameraKeys.name("CONTROL_AE_ANTIBANDING_MODE"), Object.class);
        CONTROL_AE_EXPOSURE_COMPENSATION = new Key(husk.CameraKeys.name("CONTROL_AE_EXPOSURE_COMPENSATION"), Object.class);
        CONTROL_AE_LOCK = new Key(husk.CameraKeys.name("CONTROL_AE_LOCK"), Object.class);
        CONTROL_AE_MODE = new Key(husk.CameraKeys.name("CONTROL_AE_MODE"), Object.class);
        CONTROL_AE_PRECAPTURE_TRIGGER = new Key(husk.CameraKeys.name("CONTROL_AE_PRECAPTURE_TRIGGER"), Object.class);
        CONTROL_AE_PRIORITY_MODE = new Key(husk.CameraKeys.name("CONTROL_AE_PRIORITY_MODE"), Object.class);
        CONTROL_AE_REGIONS = new Key(husk.CameraKeys.name("CONTROL_AE_REGIONS"), Object.class);
        CONTROL_AE_STATE = new Key(husk.CameraKeys.name("CONTROL_AE_STATE"), Object.class);
        CONTROL_AE_TARGET_FPS_RANGE = new Key(husk.CameraKeys.name("CONTROL_AE_TARGET_FPS_RANGE"), Object.class);
        CONTROL_AF_MODE = new Key(husk.CameraKeys.name("CONTROL_AF_MODE"), Object.class);
        CONTROL_AF_REGIONS = new Key(husk.CameraKeys.name("CONTROL_AF_REGIONS"), Object.class);
        CONTROL_AF_SCENE_CHANGE = new Key(husk.CameraKeys.name("CONTROL_AF_SCENE_CHANGE"), Object.class);
        CONTROL_AF_STATE = new Key(husk.CameraKeys.name("CONTROL_AF_STATE"), Object.class);
        CONTROL_AF_TRIGGER = new Key(husk.CameraKeys.name("CONTROL_AF_TRIGGER"), Object.class);
        CONTROL_AUTOFRAMING = new Key(husk.CameraKeys.name("CONTROL_AUTOFRAMING"), Object.class);
        CONTROL_AUTOFRAMING_STATE = new Key(husk.CameraKeys.name("CONTROL_AUTOFRAMING_STATE"), Object.class);
        CONTROL_AWB_LOCK = new Key(husk.CameraKeys.name("CONTROL_AWB_LOCK"), Object.class);
        CONTROL_AWB_MODE = new Key(husk.CameraKeys.name("CONTROL_AWB_MODE"), Object.class);
        CONTROL_AWB_REGIONS = new Key(husk.CameraKeys.name("CONTROL_AWB_REGIONS"), Object.class);
        CONTROL_AWB_STATE = new Key(husk.CameraKeys.name("CONTROL_AWB_STATE"), Object.class);
        CONTROL_CAPTURE_INTENT = new Key(husk.CameraKeys.name("CONTROL_CAPTURE_INTENT"), Object.class);
        CONTROL_EFFECT_MODE = new Key(husk.CameraKeys.name("CONTROL_EFFECT_MODE"), Object.class);
        CONTROL_ENABLE_ZSL = new Key(husk.CameraKeys.name("CONTROL_ENABLE_ZSL"), Object.class);
        CONTROL_EXTENDED_SCENE_MODE = new Key(husk.CameraKeys.name("CONTROL_EXTENDED_SCENE_MODE"), Object.class);
        CONTROL_LOW_LIGHT_BOOST_STATE = new Key(husk.CameraKeys.name("CONTROL_LOW_LIGHT_BOOST_STATE"), Object.class);
        CONTROL_MODE = new Key(husk.CameraKeys.name("CONTROL_MODE"), Object.class);
        CONTROL_POST_RAW_SENSITIVITY_BOOST = new Key(husk.CameraKeys.name("CONTROL_POST_RAW_SENSITIVITY_BOOST"), Object.class);
        CONTROL_SCENE_MODE = new Key(husk.CameraKeys.name("CONTROL_SCENE_MODE"), Object.class);
        CONTROL_SETTINGS_OVERRIDE = new Key(husk.CameraKeys.name("CONTROL_SETTINGS_OVERRIDE"), Object.class);
        CONTROL_VIDEO_STABILIZATION_MODE = new Key(husk.CameraKeys.name("CONTROL_VIDEO_STABILIZATION_MODE"), Object.class);
        CONTROL_ZOOM_METHOD = new Key(husk.CameraKeys.name("CONTROL_ZOOM_METHOD"), Object.class);
        CONTROL_ZOOM_RATIO = new Key(husk.CameraKeys.name("CONTROL_ZOOM_RATIO"), Object.class);
        DISTORTION_CORRECTION_MODE = new Key(husk.CameraKeys.name("DISTORTION_CORRECTION_MODE"), Object.class);
        EDGE_MODE = new Key(husk.CameraKeys.name("EDGE_MODE"), Object.class);
        EXTENSION_CURRENT_TYPE = new Key(husk.CameraKeys.name("EXTENSION_CURRENT_TYPE"), Object.class);
        EXTENSION_NIGHT_MODE_INDICATOR = new Key(husk.CameraKeys.name("EXTENSION_NIGHT_MODE_INDICATOR"), Object.class);
        EXTENSION_STRENGTH = new Key(husk.CameraKeys.name("EXTENSION_STRENGTH"), Object.class);
        FLASH_MODE = new Key(husk.CameraKeys.name("FLASH_MODE"), Object.class);
        FLASH_STATE = new Key(husk.CameraKeys.name("FLASH_STATE"), Object.class);
        FLASH_STRENGTH_LEVEL = new Key(husk.CameraKeys.name("FLASH_STRENGTH_LEVEL"), Object.class);
        HOT_PIXEL_MODE = new Key(husk.CameraKeys.name("HOT_PIXEL_MODE"), Object.class);
        JPEG_GPS_COORDINATES = new Key(husk.CameraKeys.name("JPEG_GPS_COORDINATES"), Object.class);
        JPEG_GPS_LOCATION = new Key(husk.CameraKeys.name("JPEG_GPS_LOCATION"), Object.class);
        JPEG_GPS_PROCESSING_METHOD = new Key(husk.CameraKeys.name("JPEG_GPS_PROCESSING_METHOD"), Object.class);
        JPEG_GPS_TIMESTAMP = new Key(husk.CameraKeys.name("JPEG_GPS_TIMESTAMP"), Object.class);
        JPEG_ORIENTATION = new Key(husk.CameraKeys.name("JPEG_ORIENTATION"), Object.class);
        JPEG_QUALITY = new Key(husk.CameraKeys.name("JPEG_QUALITY"), Object.class);
        JPEG_THUMBNAIL_QUALITY = new Key(husk.CameraKeys.name("JPEG_THUMBNAIL_QUALITY"), Object.class);
        JPEG_THUMBNAIL_SIZE = new Key(husk.CameraKeys.name("JPEG_THUMBNAIL_SIZE"), Object.class);
        LED_TRANSMIT = new Key(husk.CameraKeys.name("LED_TRANSMIT"), Object.class);
        LENS_APERTURE = new Key(husk.CameraKeys.name("LENS_APERTURE"), Object.class);
        LENS_DISTORTION = new Key(husk.CameraKeys.name("LENS_DISTORTION"), Object.class);
        LENS_FILTER_DENSITY = new Key(husk.CameraKeys.name("LENS_FILTER_DENSITY"), Object.class);
        LENS_FOCAL_LENGTH = new Key(husk.CameraKeys.name("LENS_FOCAL_LENGTH"), Object.class);
        LENS_FOCUS_DISTANCE = new Key(husk.CameraKeys.name("LENS_FOCUS_DISTANCE"), Object.class);
        LENS_FOCUS_RANGE = new Key(husk.CameraKeys.name("LENS_FOCUS_RANGE"), Object.class);
        LENS_INTRINSIC_CALIBRATION = new Key(husk.CameraKeys.name("LENS_INTRINSIC_CALIBRATION"), Object.class);
        LENS_OPTICAL_STABILIZATION_MODE = new Key(husk.CameraKeys.name("LENS_OPTICAL_STABILIZATION_MODE"), Object.class);
        LENS_POSE_ROTATION = new Key(husk.CameraKeys.name("LENS_POSE_ROTATION"), Object.class);
        LENS_POSE_TRANSLATION = new Key(husk.CameraKeys.name("LENS_POSE_TRANSLATION"), Object.class);
        LENS_RADIAL_DISTORTION = new Key(husk.CameraKeys.name("LENS_RADIAL_DISTORTION"), Object.class);
        LENS_STATE = new Key(husk.CameraKeys.name("LENS_STATE"), Object.class);
        LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID = new Key(husk.CameraKeys.name("LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID"), Object.class);
        LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_SENSOR_CROP_REGION = new Key(husk.CameraKeys.name("LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_SENSOR_CROP_REGION"), Object.class);
        NOISE_REDUCTION_MODE = new Key(husk.CameraKeys.name("NOISE_REDUCTION_MODE"), Object.class);
        QUIRKS_PARTIAL_RESULT = new Key(husk.CameraKeys.name("QUIRKS_PARTIAL_RESULT"), Object.class);
        REPROCESS_EFFECTIVE_EXPOSURE_FACTOR = new Key(husk.CameraKeys.name("REPROCESS_EFFECTIVE_EXPOSURE_FACTOR"), Object.class);
        REQUEST_FRAME_COUNT = new Key(husk.CameraKeys.name("REQUEST_FRAME_COUNT"), Object.class);
        REQUEST_ID = new Key(husk.CameraKeys.name("REQUEST_ID"), Object.class);
        REQUEST_PIPELINE_DEPTH = new Key(husk.CameraKeys.name("REQUEST_PIPELINE_DEPTH"), Object.class);
        SCALER_CROP_REGION = new Key(husk.CameraKeys.name("SCALER_CROP_REGION"), Object.class);
        SCALER_RAW_CROP_REGION = new Key(husk.CameraKeys.name("SCALER_RAW_CROP_REGION"), Object.class);
        SCALER_ROTATE_AND_CROP = new Key(husk.CameraKeys.name("SCALER_ROTATE_AND_CROP"), Object.class);
        SENSOR_DYNAMIC_BLACK_LEVEL = new Key(husk.CameraKeys.name("SENSOR_DYNAMIC_BLACK_LEVEL"), Object.class);
        SENSOR_DYNAMIC_WHITE_LEVEL = new Key(husk.CameraKeys.name("SENSOR_DYNAMIC_WHITE_LEVEL"), Object.class);
        SENSOR_EXPOSURE_TIME = new Key(husk.CameraKeys.name("SENSOR_EXPOSURE_TIME"), Object.class);
        SENSOR_FRAME_DURATION = new Key(husk.CameraKeys.name("SENSOR_FRAME_DURATION"), Object.class);
        SENSOR_GREEN_SPLIT = new Key(husk.CameraKeys.name("SENSOR_GREEN_SPLIT"), Object.class);
        SENSOR_NEUTRAL_COLOR_POINT = new Key(husk.CameraKeys.name("SENSOR_NEUTRAL_COLOR_POINT"), Object.class);
        SENSOR_NOISE_PROFILE = new Key(husk.CameraKeys.name("SENSOR_NOISE_PROFILE"), Object.class);
        SENSOR_PIXEL_MODE = new Key(husk.CameraKeys.name("SENSOR_PIXEL_MODE"), Object.class);
        SENSOR_RAW_BINNING_FACTOR_USED = new Key(husk.CameraKeys.name("SENSOR_RAW_BINNING_FACTOR_USED"), Object.class);
        SENSOR_ROLLING_SHUTTER_SKEW = new Key(husk.CameraKeys.name("SENSOR_ROLLING_SHUTTER_SKEW"), Object.class);
        SENSOR_SENSITIVITY = new Key(husk.CameraKeys.name("SENSOR_SENSITIVITY"), Object.class);
        SENSOR_TEST_PATTERN_DATA = new Key(husk.CameraKeys.name("SENSOR_TEST_PATTERN_DATA"), Object.class);
        SENSOR_TEST_PATTERN_MODE = new Key(husk.CameraKeys.name("SENSOR_TEST_PATTERN_MODE"), Object.class);
        SENSOR_TIMESTAMP = new Key(husk.CameraKeys.name("SENSOR_TIMESTAMP"), Object.class);
        SHADING_MODE = new Key(husk.CameraKeys.name("SHADING_MODE"), Object.class);
        STATISTICS_FACES = new Key(husk.CameraKeys.name("STATISTICS_FACES"), Object.class);
        STATISTICS_FACE_DETECT_MODE = new Key(husk.CameraKeys.name("STATISTICS_FACE_DETECT_MODE"), Object.class);
        STATISTICS_FACE_IDS = new Key(husk.CameraKeys.name("STATISTICS_FACE_IDS"), Object.class);
        STATISTICS_FACE_LANDMARKS = new Key(husk.CameraKeys.name("STATISTICS_FACE_LANDMARKS"), Object.class);
        STATISTICS_FACE_RECTANGLES = new Key(husk.CameraKeys.name("STATISTICS_FACE_RECTANGLES"), Object.class);
        STATISTICS_FACE_SCORES = new Key(husk.CameraKeys.name("STATISTICS_FACE_SCORES"), Object.class);
        STATISTICS_HOT_PIXEL_MAP = new Key(husk.CameraKeys.name("STATISTICS_HOT_PIXEL_MAP"), Object.class);
        STATISTICS_HOT_PIXEL_MAP_MODE = new Key(husk.CameraKeys.name("STATISTICS_HOT_PIXEL_MAP_MODE"), Object.class);
        STATISTICS_LENS_INTRINSICS_SAMPLES = new Key(husk.CameraKeys.name("STATISTICS_LENS_INTRINSICS_SAMPLES"), Object.class);
        STATISTICS_LENS_INTRINSIC_SAMPLES = new Key(husk.CameraKeys.name("STATISTICS_LENS_INTRINSIC_SAMPLES"), Object.class);
        STATISTICS_LENS_INTRINSIC_TIMESTAMPS = new Key(husk.CameraKeys.name("STATISTICS_LENS_INTRINSIC_TIMESTAMPS"), Object.class);
        STATISTICS_LENS_SHADING_CORRECTION_MAP = new Key(husk.CameraKeys.name("STATISTICS_LENS_SHADING_CORRECTION_MAP"), Object.class);
        STATISTICS_LENS_SHADING_MAP = new Key(husk.CameraKeys.name("STATISTICS_LENS_SHADING_MAP"), Object.class);
        STATISTICS_LENS_SHADING_MAP_MODE = new Key(husk.CameraKeys.name("STATISTICS_LENS_SHADING_MAP_MODE"), Object.class);
        STATISTICS_OIS_DATA_MODE = new Key(husk.CameraKeys.name("STATISTICS_OIS_DATA_MODE"), Object.class);
        STATISTICS_OIS_SAMPLES = new Key(husk.CameraKeys.name("STATISTICS_OIS_SAMPLES"), Object.class);
        STATISTICS_OIS_TIMESTAMPS = new Key(husk.CameraKeys.name("STATISTICS_OIS_TIMESTAMPS"), Object.class);
        STATISTICS_OIS_X_SHIFTS = new Key(husk.CameraKeys.name("STATISTICS_OIS_X_SHIFTS"), Object.class);
        STATISTICS_OIS_Y_SHIFTS = new Key(husk.CameraKeys.name("STATISTICS_OIS_Y_SHIFTS"), Object.class);
        STATISTICS_PREDICTED_COLOR_GAINS = new Key(husk.CameraKeys.name("STATISTICS_PREDICTED_COLOR_GAINS"), Object.class);
        STATISTICS_PREDICTED_COLOR_TRANSFORM = new Key(husk.CameraKeys.name("STATISTICS_PREDICTED_COLOR_TRANSFORM"), Object.class);
        STATISTICS_SCENE_FLICKER = new Key(husk.CameraKeys.name("STATISTICS_SCENE_FLICKER"), Object.class);
        SYNC_FRAME_NUMBER = new Key(husk.CameraKeys.name("SYNC_FRAME_NUMBER"), Object.class);
        TONEMAP_CURVE = new Key(husk.CameraKeys.name("TONEMAP_CURVE"), Object.class);
        TONEMAP_CURVE_BLUE = new Key(husk.CameraKeys.name("TONEMAP_CURVE_BLUE"), Object.class);
        TONEMAP_CURVE_GREEN = new Key(husk.CameraKeys.name("TONEMAP_CURVE_GREEN"), Object.class);
        TONEMAP_CURVE_RED = new Key(husk.CameraKeys.name("TONEMAP_CURVE_RED"), Object.class);
        TONEMAP_GAMMA = new Key(husk.CameraKeys.name("TONEMAP_GAMMA"), Object.class);
        TONEMAP_MODE = new Key(husk.CameraKeys.name("TONEMAP_MODE"), Object.class);
        TONEMAP_PRESET_CURVE = new Key(husk.CameraKeys.name("TONEMAP_PRESET_CURVE"), Object.class);
    }
    public static android.hardware.camera2.CaptureResult.Key BLACK_LEVEL_LOCK;
    public static android.hardware.camera2.CaptureResult.Key COLOR_CORRECTION_ABERRATION_MODE;
    public static android.hardware.camera2.CaptureResult.Key COLOR_CORRECTION_COLOR_TEMPERATURE;
    public static android.hardware.camera2.CaptureResult.Key COLOR_CORRECTION_COLOR_TINT;
    public static android.hardware.camera2.CaptureResult.Key COLOR_CORRECTION_GAINS;
    public static android.hardware.camera2.CaptureResult.Key COLOR_CORRECTION_MODE;
    public static android.hardware.camera2.CaptureResult.Key COLOR_CORRECTION_TRANSFORM;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_ANTIBANDING_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_EXPOSURE_COMPENSATION;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_LOCK;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_PRECAPTURE_TRIGGER;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_PRIORITY_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_REGIONS;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_STATE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AE_TARGET_FPS_RANGE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AF_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AF_REGIONS;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AF_SCENE_CHANGE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AF_STATE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AF_TRIGGER;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AUTOFRAMING;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AUTOFRAMING_STATE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AWB_LOCK;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AWB_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AWB_REGIONS;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_AWB_STATE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_CAPTURE_INTENT;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_EFFECT_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_ENABLE_ZSL;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_EXTENDED_SCENE_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_LOW_LIGHT_BOOST_STATE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_POST_RAW_SENSITIVITY_BOOST;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_SCENE_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_SETTINGS_OVERRIDE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_VIDEO_STABILIZATION_MODE;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_ZOOM_METHOD;
    public static android.hardware.camera2.CaptureResult.Key CONTROL_ZOOM_RATIO;
    public static android.hardware.camera2.CaptureResult.Key DISTORTION_CORRECTION_MODE;
    public static android.hardware.camera2.CaptureResult.Key EDGE_MODE;
    public static android.hardware.camera2.CaptureResult.Key EXTENSION_CURRENT_TYPE;
    public static android.hardware.camera2.CaptureResult.Key EXTENSION_NIGHT_MODE_INDICATOR;
    public static android.hardware.camera2.CaptureResult.Key EXTENSION_STRENGTH;
    public static android.hardware.camera2.CaptureResult.Key FLASH_MODE;
    public static android.hardware.camera2.CaptureResult.Key FLASH_STATE;
    public static android.hardware.camera2.CaptureResult.Key FLASH_STRENGTH_LEVEL;
    public static android.hardware.camera2.CaptureResult.Key HOT_PIXEL_MODE;
    public static android.hardware.camera2.CaptureResult.Key JPEG_GPS_COORDINATES;
    public static android.hardware.camera2.CaptureResult.Key JPEG_GPS_LOCATION;
    public static android.hardware.camera2.CaptureResult.Key JPEG_GPS_PROCESSING_METHOD;
    public static android.hardware.camera2.CaptureResult.Key JPEG_GPS_TIMESTAMP;
    public static android.hardware.camera2.CaptureResult.Key JPEG_ORIENTATION;
    public static android.hardware.camera2.CaptureResult.Key JPEG_QUALITY;
    public static android.hardware.camera2.CaptureResult.Key JPEG_THUMBNAIL_QUALITY;
    public static android.hardware.camera2.CaptureResult.Key JPEG_THUMBNAIL_SIZE;
    public static android.hardware.camera2.CaptureResult.Key LED_TRANSMIT;
    public static android.hardware.camera2.CaptureResult.Key LENS_APERTURE;
    public static android.hardware.camera2.CaptureResult.Key LENS_DISTORTION;
    public static android.hardware.camera2.CaptureResult.Key LENS_FILTER_DENSITY;
    public static android.hardware.camera2.CaptureResult.Key LENS_FOCAL_LENGTH;
    public static android.hardware.camera2.CaptureResult.Key LENS_FOCUS_DISTANCE;
    public static android.hardware.camera2.CaptureResult.Key LENS_FOCUS_RANGE;
    public static android.hardware.camera2.CaptureResult.Key LENS_INTRINSIC_CALIBRATION;
    public static android.hardware.camera2.CaptureResult.Key LENS_OPTICAL_STABILIZATION_MODE;
    public static android.hardware.camera2.CaptureResult.Key LENS_POSE_ROTATION;
    public static android.hardware.camera2.CaptureResult.Key LENS_POSE_TRANSLATION;
    public static android.hardware.camera2.CaptureResult.Key LENS_RADIAL_DISTORTION;
    public static android.hardware.camera2.CaptureResult.Key LENS_STATE;
    public static android.hardware.camera2.CaptureResult.Key LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID;
    public static android.hardware.camera2.CaptureResult.Key LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_SENSOR_CROP_REGION;
    public static android.hardware.camera2.CaptureResult.Key NOISE_REDUCTION_MODE;
    public static android.hardware.camera2.CaptureResult.Key QUIRKS_PARTIAL_RESULT;
    public static android.hardware.camera2.CaptureResult.Key REPROCESS_EFFECTIVE_EXPOSURE_FACTOR;
    public static android.hardware.camera2.CaptureResult.Key REQUEST_FRAME_COUNT;
    public static android.hardware.camera2.CaptureResult.Key REQUEST_ID;
    public static android.hardware.camera2.CaptureResult.Key REQUEST_PIPELINE_DEPTH;
    public static android.hardware.camera2.CaptureResult.Key SCALER_CROP_REGION;
    public static android.hardware.camera2.CaptureResult.Key SCALER_RAW_CROP_REGION;
    public static android.hardware.camera2.CaptureResult.Key SCALER_ROTATE_AND_CROP;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_DYNAMIC_BLACK_LEVEL;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_DYNAMIC_WHITE_LEVEL;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_EXPOSURE_TIME;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_FRAME_DURATION;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_GREEN_SPLIT;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_NEUTRAL_COLOR_POINT;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_NOISE_PROFILE;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_PIXEL_MODE;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_RAW_BINNING_FACTOR_USED;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_ROLLING_SHUTTER_SKEW;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_SENSITIVITY;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_TEST_PATTERN_DATA;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_TEST_PATTERN_MODE;
    public static android.hardware.camera2.CaptureResult.Key SENSOR_TIMESTAMP;
    public static android.hardware.camera2.CaptureResult.Key SHADING_MODE;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_FACES;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_FACE_DETECT_MODE;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_FACE_IDS;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_FACE_LANDMARKS;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_FACE_RECTANGLES;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_FACE_SCORES;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_HOT_PIXEL_MAP;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_HOT_PIXEL_MAP_MODE;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_LENS_INTRINSICS_SAMPLES;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_LENS_INTRINSIC_SAMPLES;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_LENS_INTRINSIC_TIMESTAMPS;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_LENS_SHADING_CORRECTION_MAP;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_LENS_SHADING_MAP;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_LENS_SHADING_MAP_MODE;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_OIS_DATA_MODE;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_OIS_SAMPLES;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_OIS_TIMESTAMPS;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_OIS_X_SHIFTS;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_OIS_Y_SHIFTS;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_PREDICTED_COLOR_GAINS;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_PREDICTED_COLOR_TRANSFORM;
    public static android.hardware.camera2.CaptureResult.Key STATISTICS_SCENE_FLICKER;
    public static android.hardware.camera2.CaptureResult.Key SYNC_FRAME_NUMBER;
    public static android.hardware.camera2.CaptureResult.Key TONEMAP_CURVE;
    public static android.hardware.camera2.CaptureResult.Key TONEMAP_CURVE_BLUE;
    public static android.hardware.camera2.CaptureResult.Key TONEMAP_CURVE_GREEN;
    public static android.hardware.camera2.CaptureResult.Key TONEMAP_CURVE_RED;
    public static android.hardware.camera2.CaptureResult.Key TONEMAP_GAMMA;
    public static android.hardware.camera2.CaptureResult.Key TONEMAP_MODE;
    public static android.hardware.camera2.CaptureResult.Key TONEMAP_PRESET_CURVE;
    public CaptureResult(android.hardware.camera2.impl.CameraMetadataNative p0, int p1) { super(); }
    public CaptureResult(java.lang.String p0, android.hardware.camera2.impl.CameraMetadataNative p1, android.hardware.camera2.CaptureRequest p2, int p3, long p4) { super(); }
    public CaptureResult(java.lang.String p0, android.hardware.camera2.impl.CameraMetadataNative p1, android.hardware.camera2.CaptureRequest p2, android.hardware.camera2.impl.CaptureResultExtras p3) { super(); }
    public void dumpToLog() {}
    public java.lang.Object get(android.hardware.camera2.CaptureResult.Key p0) { return huskProps.get(p0.getName()); }
    public java.lang.String getCameraId() { return (java.lang.String) huskProps.get("CameraId"); }
    public long getFrameNumber() { return (huskProps.get("FrameNumber") instanceof Long ? (Long) huskProps.get("FrameNumber") : 0L); }
    public java.lang.Class getKeyClass() { return (java.lang.Class) huskProps.get("KeyClass"); }
    public java.util.List getKeys() { return (huskProps.get("Keys") != null ? (java.util.List) huskProps.get("Keys") : new java.util.ArrayList()); }
    public android.hardware.camera2.impl.CameraMetadataNative getNativeCopy() { return (android.hardware.camera2.impl.CameraMetadataNative) huskProps.get("NativeCopy"); }
    protected java.lang.Object getProtected(android.hardware.camera2.CaptureResult.Key p0) { return null; }
    public java.lang.Object getProtected(java.lang.Object p0) { return null; }
    public android.hardware.camera2.CaptureRequest getRequest() { return (android.hardware.camera2.CaptureRequest) huskProps.get("Request"); }
    public int getSequenceId() { return (huskProps.get("SequenceId") instanceof Integer ? (Integer) huskProps.get("SequenceId") : 0); }
    CaptureResult() { this((android.hardware.camera2.impl.CameraMetadataNative) null, (int) 0); }
    public static final class Builder {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Builder() {}
        public Builder(android.hardware.camera2.CaptureResult p0) {}
        public android.hardware.camera2.CaptureResult build() { android.hardware.camera2.CaptureResult x = new android.hardware.camera2.CaptureResult(); x.huskProps.putAll(huskProps); return x; }
        public android.hardware.camera2.CaptureResult.Builder set(android.hardware.camera2.CaptureResult.Key p0, java.lang.Object p1) { return this; }
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
