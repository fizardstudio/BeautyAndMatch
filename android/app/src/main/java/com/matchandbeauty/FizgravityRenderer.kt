package com.matchandbeauty

object FizgravityRenderer {
    init { System.loadLibrary("match_and_beauty_core") }
    @JvmStatic external fun nativeInitGL()
    @JvmStatic external fun nativeResize(width: Int, height: Int)
    @JvmStatic external fun nativeDrawFrame(textureId: Int, landmarks: FloatArray?, matrix: FloatArray?)
}
