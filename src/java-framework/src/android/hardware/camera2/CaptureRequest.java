// Camera compatibility implementation; remaining uncommon platform members retain their stubs.
package android.hardware.camera2;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public final class CaptureRequest extends android.hardware.camera2.CameraMetadata implements android.os.Parcelable {
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
        CONTROL_AE_REGIONS_SET = new Key(husk.CameraKeys.name("CONTROL_AE_REGIONS_SET"), Object.class);
        CONTROL_AE_TARGET_FPS_RANGE = new Key(husk.CameraKeys.name("CONTROL_AE_TARGET_FPS_RANGE"), Object.class);
        CONTROL_AF_MODE = new Key(husk.CameraKeys.name("CONTROL_AF_MODE"), Object.class);
        CONTROL_AF_REGIONS = new Key(husk.CameraKeys.name("CONTROL_AF_REGIONS"), Object.class);
        CONTROL_AF_REGIONS_SET = new Key(husk.CameraKeys.name("CONTROL_AF_REGIONS_SET"), Object.class);
        CONTROL_AF_TRIGGER = new Key(husk.CameraKeys.name("CONTROL_AF_TRIGGER"), Object.class);
        CONTROL_AUTOFRAMING = new Key(husk.CameraKeys.name("CONTROL_AUTOFRAMING"), Object.class);
        CONTROL_AWB_LOCK = new Key(husk.CameraKeys.name("CONTROL_AWB_LOCK"), Object.class);
        CONTROL_AWB_MODE = new Key(husk.CameraKeys.name("CONTROL_AWB_MODE"), Object.class);
        CONTROL_AWB_REGIONS = new Key(husk.CameraKeys.name("CONTROL_AWB_REGIONS"), Object.class);
        CONTROL_AWB_REGIONS_SET = new Key(husk.CameraKeys.name("CONTROL_AWB_REGIONS_SET"), Object.class);
        CONTROL_CAPTURE_INTENT = new Key(husk.CameraKeys.name("CONTROL_CAPTURE_INTENT"), Object.class);
        CONTROL_EFFECT_MODE = new Key(husk.CameraKeys.name("CONTROL_EFFECT_MODE"), Object.class);
        CONTROL_ENABLE_ZSL = new Key(husk.CameraKeys.name("CONTROL_ENABLE_ZSL"), Object.class);
        CONTROL_EXTENDED_SCENE_MODE = new Key(husk.CameraKeys.name("CONTROL_EXTENDED_SCENE_MODE"), Object.class);
        CONTROL_MODE = new Key(husk.CameraKeys.name("CONTROL_MODE"), Object.class);
        CONTROL_POST_RAW_SENSITIVITY_BOOST = new Key(husk.CameraKeys.name("CONTROL_POST_RAW_SENSITIVITY_BOOST"), Object.class);
        CONTROL_SCENE_MODE = new Key(husk.CameraKeys.name("CONTROL_SCENE_MODE"), Object.class);
        CONTROL_SETTINGS_OVERRIDE = new Key(husk.CameraKeys.name("CONTROL_SETTINGS_OVERRIDE"), Object.class);
        CONTROL_VIDEO_STABILIZATION_MODE = new Key(husk.CameraKeys.name("CONTROL_VIDEO_STABILIZATION_MODE"), Object.class);
        CONTROL_ZOOM_METHOD = new Key(husk.CameraKeys.name("CONTROL_ZOOM_METHOD"), Object.class);
        CONTROL_ZOOM_RATIO = new Key(husk.CameraKeys.name("CONTROL_ZOOM_RATIO"), Object.class);
        DISTORTION_CORRECTION_MODE = new Key(husk.CameraKeys.name("DISTORTION_CORRECTION_MODE"), Object.class);
        EDGE_MODE = new Key(husk.CameraKeys.name("EDGE_MODE"), Object.class);
        EXTENSION_STRENGTH = new Key(husk.CameraKeys.name("EXTENSION_STRENGTH"), Object.class);
        FLASH_MODE = new Key(husk.CameraKeys.name("FLASH_MODE"), Object.class);
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
        LENS_FILTER_DENSITY = new Key(husk.CameraKeys.name("LENS_FILTER_DENSITY"), Object.class);
        LENS_FOCAL_LENGTH = new Key(husk.CameraKeys.name("LENS_FOCAL_LENGTH"), Object.class);
        LENS_FOCUS_DISTANCE = new Key(husk.CameraKeys.name("LENS_FOCUS_DISTANCE"), Object.class);
        LENS_OPTICAL_STABILIZATION_MODE = new Key(husk.CameraKeys.name("LENS_OPTICAL_STABILIZATION_MODE"), Object.class);
        NOISE_REDUCTION_MODE = new Key(husk.CameraKeys.name("NOISE_REDUCTION_MODE"), Object.class);
        REPROCESS_EFFECTIVE_EXPOSURE_FACTOR = new Key(husk.CameraKeys.name("REPROCESS_EFFECTIVE_EXPOSURE_FACTOR"), Object.class);
        REQUEST_ID = new Key(husk.CameraKeys.name("REQUEST_ID"), Object.class);
        SCALER_CROP_REGION = new Key(husk.CameraKeys.name("SCALER_CROP_REGION"), Object.class);
        SCALER_CROP_REGION_SET = new Key(husk.CameraKeys.name("SCALER_CROP_REGION_SET"), Object.class);
        SCALER_ROTATE_AND_CROP = new Key(husk.CameraKeys.name("SCALER_ROTATE_AND_CROP"), Object.class);
        SENSOR_EXPOSURE_TIME = new Key(husk.CameraKeys.name("SENSOR_EXPOSURE_TIME"), Object.class);
        SENSOR_FRAME_DURATION = new Key(husk.CameraKeys.name("SENSOR_FRAME_DURATION"), Object.class);
        SENSOR_PIXEL_MODE = new Key(husk.CameraKeys.name("SENSOR_PIXEL_MODE"), Object.class);
        SENSOR_SENSITIVITY = new Key(husk.CameraKeys.name("SENSOR_SENSITIVITY"), Object.class);
        SENSOR_TEST_PATTERN_DATA = new Key(husk.CameraKeys.name("SENSOR_TEST_PATTERN_DATA"), Object.class);
        SENSOR_TEST_PATTERN_MODE = new Key(husk.CameraKeys.name("SENSOR_TEST_PATTERN_MODE"), Object.class);
        SHADING_MODE = new Key(husk.CameraKeys.name("SHADING_MODE"), Object.class);
        STATISTICS_FACE_DETECT_MODE = new Key(husk.CameraKeys.name("STATISTICS_FACE_DETECT_MODE"), Object.class);
        STATISTICS_HOT_PIXEL_MAP_MODE = new Key(husk.CameraKeys.name("STATISTICS_HOT_PIXEL_MAP_MODE"), Object.class);
        STATISTICS_LENS_SHADING_MAP_MODE = new Key(husk.CameraKeys.name("STATISTICS_LENS_SHADING_MAP_MODE"), Object.class);
        STATISTICS_OIS_DATA_MODE = new Key(husk.CameraKeys.name("STATISTICS_OIS_DATA_MODE"), Object.class);
        TONEMAP_CURVE = new Key(husk.CameraKeys.name("TONEMAP_CURVE"), Object.class);
        TONEMAP_CURVE_BLUE = new Key(husk.CameraKeys.name("TONEMAP_CURVE_BLUE"), Object.class);
        TONEMAP_CURVE_GREEN = new Key(husk.CameraKeys.name("TONEMAP_CURVE_GREEN"), Object.class);
        TONEMAP_CURVE_RED = new Key(husk.CameraKeys.name("TONEMAP_CURVE_RED"), Object.class);
        TONEMAP_GAMMA = new Key(husk.CameraKeys.name("TONEMAP_GAMMA"), Object.class);
        TONEMAP_MODE = new Key(husk.CameraKeys.name("TONEMAP_MODE"), Object.class);
        TONEMAP_PRESET_CURVE = new Key(husk.CameraKeys.name("TONEMAP_PRESET_CURVE"), Object.class);
    }
    public static android.hardware.camera2.CaptureRequest.Key BLACK_LEVEL_LOCK;
    public static android.hardware.camera2.CaptureRequest.Key COLOR_CORRECTION_ABERRATION_MODE;
    public static android.hardware.camera2.CaptureRequest.Key COLOR_CORRECTION_COLOR_TEMPERATURE;
    public static android.hardware.camera2.CaptureRequest.Key COLOR_CORRECTION_COLOR_TINT;
    public static android.hardware.camera2.CaptureRequest.Key COLOR_CORRECTION_GAINS;
    public static android.hardware.camera2.CaptureRequest.Key COLOR_CORRECTION_MODE;
    public static android.hardware.camera2.CaptureRequest.Key COLOR_CORRECTION_TRANSFORM;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_ANTIBANDING_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_EXPOSURE_COMPENSATION;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_LOCK;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_PRECAPTURE_TRIGGER;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_PRIORITY_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_REGIONS;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_REGIONS_SET;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AE_TARGET_FPS_RANGE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AF_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AF_REGIONS;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AF_REGIONS_SET;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AF_TRIGGER;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AUTOFRAMING;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AWB_LOCK;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AWB_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AWB_REGIONS;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_AWB_REGIONS_SET;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_CAPTURE_INTENT;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_EFFECT_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_ENABLE_ZSL;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_EXTENDED_SCENE_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_POST_RAW_SENSITIVITY_BOOST;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_SCENE_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_SETTINGS_OVERRIDE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_VIDEO_STABILIZATION_MODE;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_ZOOM_METHOD;
    public static android.hardware.camera2.CaptureRequest.Key CONTROL_ZOOM_RATIO;
    public static android.os.Parcelable.Creator CREATOR = husk.StubCreator.of(CaptureRequest.class);
    public static android.hardware.camera2.CaptureRequest.Key DISTORTION_CORRECTION_MODE;
    public static android.hardware.camera2.CaptureRequest.Key EDGE_MODE;
    public static android.hardware.camera2.CaptureRequest.Key EXTENSION_STRENGTH;
    public static android.hardware.camera2.CaptureRequest.Key FLASH_MODE;
    public static android.hardware.camera2.CaptureRequest.Key FLASH_STRENGTH_LEVEL;
    public static android.hardware.camera2.CaptureRequest.Key HOT_PIXEL_MODE;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_GPS_COORDINATES;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_GPS_LOCATION;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_GPS_PROCESSING_METHOD;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_GPS_TIMESTAMP;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_ORIENTATION;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_QUALITY;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_THUMBNAIL_QUALITY;
    public static android.hardware.camera2.CaptureRequest.Key JPEG_THUMBNAIL_SIZE;
    public static android.hardware.camera2.CaptureRequest.Key LED_TRANSMIT;
    public static android.hardware.camera2.CaptureRequest.Key LENS_APERTURE;
    public static android.hardware.camera2.CaptureRequest.Key LENS_FILTER_DENSITY;
    public static android.hardware.camera2.CaptureRequest.Key LENS_FOCAL_LENGTH;
    public static android.hardware.camera2.CaptureRequest.Key LENS_FOCUS_DISTANCE;
    public static android.hardware.camera2.CaptureRequest.Key LENS_OPTICAL_STABILIZATION_MODE;
    public static android.hardware.camera2.CaptureRequest.Key NOISE_REDUCTION_MODE;
    public static android.hardware.camera2.CaptureRequest.Key REPROCESS_EFFECTIVE_EXPOSURE_FACTOR;
    public static android.hardware.camera2.CaptureRequest.Key REQUEST_ID;
    public static final int REQUEST_TYPE_COUNT = 3;
    public static final int REQUEST_TYPE_REGULAR = 0;
    public static final int REQUEST_TYPE_REPROCESS = 1;
    public static final int REQUEST_TYPE_ZSL_STILL = 2;
    public static android.hardware.camera2.CaptureRequest.Key SCALER_CROP_REGION;
    public static android.hardware.camera2.CaptureRequest.Key SCALER_CROP_REGION_SET;
    public static android.hardware.camera2.CaptureRequest.Key SCALER_ROTATE_AND_CROP;
    public static android.hardware.camera2.CaptureRequest.Key SENSOR_EXPOSURE_TIME;
    public static android.hardware.camera2.CaptureRequest.Key SENSOR_FRAME_DURATION;
    public static android.hardware.camera2.CaptureRequest.Key SENSOR_PIXEL_MODE;
    public static android.hardware.camera2.CaptureRequest.Key SENSOR_SENSITIVITY;
    public static android.hardware.camera2.CaptureRequest.Key SENSOR_TEST_PATTERN_DATA;
    public static android.hardware.camera2.CaptureRequest.Key SENSOR_TEST_PATTERN_MODE;
    public static android.hardware.camera2.CaptureRequest.Key SHADING_MODE;
    public static android.hardware.camera2.CaptureRequest.Key STATISTICS_FACE_DETECT_MODE;
    public static android.hardware.camera2.CaptureRequest.Key STATISTICS_HOT_PIXEL_MAP_MODE;
    public static android.hardware.camera2.CaptureRequest.Key STATISTICS_LENS_SHADING_MAP_MODE;
    public static android.hardware.camera2.CaptureRequest.Key STATISTICS_OIS_DATA_MODE;
    public static android.hardware.camera2.CaptureRequest.Key TONEMAP_CURVE;
    public static android.hardware.camera2.CaptureRequest.Key TONEMAP_CURVE_BLUE;
    public static android.hardware.camera2.CaptureRequest.Key TONEMAP_CURVE_GREEN;
    public static android.hardware.camera2.CaptureRequest.Key TONEMAP_CURVE_RED;
    public static android.hardware.camera2.CaptureRequest.Key TONEMAP_GAMMA;
    public static android.hardware.camera2.CaptureRequest.Key TONEMAP_MODE;
    public static android.hardware.camera2.CaptureRequest.Key TONEMAP_PRESET_CURVE;
    public boolean containsTarget(android.view.Surface p0) { return getTargets().contains(p0); }
    public void convertSurfaceToStreamId(android.util.SparseArray p0) {}
    public int describeContents() { return 0; }
    protected void finalize() {}
    public java.lang.Object get(android.hardware.camera2.CaptureRequest.Key p0) { return huskProps.get(p0.getName()); }
    public java.lang.Class getKeyClass() { return (java.lang.Class) huskProps.get("KeyClass"); }
    public java.util.List getKeys() { return (huskProps.get("Keys") != null ? (java.util.List) huskProps.get("Keys") : new java.util.ArrayList()); }
    public java.lang.String getLogicalCameraId() { return (java.lang.String) huskProps.get("LogicalCameraId"); }
    public android.hardware.camera2.impl.CameraMetadataNative getNativeCopy() { return (android.hardware.camera2.impl.CameraMetadataNative) huskProps.get("NativeCopy"); }
    protected java.lang.Object getProtected(android.hardware.camera2.CaptureRequest.Key p0) { return null; }
    public java.lang.Object getProtected(java.lang.Object p0) { return null; }
    public int getReprocessableSessionId() { return (huskProps.get("ReprocessableSessionId") instanceof Integer ? (Integer) huskProps.get("ReprocessableSessionId") : 0); }
    public int getRequestType() { return (huskProps.get("RequestType") instanceof Integer ? (Integer) huskProps.get("RequestType") : 0); }
    public int[] getStreamIds() { return (int[]) huskProps.get("StreamIds"); }
    public int[] getSurfaceIds() { return (int[]) huskProps.get("SurfaceIds"); }
    public java.lang.Object getTag() { return (java.lang.Object) huskProps.get("Tag"); }
    public java.util.Collection getTargets() { return (huskProps.get("Targets") != null ? (java.util.Collection) huskProps.get("Targets") : new java.util.ArrayList()); }
    public boolean isPartOfCRequestList() { return (huskProps.get("PartOfCRequestList") instanceof Boolean ? (Boolean) huskProps.get("PartOfCRequestList") : false); }
    public boolean isReprocess() { return (huskProps.get("Reprocess") instanceof Boolean ? (Boolean) huskProps.get("Reprocess") : false); }
    public void recoverStreamIdToSurface() {}
    public void writeToParcel(android.os.Parcel p0, int p1) {}
    protected CaptureRequest() { super(); }
    public static final class Builder {
        private final java.util.LinkedHashSet<android.view.Surface> targets = new java.util.LinkedHashSet<>();
        public Builder() {}
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Builder(android.hardware.camera2.impl.CameraMetadataNative p0, boolean p1, int p2, java.lang.String p3, java.util.Set p4) {}
        public void addTarget(android.view.Surface p0) { targets.add(p0); }
        public android.hardware.camera2.CaptureRequest build() { android.hardware.camera2.CaptureRequest x = new android.hardware.camera2.CaptureRequest(); x.huskProps.putAll(huskProps); x.huskProps.put("Targets", new java.util.ArrayList<>(targets)); return x; }
        public java.lang.Object get(android.hardware.camera2.CaptureRequest.Key p0) { return huskProps.get(p0.getName()); }
        public java.lang.Object getPhysicalCameraKey(android.hardware.camera2.CaptureRequest.Key p0, java.lang.String p1) { return null; }
        public boolean isEmpty() { return (huskProps.get("Empty") instanceof Boolean ? (Boolean) huskProps.get("Empty") : false); }
        public void removeTarget(android.view.Surface p0) { targets.remove(p0); }
        public void set(android.hardware.camera2.CaptureRequest.Key p0, java.lang.Object p1) { huskProps.put(p0.getName(), p1); }
        public void setPartOfCHSRequestList(boolean p0) { huskProps.put("PartOfCHSRequestList", Boolean.valueOf(p0)); }
        public android.hardware.camera2.CaptureRequest.Builder setPhysicalCameraKey(android.hardware.camera2.CaptureRequest.Key p0, java.lang.Object p1, java.lang.String p2) { return this; }
        public void setTag(java.lang.Object p0) { huskProps.put("Tag", p0); }

    }
    public static final class Key {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Key(java.lang.String p0, android.hardware.camera2.utils.TypeReference p1) { huskProps.put("Name", p0); }
        public Key(java.lang.String p0, java.lang.Class p1) { huskProps.put("Name", p0); }
        public Key(java.lang.String p0, java.lang.Class p1, long p2) { huskProps.put("Name", p0); }
        @Override public boolean equals(Object other) { return other instanceof Key && java.util.Objects.equals(getName(), ((Key)other).getName()); }
        @Override public int hashCode() { return java.util.Objects.hashCode(getName()); }
        public java.lang.String getName() { return (java.lang.String) huskProps.get("Name"); }
        public android.hardware.camera2.impl.CameraMetadataNative.Key getNativeKey() { return (android.hardware.camera2.impl.CameraMetadataNative.Key) huskProps.get("NativeKey"); }
        public long getVendorId() { return (huskProps.get("VendorId") instanceof Long ? (Long) huskProps.get("VendorId") : 0L); }
        Key() { this((java.lang.String) null, (android.hardware.camera2.utils.TypeReference) null); }
    }
}
