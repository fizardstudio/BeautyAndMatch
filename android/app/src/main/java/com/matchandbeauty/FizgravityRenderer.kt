package com.matchandbeauty

import java.nio.ByteBuffer

object FizgravityRenderer {
    init { System.loadLibrary("match_and_beauty_core") }
    @JvmStatic external fun nativeInitGL()
    @JvmStatic external fun nativeResize(width: Int, height: Int)
    @JvmStatic external fun nativeDrawSyncFrame(
        textureId: Int,
        imageBuffer: ByteBuffer,
        width: Int,
        height: Int,
        rowStride: Int,
        landmarks: FloatArray?,
        hasNewImage: Boolean
    )
    @JvmStatic external fun nativeSetMakeup(regionType: Int, r: Float, g: Float, b: Float, a: Float)
    @JvmStatic external fun nativeSetMakeupStyle(regionType: Int, style: Int)
    @JvmStatic external fun nativeSetFoundationBlur(radius: Float)
    @JvmStatic external fun nativeSetLipstickGlossiness(value: Float)
    @JvmStatic external fun nativeSetAmbientLighting(cctKelvin: Float, intensity: Float)
    @JvmStatic external fun nativeSetShowMakeup(value: Float)
    @JvmStatic external fun nativeLoadGlitterTexture(buffer: ByteBuffer, width: Int, height: Int)
}
