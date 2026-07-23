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
        isNewLandmarks: Boolean
    )
    @JvmStatic external fun nativeSetMakeup(regionType: Int, r: Float, g: Float, b: Float, a: Float)
}
