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
        map["onMorphologyDetected"] = mutableMapOf("registrationName" to "onMorphologyDetected")
        return map
    }

    override fun getCommandsMap(): Map<String, Int>? {
        return mapOf("takeSnapshot" to 1, "requestScan" to 2)
    }

    override fun receiveCommand(root: FizgravityARView, commandId: String, args: com.facebook.react.bridge.ReadableArray?) {
        super.receiveCommand(root, commandId, args)
        if (commandId == "1" || commandId == "takeSnapshot") {
            root.pendingSnapshot = true
            root.requestRender()
        } else if (commandId == "2" || commandId == "requestScan") {
            root.requestMorphologyScan()
        }
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

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupHighlight")
    fun setMakeupHighlight(view: FizgravityARView, colorArray: com.facebook.react.bridge.ReadableArray?) {
        if (colorArray != null && colorArray.size() == 4) {
            FizgravityRenderer.nativeSetMakeup(5, colorArray.getDouble(0).toFloat(), colorArray.getDouble(1).toFloat(), colorArray.getDouble(2).toFloat(), colorArray.getDouble(3).toFloat())
        }
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupConcealer")
    fun setMakeupConcealer(view: FizgravityARView, colorArray: com.facebook.react.bridge.ReadableArray?) {
        if (colorArray != null && colorArray.size() == 4) {
            FizgravityRenderer.nativeSetMakeup(6, colorArray.getDouble(0).toFloat(), colorArray.getDouble(1).toFloat(), colorArray.getDouble(2).toFloat(), colorArray.getDouble(3).toFloat())
        }
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupContourStyle")
    fun setMakeupContourStyle(view: FizgravityARView, style: Int) {
        FizgravityRenderer.nativeSetMakeupStyle(4, style)
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupBlushStyle")
    fun setMakeupBlushStyle(view: FizgravityARView, style: Int) {
        FizgravityRenderer.nativeSetMakeupStyle(1, style)
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupFoundationType")
    fun setMakeupFoundationType(view: FizgravityARView, style: Int) {
        FizgravityRenderer.nativeSetMakeupStyle(2, style)
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupFoundationBlur", defaultFloat = 8.0f)
    fun setMakeupFoundationBlur(view: FizgravityARView, radius: Float) {
        FizgravityRenderer.nativeSetFoundationBlur(radius)
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "makeupConcealerStyle")
    fun setMakeupConcealerStyle(view: FizgravityARView, style: Int) {
        FizgravityRenderer.nativeSetConcealerStyle(style)
    }

    @com.facebook.react.uimanager.annotations.ReactProp(name = "scanTrigger")
    fun setScanTrigger(view: FizgravityARView, trigger: Int) {
        if (trigger > 0) {
            view.requestMorphologyScan()
        }
    }
}
