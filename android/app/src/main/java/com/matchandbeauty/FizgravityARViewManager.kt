package com.matchandbeauty

import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext

class FizgravityARViewManager : SimpleViewManager<FizgravityARView>() {
    override fun getName(): String {
        return "FizgravityARView"
    }

    override fun createViewInstance(reactContext: ThemedReactContext): FizgravityARView {
        return FizgravityARView(reactContext)
    }

    override fun getExportedCustomDirectEventTypeConstants(): MutableMap<String, Any> {
        val map = super.getExportedCustomDirectEventTypeConstants() ?: mutableMapOf()
        map["onFaceDetected"] = mutableMapOf("registrationName" to "onFaceDetected")
        return map
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupLipstick")
    fun setMakeupLipstick(view: FizgravityARView, colorArray: com.facebook.react.bridge.ReadableArray?) {
        if (colorArray != null && colorArray.size() == 4) {
            FizgravityRenderer.nativeSetMakeup(0, colorArray.getDouble(0).toFloat(), colorArray.getDouble(1).toFloat(), colorArray.getDouble(2).toFloat(), colorArray.getDouble(3).toFloat())
        }
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupBlush")
    fun setMakeupBlush(view: FizgravityARView, colorArray: com.facebook.react.bridge.ReadableArray?) {
        if (colorArray != null && colorArray.size() == 4) {
            FizgravityRenderer.nativeSetMakeup(1, colorArray.getDouble(0).toFloat(), colorArray.getDouble(1).toFloat(), colorArray.getDouble(2).toFloat(), colorArray.getDouble(3).toFloat())
        }
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupFoundation")
    fun setMakeupFoundation(view: FizgravityARView, colorArray: com.facebook.react.bridge.ReadableArray?) {
        if (colorArray != null && colorArray.size() == 4) {
            FizgravityRenderer.nativeSetMakeup(2, colorArray.getDouble(0).toFloat(), colorArray.getDouble(1).toFloat(), colorArray.getDouble(2).toFloat(), colorArray.getDouble(3).toFloat())
        }
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupEyeshadow")
    fun setMakeupEyeshadow(view: FizgravityARView, colorArray: com.facebook.react.bridge.ReadableArray?) {
        if (colorArray != null && colorArray.size() == 4) {
            FizgravityRenderer.nativeSetMakeup(3, colorArray.getDouble(0).toFloat(), colorArray.getDouble(1).toFloat(), colorArray.getDouble(2).toFloat(), colorArray.getDouble(3).toFloat())
        }
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupContour")
    fun setMakeupContour(view: FizgravityARView, colorArray: com.facebook.react.bridge.ReadableArray?) {
        if (colorArray != null && colorArray.size() == 4) {
            FizgravityRenderer.nativeSetMakeup(4, colorArray.getDouble(0).toFloat(), colorArray.getDouble(1).toFloat(), colorArray.getDouble(2).toFloat(), colorArray.getDouble(3).toFloat())
        }
    }
}
