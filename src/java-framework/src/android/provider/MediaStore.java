// Started from tools/compat/genstubs.py's signatures; the content URIs are MediaStore's own.
package android.provider;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public final class MediaStore {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public static final java.lang.String ACCESS_MEDIA_OWNER_PACKAGE_NAME_PERMISSION = "com.android.providers.media.permission.ACCESS_MEDIA_OWNER_PACKAGE_NAME";
    public static final java.lang.String ACCESS_OEM_METADATA_PERMISSION = "com.android.providers.media.permission.ACCESS_OEM_METADATA";
    public static final java.lang.String ACTION_IMAGE_CAPTURE = "android.media.action.IMAGE_CAPTURE";
    public static final java.lang.String ACTION_IMAGE_CAPTURE_SECURE = "android.media.action.IMAGE_CAPTURE_SECURE";
    public static final java.lang.String ACTION_MOTION_PHOTO_CAPTURE = "android.provider.action.MOTION_PHOTO_CAPTURE";
    public static final java.lang.String ACTION_MOTION_PHOTO_CAPTURE_SECURE = "android.provider.action.MOTION_PHOTO_CAPTURE_SECURE";
    public static final java.lang.String ACTION_PICK_IMAGES = "android.provider.action.PICK_IMAGES";
    public static final java.lang.String ACTION_PICK_IMAGES_SETTINGS = "android.provider.action.PICK_IMAGES_SETTINGS";
    public static final java.lang.String ACTION_REVIEW = "android.provider.action.REVIEW";
    public static final java.lang.String ACTION_REVIEW_SECURE = "android.provider.action.REVIEW_SECURE";
    public static final java.lang.String ACTION_USER_SELECT_IMAGES_FOR_APP = "android.provider.action.USER_SELECT_IMAGES_FOR_APP";
    public static final java.lang.String ACTION_VIDEO_CAPTURE = "android.media.action.VIDEO_CAPTURE";
    public static final java.lang.String AUTHORITY = "media";
    public static final java.lang.String AUTHORITY_LEGACY = "media_legacy";
    public static final android.net.Uri AUTHORITY_LEGACY_URI = android.net.Uri.parse("content://media_legacy");
    public static final android.net.Uri AUTHORITY_URI = android.net.Uri.parse("content://media");
    public static final java.lang.String BULK_UPDATE_OEM_METADATA_CALL = "bulk_update_oem_metadata";
    public static final java.lang.String CREATE_CANCELLATION_SIGNAL_CALL = "create_cancellation_signal_call";
    public static final java.lang.String CREATE_CANCELLATION_SIGNAL_RESULT = "create_cancellation_signal_result";
    public static final java.lang.String CREATE_DELETE_REQUEST_CALL = "create_delete_request";
    public static final java.lang.String CREATE_FAVORITE_REQUEST_CALL = "create_favorite_request";
    public static final java.lang.String CREATE_SURFACE_CONTROLLER = "create_surface_controller";
    public static final java.lang.String CREATE_TRASH_REQUEST_CALL = "create_trash_request";
    public static final java.lang.String CREATE_WRITE_REQUEST_CALL = "create_write_request";
    public static final java.lang.String DELETE_BACKED_UP_FILE_PATHS = "delete_backed_up_file_paths";
    public static final java.lang.String ENSURE_PROVIDERS_CALL = "ensure_providers_call";
    public static final java.lang.String EXTERNAL_STORAGE_PROVIDER_AUTHORITY = "com.android.externalstorage.documents";
    public static final java.lang.String EXTRA_ACCEPT_ORIGINAL_MEDIA_FORMAT = "android.provider.extra.ACCEPT_ORIGINAL_MEDIA_FORMAT";
    public static final java.lang.String EXTRA_ALBUM_AUTHORITY = "album_authority";
    public static final java.lang.String EXTRA_ALBUM_ID = "album_id";
    public static final java.lang.String EXTRA_BRIGHTNESS = "android.provider.extra.BRIGHTNESS";
    public static final java.lang.String EXTRA_CALLING_PACKAGE_UID = "calling_package_uid";
    public static final java.lang.String EXTRA_CLIP_DATA = "clip_data";
    public static final java.lang.String EXTRA_CLOUD_PROVIDER = "cloud_provider";
    public static final java.lang.String EXTRA_CLOUD_PROVIDER_RESULT = "cloud_provider_result";
    public static final java.lang.String EXTRA_CONTENT_VALUES = "content_values";
    public static final java.lang.String EXTRA_DURATION_LIMIT = "android.intent.extra.durationLimit";
    public static final java.lang.String EXTRA_FILE_DESCRIPTOR = "file_descriptor";
    public static final java.lang.String EXTRA_FINISH_ON_COMPLETION = "android.intent.extra.finishOnCompletion";
    public static final java.lang.String EXTRA_FULL_SCREEN = "android.intent.extra.fullScreen";
    public static final java.lang.String EXTRA_IS_STABLE_URIS_ENABLED = "is_stable_uris_enabled";
    public static final java.lang.String EXTRA_IS_SYSTEM_GALLERY_RESPONSE = "is_system_gallery_response";
    public static final java.lang.String EXTRA_IS_SYSTEM_GALLERY_UID = "is_system_gallery_uid";
    public static final java.lang.String EXTRA_LOCAL_ONLY = "is_local_only";
    public static final java.lang.String EXTRA_LOCAL_PROVIDER = "local_provider";
    public static final java.lang.String EXTRA_MEDIA_ALBUM = "android.intent.extra.album";
    public static final java.lang.String EXTRA_MEDIA_ARTIST = "android.intent.extra.artist";
    public static final java.lang.String EXTRA_MEDIA_CAPABILITIES = "android.provider.extra.MEDIA_CAPABILITIES";
    public static final java.lang.String EXTRA_MEDIA_CAPABILITIES_UID = "android.provider.extra.MEDIA_CAPABILITIES_UID";
    public static final java.lang.String EXTRA_MEDIA_FOCUS = "android.intent.extra.focus";
    public static final java.lang.String EXTRA_MEDIA_GENRE = "android.intent.extra.genre";
    public static final java.lang.String EXTRA_MEDIA_PLAYLIST = "android.intent.extra.playlist";
    public static final java.lang.String EXTRA_MEDIA_RADIO_CHANNEL = "android.intent.extra.radio_channel";
    public static final java.lang.String EXTRA_MEDIA_TITLE = "android.intent.extra.title";
    public static final java.lang.String EXTRA_MODE = "android.provider.extra.MODE";
    public static final java.lang.String EXTRA_OPEN_ASSET_FILE_REQUEST = "open_asset_file_request";
    public static final java.lang.String EXTRA_OPEN_FILE_REQUEST = "open_file_request";
    public static final java.lang.String EXTRA_OUTPUT = "output";
    public static final java.lang.String EXTRA_PICKER_PRE_SELECTION_URIS = "android.provider.extra.PICKER_PRE_SELECTION_URIS";
    public static final java.lang.String EXTRA_PICK_IMAGES_ACCENT_COLOR = "android.provider.extra.PICK_IMAGES_ACCENT_COLOR";
    public static final java.lang.String EXTRA_PICK_IMAGES_HIGHLIGHT_ALBUM = "android.provider.extra.PICK_IMAGES_HIGHLIGHT_ALBUM";
    public static final java.lang.String EXTRA_PICK_IMAGES_HIGHLIGHT_SEARCH_RESULTS = "android.provider.extra.PICK_IMAGES_HIGHLIGHT_SEARCH_RESULTS";
    public static final java.lang.String EXTRA_PICK_IMAGES_IN_ORDER = "android.provider.extra.PICK_IMAGES_IN_ORDER";
    public static final java.lang.String EXTRA_PICK_IMAGES_LAUNCH_TAB = "android.provider.extra.PICK_IMAGES_LAUNCH_TAB";
    public static final java.lang.String EXTRA_PICK_IMAGES_MAX = "android.provider.extra.PICK_IMAGES_MAX";
    public static final java.lang.String EXTRA_RESULT = "result";
    public static final java.lang.String EXTRA_SCREEN_ORIENTATION = "android.intent.extra.screenOrientation";
    public static final java.lang.String EXTRA_SHOW_ACTION_ICONS = "android.intent.extra.showActionIcons";
    public static final java.lang.String EXTRA_SIZE_LIMIT = "android.intent.extra.sizeLimit";
    public static final java.lang.String EXTRA_URI = "uri";
    public static final java.lang.String EXTRA_URI_LIST = "uri_list";
    public static final java.lang.String EXTRA_URI_PERMISSIONS = "uriPermissions";
    public static final java.lang.String EXTRA_VIDEO_QUALITY = "android.intent.extra.videoQuality";
    public static final java.lang.String FILE_PATH = "file_path";
    public static final java.lang.String FINISH_LEGACY_MIGRATION_CALL = "finish_legacy_migration";
    public static final java.lang.String GET_BACKUP_FILES = "get_backup_files";
    public static final java.lang.String GET_CLOUD_PROVIDER_CALL = "get_cloud_provider";
    public static final java.lang.String GET_CLOUD_PROVIDER_DETAILS = "get_cloud_provider_details";
    public static final java.lang.String GET_CLOUD_PROVIDER_DETAILS_RESULT = "get_cloud_provider_details_result";
    public static final java.lang.String GET_CLOUD_PROVIDER_LABEL_CALL = "get_cloud_provider_label";
    public static final java.lang.String GET_CLOUD_PROVIDER_RESULT = "get_cloud_provider_result";
    public static final java.lang.String GET_DOCUMENT_URI_CALL = "get_document_uri";
    public static final java.lang.String GET_GENERATION_CALL = "get_generation";
    public static final java.lang.String GET_MEDIA_URI_CALL = "get_media_uri";
    public static final java.lang.String GET_OWNER_PACKAGE_NAME = "get_owner_package_name";
    public static final java.lang.String GET_RECOVERY_DATA = "get_recovery_data";
    public static final java.lang.String GET_REDACTED_MEDIA_URI_CALL = "get_redacted_media_uri";
    public static final java.lang.String GET_REDACTED_MEDIA_URI_LIST_CALL = "get_redacted_media_uri_list";
    public static final java.lang.String GET_VERSION_CALL = "get_version";
    public static final java.lang.String GRANT_MEDIA_READ_FOR_PACKAGE_CALL = "grant_media_read_for_package";
    public static final java.lang.String INTENT_ACTION_MEDIA_PLAY_FROM_SEARCH = "android.media.action.MEDIA_PLAY_FROM_SEARCH";
    public static final java.lang.String INTENT_ACTION_MEDIA_SEARCH = "android.intent.action.MEDIA_SEARCH";
    public static final java.lang.String INTENT_ACTION_MUSIC_PLAYER = "android.intent.action.MUSIC_PLAYER";
    public static final java.lang.String INTENT_ACTION_STILL_IMAGE_CAMERA = "android.media.action.STILL_IMAGE_CAMERA";
    public static final java.lang.String INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE = "android.media.action.STILL_IMAGE_CAMERA_SECURE";
    public static final java.lang.String INTENT_ACTION_TEXT_OPEN_FROM_SEARCH = "android.media.action.TEXT_OPEN_FROM_SEARCH";
    public static final java.lang.String INTENT_ACTION_VIDEO_CAMERA = "android.media.action.VIDEO_CAMERA";
    public static final java.lang.String INTENT_ACTION_VIDEO_PLAY_FROM_SEARCH = "android.media.action.VIDEO_PLAY_FROM_SEARCH";
    public static final java.lang.String IS_CURRENT_CLOUD_PROVIDER_CALL = "is_current_cloud_provider";
    public static final java.lang.String IS_SUPPORTED_CLOUD_PROVIDER_CALL = "is_supported_cloud_provider";
    public static final java.lang.String IS_SYSTEM_GALLERY_CALL = "is_system_gallery";
    public static final java.lang.String KEY_PICK_IMAGES_HIGHLIGHT_ALBUM_ID = "android.provider.media.key.PICK_IMAGES_HIGHLIGHT_MEDIA_ALBUM_ID";
    public static final java.lang.String KEY_PICK_IMAGES_HIGHLIGHT_SEARCH_TEXT_QUERY = "android.provider.media.key.PICK_IMAGES_HIGHLIGHT_SEARCH_TEXT_QUERY";
    public static final java.lang.String KEY_PICK_IMAGES_HIGHLIGHT_TYPE = "android.provider.media.key.PICK_IMAGES_HIGHLIGHT_TYPE";
    public static final java.lang.String MARK_FILE_AS_RESTORED = "mark_file_as_restored";
    public static final java.lang.String MARK_FILE_AS_TRASHED = "mark_file_as_trashed";
    public static final java.lang.String MARK_MEDIA_AS_FAVORITE = "mark_media_as_favorite";
    public static final int MATCH_DEFAULT = 0;
    public static final int MATCH_EXCLUDE = 2;
    public static final int MATCH_INCLUDE = 1;
    public static final int MATCH_ONLY = 3;
    public static final java.lang.String MEDIA_IGNORE_FILENAME = ".nomedia";
    public static final java.lang.String MEDIA_SCANNER_VOLUME = "volume";
    public static final java.lang.String MEDIA_SERVICE_V2_CALL = "media_service_v2_call";
    public static final java.lang.String META_DATA_REVIEW_GALLERY_PREWARM_SERVICE = "android.media.review_gallery_prewarm_service";
    public static final java.lang.String META_DATA_STILL_IMAGE_CAMERA_PREWARM_SERVICE = "android.media.still_image_camera_preview_service";
    public static final int MY_UID = 0;
    public static final int MY_USER_ID = 0;
    public static final java.lang.String NOTIFY_CLOUD_MEDIA_CHANGED_EVENT_CALL = "notify_cloud_media_changed_event";
    public static final java.lang.String OPEN_ASSET_FILE_CALL = "open_asset_file_call";
    public static final java.lang.String OPEN_FILE_CALL = "open_file_call";
    public static final java.lang.String PARAM_DELETE_DATA = "deletedata";
    public static final java.lang.String PARAM_INCLUDE_PENDING = "includePending";
    public static final java.lang.String PARAM_LIMIT = "limit";
    public static final java.lang.String PARAM_PROGRESS = "progress";
    public static final java.lang.String PARAM_REQUIRE_ORIGINAL = "requireOriginal";
    public static final java.lang.String PARENT_FILE_PATH = "parent_file_path";
    public static final int PER_USER_RANGE = 100000;
    public static final java.lang.String PICKER_GET_SEARCH_PROVIDERS_CALL = "picker_internal_get_search_providers";
    public static final java.lang.String PICKER_INTERNAL_SEARCH_MEDIA_INIT_CALL = "picker_internal_search_media_init";
    public static final java.lang.String PICKER_MEDIA_INIT_CALL = "picker_media_init";
    public static final java.lang.String PICKER_MEDIA_IN_MEDIA_SET_INIT_CALL = "picker_media_in_media_set_init";
    public static final java.lang.String PICKER_MEDIA_SETS_INIT_CALL = "picker_media_sets_init_call";
    public static final java.lang.String PICKER_TRANSCODE_CALL = "picker_transcode";
    public static final java.lang.String PICKER_TRANSCODE_RESULT = "picker_transcode_result";
    public static final java.lang.String PICK_IMAGES_HIGHLIGHT_ALBUM_CAMERA = "android.provider.media.PICK_IMAGES_HIGHLIGHT_ALBUM_CAMERA";
    public static final java.lang.String PICK_IMAGES_HIGHLIGHT_ALBUM_DOWNLOADS = "android.provider.media.PICK_IMAGES_HIGHLIGHT_ALBUM_DOWNLOADS";
    public static final java.lang.String PICK_IMAGES_HIGHLIGHT_ALBUM_FAVORITES = "android.provider.media.PICK_IMAGES_HIGHLIGHT_ALBUM_FAVORITES";
    public static final java.lang.String PICK_IMAGES_HIGHLIGHT_ALBUM_SCREENSHOTS = "android.provider.media.PICK_IMAGES_HIGHLIGHT_ALBUM_SCREENSHOTS";
    public static final java.lang.String PICK_IMAGES_HIGHLIGHT_ALBUM_VIDEOS = "android.provider.media.PICK_IMAGES_HIGHLIGHT_ALBUM_VIDEOS";
    public static final int PICK_IMAGES_HIGHLIGHT_TYPE_COLLAPSED = 0;
    public static final int PICK_IMAGES_HIGHLIGHT_TYPE_EXPANDED = 1;
    public static final int PICK_IMAGES_TAB_ALBUMS = 0;
    public static final int PICK_IMAGES_TAB_IMAGES = 1;
    public static final java.lang.String QUERY_ARG_ALBUM_AUTHORITY = "android:query-arg-album_authority";
    public static final java.lang.String QUERY_ARG_ALBUM_ID = "android:query-arg-album_id";
    public static final java.lang.String QUERY_ARG_ALLOW_MOVEMENT = "android:query-arg-allow-movement";
    public static final java.lang.String QUERY_ARG_DEFER_SCAN = "android:query-arg-defer-scan";
    public static final java.lang.String QUERY_ARG_INCLUDE_RECENTLY_UNMOUNTED_VOLUMES = "android:query-arg-recently-unmounted-volumes";
    public static final java.lang.String QUERY_ARG_LATEST_SELECTION_ONLY = "android:query-arg-latest-selection-only";
    public static final java.lang.String QUERY_ARG_MATCH_FAVORITE = "android:query-arg-match-favorite";
    public static final java.lang.String QUERY_ARG_MATCH_PENDING = "android:query-arg-match-pending";
    public static final java.lang.String QUERY_ARG_MATCH_TRASHED = "android:query-arg-match-trashed";
    public static final java.lang.String QUERY_ARG_MEDIA_STANDARD_SORT_ORDER = "android:query-arg-media-standard-sort-order";
    public static final java.lang.String QUERY_ARG_MIME_TYPE = "android:query-arg-mime_type";
    public static final java.lang.String QUERY_ARG_REDACTED_URI = "android:query-arg-redacted-uri";
    public static final java.lang.String QUERY_ARG_RELATED_URI = "android:query-arg-related-uri";
    public static final java.lang.String QUERY_ARG_SIZE_BYTES = "android:query-arg-size_bytes";
    public static final java.lang.String QUERY_FILE_ATTRS_FROM_LEVELDB = "query_file_attrs_from_leveldb";
    public static final java.lang.String READ_BACKUP = "read_backup";
    public static final java.lang.String REMOVE_RECOVERY_DATA = "remove_recovery_data";
    public static final java.lang.String RESOLVE_PLAYLIST_MEMBERS_CALL = "resolve_playlist_members";
    public static final java.lang.String REVOKED_ALL_READ_GRANTS_FOR_PACKAGE_CALL = "revoke_all_media_grants_for_package";
    public static final java.lang.String REVOKE_READ_GRANT_FOR_PACKAGE_CALL = "revoke_media_read_for_package";
    public static final java.lang.String RUN_IDLE_MAINTENANCE_CALL = "run_idle_maintenance";
    public static final java.lang.String RUN_IDLE_MAINTENANCE_FOR_STABLE_URIS = "idle_maintenance_for_stable_uris";
    public static final java.lang.String SCAN_FILE_CALL = "scan_file";
    public static final java.lang.String SCAN_VOLUME_CALL = "scan_volume";
    public static final java.lang.String SET_CLOUD_PROVIDER_CALL = "set_cloud_provider";
    public static final java.lang.String SET_CLOUD_PROVIDER_RESULT = "set_cloud_provider_result";
    public static final java.lang.String SET_STABLE_URIS_FLAG = "set_stable_uris_flag";
    public static final java.lang.String START_LEGACY_MIGRATION_CALL = "start_legacy_migration";
    public static final java.lang.String SYNC_PROVIDERS_CALL = "sync_providers";
    public static final java.lang.String UNKNOWN_STRING = "<unknown>";
    public static final java.lang.String UPDATE_OEM_METADATA_PERMISSION = "com.android.providers.media.permission.UPDATE_OEM_METADATA";
    public static final java.lang.String USES_FUSE_PASSTHROUGH = "uses_fuse_passthrough";
    public static final java.lang.String USES_FUSE_PASSTHROUGH_RESULT = "uses_fuse_passthrough_result";
    public static final java.lang.String VOLUME_DEMO = "demo";
    public static final java.lang.String VOLUME_EXTERNAL = "external";
    public static final java.lang.String VOLUME_EXTERNAL_PRIMARY = "external_primary";
    public static final java.lang.String VOLUME_INTERNAL = "internal";
    public static final java.lang.String WAIT_FOR_IDLE_CALL = "wait_for_idle";
    public MediaStore() {}
    public static void bulkUpdateOemMetadataInNextScan(android.content.Context p0) {}
    public static boolean canManageMedia(android.content.Context p0) { return false; }
    public static java.lang.String checkArgumentVolumeName(java.lang.String p0) { return null; }
    public static android.app.PendingIntent createDeleteRequest(android.content.ContentResolver p0, java.util.Collection p1) { return null; }
    public static android.app.PendingIntent createFavoriteRequest(android.content.ContentResolver p0, java.util.Collection p1, boolean p2) { return null; }
    public static android.app.PendingIntent createTrashRequest(android.content.ContentResolver p0, java.util.Collection p1, boolean p2) { return null; }
    public static android.app.PendingIntent createWriteRequest(android.content.ContentResolver p0, java.util.Collection p1) { return null; }
    public static void deleteBackedUpFilePaths(android.content.ContentResolver p0, java.lang.String p1) {}
    public static void finishLegacyMigration(android.content.ContentResolver p0, java.lang.String p1) {}
    public static java.lang.String[] getBackupFiles(android.content.ContentResolver p0) { return null; }
    public static java.lang.String getCurrentCloudProvider(android.content.ContentResolver p0) { return null; }
    public static android.net.Uri getDocumentUri(android.content.Context p0, android.net.Uri p1) { return null; }
    public static java.util.Set getExternalVolumeNames(android.content.Context p0) { return new java.util.HashSet(); }
    public static long getGeneration(android.content.ContentResolver p0, java.lang.String p1) { return 0L; }
    public static long getGeneration(android.content.Context p0, java.lang.String p1) { return 0L; }
    public static boolean getIncludePending(android.net.Uri p0) { return false; }
    public static android.net.Uri getMediaScannerUri() { return null; }
    public static android.net.Uri getMediaUri(android.content.Context p0, android.net.Uri p1) { return null; }
    public static android.os.ParcelFileDescriptor getOriginalMediaFormatFileDescriptor(android.content.Context p0, android.os.ParcelFileDescriptor p1) { return null; }
    public static java.lang.String getOwnerPackageName(android.content.ContentResolver p0, int p1) { return null; }
    public static int getPickImagesMaxLimit() { return 0; }
    public static java.util.Set getRecentExternalVolumeNames(android.content.Context p0) { return new java.util.HashSet(); }
    public static java.lang.String[] getRecoveryData(android.content.ContentResolver p0) { return null; }
    public static android.net.Uri getRedactedUri(android.content.ContentResolver p0, android.net.Uri p1) { return null; }
    public static java.util.List getRedactedUri(android.content.ContentResolver p0, java.util.List p1) { return new java.util.ArrayList(); }
    public static boolean getRequireOriginal(android.net.Uri p0) { return false; }
    public static java.lang.String getVersion(android.content.Context p0) { return null; }
    public static java.lang.String getVersion(android.content.Context p0, java.lang.String p1) { return null; }
    public static java.lang.String getVolumeName(android.net.Uri p0) { java.util.List<String> s = p0.getPathSegments(); return s.isEmpty() ? VOLUME_EXTERNAL : s.get(0); }
    public static java.lang.String getVolumeName(java.io.File p0) { return null; }
    public static void grantMediaReadForPackage(android.content.Context p0, int p1, java.util.List p2) {}
    public static boolean isCurrentCloudMediaProviderAuthority(android.content.ContentResolver p0, java.lang.String p1) { return false; }
    public static boolean isCurrentSystemGallery(android.content.ContentResolver p0, int p1, java.lang.String p2) { return false; }
    public static boolean isKnownVolume(java.lang.String p0) { return false; }
    public static boolean isSupportedCloudMediaProviderAuthority(android.content.ContentResolver p0, java.lang.String p1) { return false; }
    public static void markIsFavoriteStatus(android.content.ContentResolver p0, java.util.Collection p1, boolean p2) {}
    public static void notifyCloudMediaChangedEvent(android.content.ContentResolver p0, java.lang.String p1, java.lang.String p2) {}
    public static android.content.res.AssetFileDescriptor openAssetFileDescriptor(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String p2, android.os.CancellationSignal p3) { return null; }
    public static android.os.ParcelFileDescriptor openFileDescriptor(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String p2, android.os.CancellationSignal p3) { return null; }
    public static android.content.res.AssetFileDescriptor openTypedAssetFileDescriptor(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String p2, android.os.Bundle p3, android.os.CancellationSignal p4) { return null; }
    public static java.lang.String readBackup(android.content.ContentResolver p0, java.lang.String p1, java.lang.String p2) { return null; }
    public static void removeRecoveryData(android.content.ContentResolver p0) {}
    public static void resolvePlaylistMembers(android.content.ContentResolver p0, android.net.Uri p1) {}
    public static java.lang.String restoreFileFromTrash(android.content.ContentResolver p0, java.lang.String p1, java.lang.String p2) { return null; }
    public static void revokeAllMediaReadForPackages(android.content.Context p0, int p1) {}
    public static void revokeMediaReadForPackages(android.content.Context p0, int p1, java.util.List p2) {}
    public static android.net.Uri rewriteToLegacy(android.net.Uri p0) { return null; }
    public static void runIdleMaintenance(android.content.ContentResolver p0) {}
    public static void runIdleMaintenanceForStableUris(android.content.ContentResolver p0) {}
    public static android.net.Uri scanFile(android.content.ContentResolver p0, java.io.File p1) { return null; }
    public static void scanVolume(android.content.ContentResolver p0, java.lang.String p1) {}
    public static android.net.Uri.Builder setIncludePending(android.net.Uri.Builder p0) { return p0; }
    public static android.net.Uri setIncludePending(android.net.Uri p0) { return p0; }
    public static android.net.Uri setRequireOriginal(android.net.Uri p0) { return p0; }
    public static void setStableUrisFlag(android.content.ContentResolver p0, java.lang.String p1, boolean p2) {}
    public static void startLegacyMigration(android.content.ContentResolver p0, java.lang.String p1) {}
    public static java.lang.String trashFile(android.content.ContentResolver p0, java.lang.String p1) { return null; }
    public static void waitForIdle(android.content.ContentResolver p0) {}
    public static final class Audio {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Audio() {}
        public static java.lang.String keyFor(java.lang.String p0) { return null; }
        public interface AlbumColumns {
            java.lang.String ALBUM = "album";
            java.lang.String ALBUM_ART = "album_art";
            java.lang.String ALBUM_ID = "album_id";
            java.lang.String ALBUM_KEY = "album_key";
            java.lang.String ARTIST = "artist";
            java.lang.String ARTIST_ID = "artist_id";
            java.lang.String ARTIST_KEY = "artist_key";
            java.lang.String FIRST_YEAR = "minyear";
            java.lang.String LAST_YEAR = "maxyear";
            java.lang.String NUMBER_OF_SONGS = "numsongs";
            java.lang.String NUMBER_OF_SONGS_FOR_ARTIST = "numsongs_by_artist";
        }
        public static final class Albums implements android.provider.BaseColumns, android.provider.MediaStore.Audio.AlbumColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/albums";
            public static final java.lang.String DEFAULT_SORT_ORDER = "album_key";
            public static final java.lang.String ENTRY_CONTENT_TYPE = "vnd.android.cursor.item/album";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public Albums() {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/audio/albums"); }
        }
        public interface ArtistColumns {
            java.lang.String ARTIST = "artist";
            java.lang.String ARTIST_KEY = "artist_key";
            java.lang.String NUMBER_OF_ALBUMS = "number_of_albums";
            java.lang.String NUMBER_OF_TRACKS = "number_of_tracks";
        }
        public static final class Artists implements android.provider.BaseColumns, android.provider.MediaStore.Audio.ArtistColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/artists";
            public static final java.lang.String DEFAULT_SORT_ORDER = "artist_key";
            public static final java.lang.String ENTRY_CONTENT_TYPE = "vnd.android.cursor.item/artist";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public Artists() {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/audio/artists"); }
            public static final class Albums implements android.provider.BaseColumns, android.provider.MediaStore.Audio.AlbumColumns {
                private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
                public Albums() {}
                public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.net.Uri.parse("content://media/" + p0 + "/audio/artists/" + p1 + "/albums"); }
            }
        }
        public interface AudioColumns extends android.provider.MediaStore.MediaColumns {
            java.lang.String ALBUM = "album";
            java.lang.String ALBUM_ARTIST = "album_artist";
            java.lang.String ALBUM_ID = "album_id";
            java.lang.String ALBUM_KEY = "album_key";
            java.lang.String ARTIST = "artist";
            java.lang.String ARTIST_ID = "artist_id";
            java.lang.String ARTIST_KEY = "artist_key";
            java.lang.String BITS_PER_SAMPLE = "bits_per_sample";
            java.lang.String BOOKMARK = "bookmark";
            java.lang.String COMPOSER = "composer";
            java.lang.String DURATION = "duration";
            java.lang.String GENRE = "genre";
            java.lang.String GENRE_ID = "genre_id";
            java.lang.String GENRE_KEY = "genre_key";
            java.lang.String IS_ALARM = "is_alarm";
            java.lang.String IS_AUDIOBOOK = "is_audiobook";
            java.lang.String IS_MUSIC = "is_music";
            java.lang.String IS_NOTIFICATION = "is_notification";
            java.lang.String IS_PODCAST = "is_podcast";
            java.lang.String IS_RECORDING = "is_recording";
            java.lang.String IS_RINGTONE = "is_ringtone";
            java.lang.String SAMPLERATE = "samplerate";
            java.lang.String TITLE_KEY = "title_key";
            java.lang.String TITLE_RESOURCE_URI = "title_resource_uri";
            java.lang.String TRACK = "track";
            java.lang.String YEAR = "year";
        }
        public static final class Genres implements android.provider.BaseColumns, android.provider.MediaStore.Audio.GenresColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/genre";
            public static final java.lang.String DEFAULT_SORT_ORDER = "name";
            public static final java.lang.String ENTRY_CONTENT_TYPE = "vnd.android.cursor.item/genre";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public Genres() {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/audio/genres"); }
            public static android.net.Uri getContentUriForAudioId(java.lang.String p0, int p1) { return null; }
            public static final class Members implements android.provider.MediaStore.Audio.AudioColumns {
                private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
                public static final java.lang.String AUDIO_ID = "audio_id";
                public static final java.lang.String CONTENT_DIRECTORY = "members";
                public static final java.lang.String DEFAULT_SORT_ORDER = "title_key";
                public static final java.lang.String GENRE_ID = "genre_id";
                public Members() {}
                public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.net.Uri.parse("content://media/" + p0 + "/audio/genres/" + p1 + "/members"); }
            }
        }
        public interface GenresColumns {
            java.lang.String NAME = "name";
        }
        public static final class Media implements android.provider.MediaStore.Audio.AudioColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/audio";
            public static final java.lang.String DEFAULT_SORT_ORDER = "title_key";
            public static final java.lang.String ENTRY_CONTENT_TYPE = "vnd.android.cursor.item/audio";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final java.lang.String EXTRA_MAX_BYTES = "android.provider.MediaStore.extra.MAX_BYTES";
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public static final java.lang.String RECORD_SOUND_ACTION = "android.provider.MediaStore.RECORD_SOUND";
            public Media() {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/audio/media"); }
            public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.content.ContentUris.withAppendedId(getContentUri(p0), p1); }
            public static android.net.Uri getContentUriForPath(java.lang.String p0) { return getContentUri("external"); }
        }
        public static final class Playlists implements android.provider.BaseColumns, android.provider.MediaStore.Audio.PlaylistsColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/playlist";
            public static final java.lang.String DEFAULT_SORT_ORDER = "name";
            public static final java.lang.String ENTRY_CONTENT_TYPE = "vnd.android.cursor.item/playlist";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public Playlists() {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/audio/playlists"); }
            public static final class Members implements android.provider.MediaStore.Audio.AudioColumns {
                private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
                public static final java.lang.String AUDIO_ID = "audio_id";
                public static final java.lang.String CONTENT_DIRECTORY = "members";
                public static final java.lang.String DEFAULT_SORT_ORDER = "play_order";
                public static final java.lang.String PLAYLIST_ID = "playlist_id";
                public static final java.lang.String PLAY_ORDER = "play_order";
                public static final java.lang.String _ID = "_id";
                public Members() {}
                public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.net.Uri.parse("content://media/" + p0 + "/audio/playlists/" + p1 + "/members"); }
                public static boolean moveItem(android.content.ContentResolver p0, long p1, int p2, int p3) { return false; }
            }
        }
        public interface PlaylistsColumns extends android.provider.MediaStore.MediaColumns {
            java.lang.String DATA = "_data";
            java.lang.String DATE_ADDED = "date_added";
            java.lang.String DATE_MODIFIED = "date_modified";
            java.lang.String NAME = "name";
        }
        public static final class Radio {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String ENTRY_CONTENT_TYPE = "vnd.android.cursor.item/radio";
            protected Radio() {}
        }
        public static class Thumbnails implements android.provider.BaseColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String ALBUM_ID = "album_id";
            public static final java.lang.String DATA = "_data";
            public Thumbnails() {}
        }
    }
    public interface DownloadColumns extends android.provider.MediaStore.MediaColumns {
        java.lang.String DESCRIPTION = "description";
        java.lang.String DOWNLOAD_URI = "download_uri";
        java.lang.String REFERER_URI = "referer_uri";
    }
    public static final class Downloads implements android.provider.MediaStore.DownloadColumns {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/download";
        public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
        public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
        public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/downloads"); }
        public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.content.ContentUris.withAppendedId(getContentUri(p0), p1); }
        public static android.net.Uri getContentUriForPath(java.lang.String p0) { return getContentUri("external"); }
        protected Downloads() {}
    }
    public static final class Files {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
        public static final java.lang.String TABLE = "files";
        public Files() {}
        public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/file"); }
        public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.content.ContentUris.withAppendedId(getContentUri(p0), p1); }
        public static android.net.Uri getContentUriForPath(java.lang.String p0) { return getContentUri("external"); }
        public static android.net.Uri getDirectoryUri(java.lang.String p0) { return null; }
        public static android.net.Uri getMtpObjectsUri(java.lang.String p0) { return null; }
        public static android.net.Uri getMtpObjectsUri(java.lang.String p0, long p1) { return null; }
        public static android.net.Uri getMtpReferencesUri(java.lang.String p0, long p1) { return null; }
        public interface FileColumns extends android.provider.MediaStore.MediaColumns {
            java.lang.String FORMAT = "format";
            java.lang.String MEDIA_TYPE = "media_type";
            int MEDIA_TYPE_AUDIO = 2;
            int MEDIA_TYPE_COUNT = 7;
            int MEDIA_TYPE_DOCUMENT = 6;
            int MEDIA_TYPE_IMAGE = 1;
            int MEDIA_TYPE_NONE = 0;
            int MEDIA_TYPE_PLAYLIST = 4;
            int MEDIA_TYPE_SUBTITLE = 5;
            int MEDIA_TYPE_VIDEO = 3;
            java.lang.String MIME_TYPE = "mime_type";
            java.lang.String PARENT = "parent";
            java.lang.String REDACTED_URI_ID = "redacted_uri_id";
            java.lang.String STORAGE_ID = "storage_id";
            java.lang.String TITLE = "title";
            int TRANSCODE_COMPLETE = 1;
            int TRANSCODE_EMPTY = 0;
            java.lang.String _MODIFIER = "_modifier";
            int _MODIFIER_CR = 2;
            int _MODIFIER_CR_PENDING_METADATA = 4;
            int _MODIFIER_FUSE = 1;
            int _MODIFIER_MEDIA_SCAN = 3;
            int _MODIFIER_SCHEMA_UPDATE = 5;
            java.lang.String _SPECIAL_FORMAT = "_special_format";
            int _SPECIAL_FORMAT_ANIMATED_WEBP = 3;
            int _SPECIAL_FORMAT_GIF = 1;
            int _SPECIAL_FORMAT_MOTION_PHOTO = 2;
            int _SPECIAL_FORMAT_NONE = 0;
            java.lang.String _TRANSCODE_STATUS = "_transcode_status";
            java.lang.String _USER_ID = "_user_id";
            java.lang.String _VIDEO_CODEC_TYPE = "_video_codec_type";
        }
    }
    public static final class Images {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public Images() {}
        public interface ImageColumns extends android.provider.MediaStore.MediaColumns {
            java.lang.String BUCKET_DISPLAY_NAME = "bucket_display_name";
            java.lang.String BUCKET_ID = "bucket_id";
            java.lang.String DATE_TAKEN = "datetaken";
            java.lang.String DESCRIPTION = "description";
            java.lang.String EXPOSURE_TIME = "exposure_time";
            java.lang.String F_NUMBER = "f_number";
            java.lang.String GROUP_ID = "group_id";
            java.lang.String ISO = "iso";
            java.lang.String IS_PRIVATE = "isprivate";
            java.lang.String LATITUDE = "latitude";
            java.lang.String LONGITUDE = "longitude";
            java.lang.String MINI_THUMB_MAGIC = "mini_thumb_magic";
            java.lang.String ORIENTATION = "orientation";
            java.lang.String PICASA_ID = "picasa_id";
            java.lang.String SCENE_CAPTURE_TYPE = "scene_capture_type";
        }
        public static final class Media implements android.provider.MediaStore.Images.ImageColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/image";
            public static final java.lang.String DEFAULT_SORT_ORDER = "bucket_display_name";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public Media() {}
            public static android.graphics.Bitmap getBitmap(android.content.ContentResolver p0, android.net.Uri p1) throws java.io.FileNotFoundException, java.io.IOException { try (java.io.InputStream in = p0.openInputStream(p1)) { return android.graphics.BitmapFactory.decodeStream(in); } }
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/images/media"); }
            public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.content.ContentUris.withAppendedId(getContentUri(p0), p1); }
            public static java.lang.String insertImage(android.content.ContentResolver p0, android.graphics.Bitmap p1, java.lang.String p2, java.lang.String p3) { return null; }
            public static java.lang.String insertImage(android.content.ContentResolver p0, java.lang.String p1, java.lang.String p2, java.lang.String p3) { return null; }
            public static android.database.Cursor query(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String[] p2) { return p0.query(p1, p2, null, null, null); }
            public static android.database.Cursor query(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String[] p2, java.lang.String p3, java.lang.String p4) { return p0.query(p1, p2, p3, null, p4); }
            public static android.database.Cursor query(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String[] p2, java.lang.String p3, java.lang.String[] p4, java.lang.String p5) { return null; }
        }
        public static class Thumbnails implements android.provider.BaseColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String DATA = "_data";
            public static final java.lang.String DEFAULT_SORT_ORDER = "image_id ASC";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final int FULL_SCREEN_KIND = 2;
            public static final java.lang.String HEIGHT = "height";
            public static final java.lang.String IMAGE_ID = "image_id";
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public static final java.lang.String KIND = "kind";
            public static final int MICRO_KIND = 3;
            public static final int MINI_KIND = 1;
            public static final java.lang.String THUMB_DATA = "thumb_data";
            public static final java.lang.String WIDTH = "width";
            public Thumbnails() {}
            public static void cancelThumbnailRequest(android.content.ContentResolver p0, long p1) {}
            public static void cancelThumbnailRequest(android.content.ContentResolver p0, long p1, long p2) {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/images/thumbnails"); }
            public static android.util.Size getKindSize(int p0) { return null; }
            public static android.graphics.Bitmap getThumbnail(android.content.ContentResolver p0, long p1, int p2, android.graphics.BitmapFactory.Options p3) { return null; }
            public static android.graphics.Bitmap getThumbnail(android.content.ContentResolver p0, long p1, long p2, int p3, android.graphics.BitmapFactory.Options p4) { return null; }
            public static android.database.Cursor query(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String[] p2) { return p0.query(p1, p2, null, null, null); }
            public static android.database.Cursor queryMiniThumbnail(android.content.ContentResolver p0, long p1, int p2, java.lang.String[] p3) { return null; }
            public static android.database.Cursor queryMiniThumbnails(android.content.ContentResolver p0, android.net.Uri p1, int p2, java.lang.String[] p3) { return null; }
        }
    }
    public interface MediaColumns extends android.provider.BaseColumns {
        java.lang.String ALBUM = "album";
        java.lang.String ALBUM_ARTIST = "album_artist";
        java.lang.String ARTIST = "artist";
        java.lang.String AUTHOR = "author";
        java.lang.String BITRATE = "bitrate";
        java.lang.String BUCKET_DISPLAY_NAME = "bucket_display_name";
        java.lang.String BUCKET_ID = "bucket_id";
        java.lang.String CAPTURE_FRAMERATE = "capture_framerate";
        java.lang.String CD_TRACK_NUMBER = "cd_track_number";
        java.lang.String COMPILATION = "compilation";
        java.lang.String COMPOSER = "composer";
        java.lang.String DATA = "_data";
        java.lang.String DATE_ADDED = "date_added";
        java.lang.String DATE_EXPIRES = "date_expires";
        java.lang.String DATE_MODIFIED = "date_modified";
        java.lang.String DATE_TAKEN = "datetaken";
        java.lang.String DISC_NUMBER = "disc_number";
        java.lang.String DISPLAY_NAME = "_display_name";
        java.lang.String DOCUMENT_ID = "document_id";
        java.lang.String DURATION = "duration";
        java.lang.String GENERATION_ADDED = "generation_added";
        java.lang.String GENERATION_MODIFIED = "generation_modified";
        int GENERATION_MODIFIED_UNCHANGED = -1;
        java.lang.String GENRE = "genre";
        java.lang.String GROUP_ID = "group_id";
        java.lang.String HEIGHT = "height";
        java.lang.String INFERRED_DATE = "inferred_date";
        java.lang.String INSTANCE_ID = "instance_id";
        java.lang.String IS_DOWNLOAD = "is_download";
        java.lang.String IS_DRM = "is_drm";
        java.lang.String IS_FAVORITE = "is_favorite";
        java.lang.String IS_PENDING = "is_pending";
        java.lang.String IS_TRASHED = "is_trashed";
        java.lang.String MIME_TYPE = "mime_type";
        java.lang.String NUM_TRACKS = "num_tracks";
        java.lang.String OEM_METADATA = "oem_metadata";
        java.lang.String ORIENTATION = "orientation";
        java.lang.String ORIGINAL_DOCUMENT_ID = "original_document_id";
        java.lang.String OWNER_PACKAGE_NAME = "owner_package_name";
        java.lang.String RELATIVE_PATH = "relative_path";
        java.lang.String RESOLUTION = "resolution";
        java.lang.String SIZE = "_size";
        java.lang.String TITLE = "title";
        java.lang.String VOLUME_NAME = "volume_name";
        java.lang.String WIDTH = "width";
        java.lang.String WRITER = "writer";
        java.lang.String XMP = "xmp";
        java.lang.String YEAR = "year";
    }
    public static class PickerMediaColumns {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static final java.lang.String DATA = "_data";
        public static final java.lang.String DATE_TAKEN = "datetaken";
        public static final java.lang.String DISPLAY_NAME = "_display_name";
        public static final java.lang.String DURATION_MILLIS = "duration";
        public static final java.lang.String HEIGHT = "height";
        public static final java.lang.String MIME_TYPE = "mime_type";
        public static final java.lang.String ORIENTATION = "orientation";
        public static final java.lang.String SIZE = "_size";
        public static final java.lang.String WIDTH = "width";
        protected PickerMediaColumns() {}
    }
    public static class ThumbnailConstants {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static final int FULL_SCREEN_KIND = 2;
        public static android.util.Size FULL_SCREEN_SIZE;
        public static final int MICRO_KIND = 3;
        public static android.util.Size MICRO_SIZE;
        public static final int MINI_KIND = 1;
        public static android.util.Size MINI_SIZE;
        public ThumbnailConstants() {}
        public static android.util.Size getKindSize(int p0) { return null; }
    }
    public static final class Video {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static final java.lang.String DEFAULT_SORT_ORDER = "_display_name";
        public Video() {}
        public static android.database.Cursor query(android.content.ContentResolver p0, android.net.Uri p1, java.lang.String[] p2) { return p0.query(p1, p2, null, null, null); }
        public static final class Media implements android.provider.MediaStore.Video.VideoColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String CONTENT_TYPE = "vnd.android.cursor.dir/video";
            public static final java.lang.String DEFAULT_SORT_ORDER = "title";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public Media() {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/video/media"); }
            public static android.net.Uri getContentUri(java.lang.String p0, long p1) { return android.content.ContentUris.withAppendedId(getContentUri(p0), p1); }
        }
        public static class Thumbnails implements android.provider.BaseColumns {
            private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
            public static final java.lang.String DATA = "_data";
            public static final java.lang.String DEFAULT_SORT_ORDER = "video_id ASC";
            public static final android.net.Uri EXTERNAL_CONTENT_URI = getContentUri("external");
            public static final int FULL_SCREEN_KIND = 2;
            public static final java.lang.String HEIGHT = "height";
            public static final android.net.Uri INTERNAL_CONTENT_URI = getContentUri("internal");
            public static final java.lang.String KIND = "kind";
            public static final int MICRO_KIND = 3;
            public static final int MINI_KIND = 1;
            public static final java.lang.String VIDEO_ID = "video_id";
            public static final java.lang.String WIDTH = "width";
            public Thumbnails() {}
            public static void cancelThumbnailRequest(android.content.ContentResolver p0, long p1) {}
            public static void cancelThumbnailRequest(android.content.ContentResolver p0, long p1, long p2) {}
            public static android.net.Uri getContentUri(java.lang.String p0) { return android.net.Uri.parse("content://media/" + p0 + "/video/thumbnails"); }
            public static android.util.Size getKindSize(int p0) { return null; }
            public static android.graphics.Bitmap getThumbnail(android.content.ContentResolver p0, long p1, int p2, android.graphics.BitmapFactory.Options p3) { return null; }
            public static android.graphics.Bitmap getThumbnail(android.content.ContentResolver p0, long p1, long p2, int p3, android.graphics.BitmapFactory.Options p4) { return null; }
        }
        public interface VideoColumns extends android.provider.MediaStore.MediaColumns {
            java.lang.String ALBUM = "album";
            java.lang.String ARTIST = "artist";
            java.lang.String BOOKMARK = "bookmark";
            java.lang.String BUCKET_DISPLAY_NAME = "bucket_display_name";
            java.lang.String BUCKET_ID = "bucket_id";
            java.lang.String CATEGORY = "category";
            java.lang.String COLOR_RANGE = "color_range";
            java.lang.String COLOR_STANDARD = "color_standard";
            java.lang.String COLOR_TRANSFER = "color_transfer";
            java.lang.String DATE_TAKEN = "datetaken";
            java.lang.String DESCRIPTION = "description";
            java.lang.String DURATION = "duration";
            java.lang.String GROUP_ID = "group_id";
            java.lang.String IS_PRIVATE = "isprivate";
            java.lang.String LANGUAGE = "language";
            java.lang.String LATITUDE = "latitude";
            java.lang.String LONGITUDE = "longitude";
            java.lang.String MINI_THUMB_MAGIC = "mini_thumb_magic";
            java.lang.String RESOLUTION = "resolution";
            java.lang.String TAGS = "tags";
        }
    }
}
