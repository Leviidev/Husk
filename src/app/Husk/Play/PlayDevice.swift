// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// The phone Husk says it is when it talks to Google Play: a Pixel 8 on Android 15.
///
/// Google Play decides which build of an app to hand out -- which processor's libraries, which screen density's split, which
/// app it will offer at all -- from what a device says about itself when it registers. An arm64 phone with a 1080p screen is
/// what the translation layer runs best (arm64-v8a code) and what most games are built for. The Play Store and Play Services
/// version numbers are current releases of Google's own apps; old ones are refused.
enum PlayDevice {
    static let fingerprint = "google/shiba/shiba:15/AP4A.250105.002/12701944:user/release-keys"
    static let buildID = "AP4A.250105.002"
    static let device = "shiba"
    static let product = "shiba"
    static let hardware = "shiba"
    static let model = "Pixel 8"
    static let brand = "google"
    static let manufacturer = "Google"
    static let radio = "g5300i-240820-241014-B-12497427"
    static let bootloader = "ripcurrent-15.1-12397981"
    static let sdk = 35
    static let release = "15"

    static let vendingVersion = 84582130
    static let vendingVersionString = "45.8.21-31 [0] [PR] 747433787"
    static let playServicesVersion = 251333035

    static let screenWidth = 1080
    static let screenHeight = 2400
    static let screenDensity = 420
    static let glEsVersion = 0x0003_0002                 // OpenGL ES 3.2
    static let platforms = ["arm64-v8a", "armeabi-v7a", "armeabi"]
    static let simOperator = "310260"
    static let cellOperator = "310260"

    static var locale: String {
        let id = Locale.current.identifier.replacingOccurrences(of: "-", with: "_")
        return id.contains("_") ? id : "en_US"
    }
    /// The two-letter region, from the API each OS version has (this file also builds into tools/playdl on the Mac).
    static var regionCode: String? {
        if #available(iOS 16, macOS 13, *) { return Locale.current.region?.identifier }
        return Locale.current.regionCode
    }
    static var country: String { (PlayDevice.regionCode ?? "US").lowercased() }

    static let sharedLibraries = [
        "android.ext.shared", "android.hidl.base-V1.0-java", "android.hidl.manager-V1.0-java", "android.net.ipsec.ike",
        "android.test.base", "android.test.mock", "android.test.runner", "com.android.future.usb.accessory",
        "com.android.location.provider", "com.android.media.remotedisplay", "com.android.mediadrm.signer", "com.android.nfc_extras",
        "com.google.android.dialer.support", "com.google.android.gms", "com.google.android.maps", "javax.obex",
        "org.apache.http.legacy",
    ]

    static let features = [
        "android.hardware.audio.low_latency", "android.hardware.audio.output", "android.hardware.audio.pro",
        "android.hardware.bluetooth", "android.hardware.bluetooth_le", "android.hardware.camera", "android.hardware.camera.any",
        "android.hardware.camera.autofocus", "android.hardware.camera.capability.manual_post_processing",
        "android.hardware.camera.capability.manual_sensor", "android.hardware.camera.capability.raw", "android.hardware.camera.flash",
        "android.hardware.camera.front", "android.hardware.camera.level.full", "android.hardware.faketouch",
        "android.hardware.fingerprint", "android.hardware.location", "android.hardware.location.gps", "android.hardware.location.network",
        "android.hardware.microphone", "android.hardware.nfc", "android.hardware.nfc.hce", "android.hardware.opengles.aep",
        "android.hardware.ram.normal", "android.hardware.screen.landscape", "android.hardware.screen.portrait",
        "android.hardware.sensor.accelerometer", "android.hardware.sensor.barometer", "android.hardware.sensor.compass",
        "android.hardware.sensor.gyroscope", "android.hardware.sensor.light", "android.hardware.sensor.proximity",
        "android.hardware.sensor.stepcounter", "android.hardware.sensor.stepdetector", "android.hardware.telephony",
        "android.hardware.telephony.gsm", "android.hardware.touchscreen", "android.hardware.touchscreen.multitouch",
        "android.hardware.touchscreen.multitouch.distinct", "android.hardware.touchscreen.multitouch.jazzhand",
        "android.hardware.usb.accessory", "android.hardware.usb.host", "android.hardware.vulkan.compute",
        "android.hardware.vulkan.level", "android.hardware.vulkan.version", "android.hardware.wifi", "android.hardware.wifi.direct",
        "android.software.activities_on_secondary_displays", "android.software.app_widgets", "android.software.autofill",
        "android.software.backup", "android.software.companion_device_setup", "android.software.cts",
        "android.software.device_admin", "android.software.home_screen", "android.software.input_methods",
        "android.software.live_wallpaper", "android.software.managed_users", "android.software.midi",
        "android.software.picture_in_picture", "android.software.print", "android.software.securely_removes_users",
        "android.software.verified_boot", "android.software.voice_recognizers", "android.software.webview",
        "com.google.android.feature.GOOGLE_BUILD", "com.google.android.feature.GOOGLE_EXPERIENCE",
        "com.google.android.feature.PIXEL_EXPERIENCE",
    ]

    static let glExtensions = [
        "GL_ARM_rgba8", "GL_ARM_shader_framebuffer_fetch", "GL_ARM_shader_framebuffer_fetch_depth_stencil",
        "GL_EXT_YUV_target", "GL_EXT_blend_minmax", "GL_EXT_buffer_storage", "GL_EXT_color_buffer_float",
        "GL_EXT_color_buffer_half_float", "GL_EXT_copy_image", "GL_EXT_debug_marker", "GL_EXT_discard_framebuffer",
        "GL_EXT_disjoint_timer_query", "GL_EXT_draw_buffers_indexed", "GL_EXT_geometry_shader", "GL_EXT_gpu_shader5",
        "GL_EXT_multisampled_render_to_texture", "GL_EXT_occlusion_query_boolean", "GL_EXT_primitive_bounding_box",
        "GL_EXT_read_format_bgra", "GL_EXT_robustness", "GL_EXT_sRGB", "GL_EXT_sRGB_write_control",
        "GL_EXT_shader_framebuffer_fetch", "GL_EXT_shader_io_blocks", "GL_EXT_shader_pixel_local_storage",
        "GL_EXT_tessellation_shader", "GL_EXT_texture_border_clamp", "GL_EXT_texture_buffer", "GL_EXT_texture_cube_map_array",
        "GL_EXT_texture_filter_anisotropic", "GL_EXT_texture_format_BGRA8888", "GL_EXT_texture_rg", "GL_EXT_texture_sRGB_decode",
        "GL_EXT_texture_storage", "GL_EXT_texture_type_2_10_10_10_REV", "GL_KHR_blend_equation_advanced", "GL_KHR_debug",
        "GL_KHR_robustness", "GL_KHR_texture_compression_astc_hdr", "GL_KHR_texture_compression_astc_ldr",
        "GL_OES_EGL_image", "GL_OES_EGL_image_external", "GL_OES_EGL_image_external_essl3", "GL_OES_EGL_sync",
        "GL_OES_compressed_ETC1_RGB8_texture", "GL_OES_depth24", "GL_OES_depth_texture", "GL_OES_element_index_uint",
        "GL_OES_fbo_render_mipmap", "GL_OES_packed_depth_stencil", "GL_OES_rgb8_rgba8", "GL_OES_sample_shading",
        "GL_OES_sample_variables", "GL_OES_shader_image_atomic", "GL_OES_shader_multisample_interpolation",
        "GL_OES_standard_derivatives", "GL_OES_surfaceless_context", "GL_OES_texture_3D", "GL_OES_texture_float",
        "GL_OES_texture_float_linear", "GL_OES_texture_half_float", "GL_OES_texture_half_float_linear", "GL_OES_texture_npot",
        "GL_OES_texture_stencil8", "GL_OES_texture_storage_multisample_2d_array", "GL_OES_vertex_array_object",
    ]

    /// DeviceConfigurationProto: what the screen, the GPU and the system offer.
    static func configuration(_ w: inout ProtoWriter) {
        w.int(1, 3)                                     // touchScreen: finger
        w.int(2, 1)                                     // keyboard: none
        w.int(3, 1)                                     // navigation: none
        w.int(4, 2)                                     // screenLayout: normal
        w.bool(5, false)
        w.bool(6, false)
        w.int(7, screenDensity)
        w.int(8, glEsVersion)
        for s in sharedLibraries { w.string(9, s) }
        for s in features { w.string(10, s) }
        for s in platforms { w.string(11, s) }
        w.int(12, screenWidth)
        w.int(13, screenHeight)
        w.string(14, locale)
        for s in glExtensions { w.string(15, s) }
        w.int(16, 0)
        w.int(18, screenWidth * 160 / screenDensity)    // smallest width in dp
        w.int(20, 8 << 30)                              // 8 GB of memory
        w.int(21, 9)
        for s in features { w.message(26) { $0.string(1, s); $0.int(2, 0) } }
    }

    /// AndroidCheckinRequest: the first contact, which hands back this device's id (the GSF id).
    static func checkinRequest() -> Data {
        var w = ProtoWriter()
        w.int(2, 0)                                     // id: none yet
        w.message(4) { c in                             // checkin
            c.message(1) { b in                         // build
                b.string(1, fingerprint)
                b.string(2, hardware)
                b.string(3, brand)
                b.string(4, radio)
                b.string(5, bootloader)
                b.string(6, "android-google")
                b.int(7, Int64(Date().timeIntervalSince1970))
                b.int(8, playServicesVersion)
                b.string(9, device)
                b.int(10, sdk)
                b.string(11, model)
                b.string(12, manufacturer)
                b.string(13, product)
                b.bool(14, false)
            }
            c.int(2, 0)
            c.string(6, cellOperator)
            c.string(7, simOperator)
            c.string(8, "mobile-notroaming")
            c.int(9, 0)
        }
        w.string(6, locale)
        w.string(12, TimeZone.current.identifier)
        w.int(14, 3)                                    // version
        w.message(18, configuration)
        w.int(20, 0)                                    // fragment
        return w.data
    }

    /// "Android-Finsky/…": who the Play Store says it is.
    static var finskyUserAgent: String {
        "Android-Finsky/\(vendingVersionString) (api=3,versionCode=\(vendingVersion),sdk=\(sdk),device=\(device),hardware=\(hardware),"
            + "product=\(product),platformVersionRelease=\(release),model=\(model.replacingOccurrences(of: " ", with: "%20")),"
            + "buildId=\(buildID),isWideScreen=0,supportedAbis=\(platforms.joined(separator: ";")))"
    }

    static var authUserAgent: String { "GoogleAuth/1.4 (\(device) \(buildID))" }
}
