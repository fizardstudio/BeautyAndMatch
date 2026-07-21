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
}
