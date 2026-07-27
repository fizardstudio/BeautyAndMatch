import React, { useEffect, useRef, useState, useMemo } from 'react';
import { 
  StyleSheet, View, Text, TouchableOpacity, PermissionsAndroid, 
  Platform, ActivityIndicator, Dimensions, ScrollView, Animated, Easing, PanResponder,
  UIManager, findNodeHandle, DeviceEventEmitter
} from 'react-native';
import { requireNativeComponent } from 'react-native';
import { useMakeupStore } from './src/store/makeupStore';

// --- Native AR View ---
const FizgravityARView = requireNativeComponent<any>('FizgravityARView');

// --- Helper Functions ---
function hexToRGBA(hex: string, alpha: number): [number, number, number, number] {
  if (hex === '#00000000') return [0, 0, 0, 0];
  const r = parseInt(hex.slice(1, 3), 16) / 255.0;
  const g = parseInt(hex.slice(3, 5), 16) / 255.0;
  const b = parseInt(hex.slice(5, 7), 16) / 255.0;
  return [r, g, b, alpha];
}

// --- UI Components ---

// Glass Slider for premium look
const GlassSlider = ({ label, min, max, value, onChange }: { label: string, min: number, max: number, value: number, onChange: (v: number) => void }) => {
  const [trackWidth, setTrackWidth] = useState(0);

  const panResponder = useMemo(() => PanResponder.create({
    onStartShouldSetPanResponder: () => true,
    onMoveShouldSetPanResponder: () => true,
    onPanResponderTerminationRequest: () => false,
    onPanResponderGrant: (evt) => {
      if (trackWidth > 0) {
        let rawVal = (evt.nativeEvent.locationX / trackWidth) * (max - min) + min;
        onChange(Math.max(min, Math.min(max, rawVal)));
      }
    },
    onPanResponderMove: (evt) => {
      if (trackWidth > 0) {
        let rawVal = (evt.nativeEvent.locationX / trackWidth) * (max - min) + min;
        onChange(Math.max(min, Math.min(max, rawVal)));
      }
    }
  }), [min, max, onChange, trackWidth]);

  return (
    <View style={styles.sliderContainer}>
      <View style={styles.sliderHeader}>
        <Text style={styles.sliderLabel}>{label}</Text>
        <Text style={styles.sliderValue}>{value.toFixed(2)}</Text>
      </View>
      <View style={styles.sliderTrackBg} onLayout={(e) => setTrackWidth(e.nativeEvent.layout.width)}>
        <View style={[styles.sliderTrackFill, { width: trackWidth > 0 ? `${((value - min) / (max - min)) * 100}%` : 0 }]} />
        <View style={[styles.sliderThumb, { left: trackWidth > 0 ? `${((value - min) / (max - min)) * 100}%` : 0 }]} pointerEvents="none" />
        <View 
          {...panResponder.panHandlers}
          style={{ position: 'absolute', top: -15, bottom: -15, left: -10, right: -10 }}
        />
      </View>
    </View>
  );
};

// Circular Color Swatch
const ColorSwatch = ({ color, selected, onPress }: { color: string, selected: boolean, onPress: () => void }) => (
  <TouchableOpacity
    style={[
      styles.swatch,
      { backgroundColor: color === '#00000000' ? 'transparent' : color },
      selected && styles.swatchSelected,
      color === '#00000000' && styles.swatchClear
    ]}
    onPress={onPress}
  >
    {color === '#00000000' && <Text style={{color: '#999', fontSize: 10}}>Clear</Text>}
  </TouchableOpacity>
);

const ColorPicker = ({ label, colors, selectedColor, onSelect }: { label: string, colors: string[], selectedColor: string, onSelect: (color: string) => void }) => (
  <View style={styles.pickerContainer}>
    <Text style={styles.pickerLabel}>{label}</Text>
    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
      {colors.map(c => (
        <ColorSwatch key={c} color={c} selected={selectedColor === c} onPress={() => onSelect(c)} />
      ))}
    </ScrollView>
  </View>
);

// Pill Button for Styles
const StylePill = ({ title, selected, onPress }: { title: string, selected: boolean, onPress: () => void }) => (
  <TouchableOpacity style={[styles.pill, selected && styles.pillSelected]} onPress={onPress}>
    <Text style={[styles.pillText, selected && styles.pillTextSelected]}>{title}</Text>
  </TouchableOpacity>
);

const TabButton = ({ title, active, onPress }: { title: string, active: boolean, onPress: () => void }) => (
  <TouchableOpacity style={[styles.tabButton, active && styles.activeTabButton]} onPress={onPress}>
    <Text style={[styles.tabText, active && styles.activeTabText]}>{title}</Text>
  </TouchableOpacity>
);

const App = () => {
  const arViewRef = useRef<any>(null);
  const [hasPermission, setHasPermission] = useState(false);
  const [showMesh, setShowMesh] = useState(true);
  const [activeTab, setActiveTab] = useState('complexion');
  const [showDiagnosticsPanel, setShowDiagnosticsPanel] = useState(false);
  const [isScanning, setIsScanning] = useState(false);
  const [scanTriggerId, setScanTriggerId] = useState(0);
  
  // Animation for AI Scan
  const scanAnim = useRef(new Animated.Value(0)).current;

  const state = useMakeupStore();
  const { 
    foundationColor, foundationOpacity, foundationBlur, foundationType,
    concealerColor, concealerOpacity, concealerStyle,
    lipstickColor, lipstickOpacity, blushColor, blushOpacity, blushStyle,
    contourColor, contourIntensity, contourStyle,
    eyeshadowColor, eyeshadowOpacity, faceShape,
    setFoundation, setConcealer, setBlush, setContour, setLipstick, setDiagnostics
  } = state;

  useEffect(() => {
    (async () => {
      if (Platform.OS === 'android') {
        const result = await PermissionsAndroid.request(PermissionsAndroid.PERMISSIONS.CAMERA);
        setHasPermission(result === PermissionsAndroid.RESULTS.GRANTED);
      } else {
        setHasPermission(true);
      }
    })();
  }, []);

  // Listen for morphology result from FizgravityARView
  useEffect(() => {
    const sub = DeviceEventEmitter.addListener('MorphologyResult', (data: any) => {
      useMakeupStore.getState().setDiagnostics({
        faceShape: data.faceShape,
        eyeShape: data.eyeShape,
        noseShape: data.noseShape,
        jawWidth: data.jawWidth,
        faceLength: data.faceLength,
        canthalTilt: data.canthalTilt,
        eyeAspectRatio: data.eyeAspectRatio,
        alarBaseWidth: data.alarBaseWidth,
        intercanthalDistance: data.intercanthalDistance,
      });
      scanAnim.stopAnimation();
      setIsScanning(false);
      setShowDiagnosticsPanel(true);
      setActiveTab('diagnostics');
    });
    return () => sub.remove();
  }, []);

  const startAIScan = () => {
    setIsScanning(true);
    // Use prop trigger to bypass Fabric command dispatch issues
    setScanTriggerId(Date.now());

    Animated.loop(
      Animated.sequence([
        Animated.timing(scanAnim, { toValue: 1, duration: 1500, easing: Easing.inOut(Easing.ease), useNativeDriver: true }),
        Animated.timing(scanAnim, { toValue: 0, duration: 1500, easing: Easing.inOut(Easing.ease), useNativeDriver: true })
      ])
    ).start();
    // Hard timeout: force stop scanning after 8s
    setTimeout(() => {
      scanAnim.stopAnimation();
      setIsScanning(false);
      setShowDiagnosticsPanel(true);
      setActiveTab('diagnostics');
    }, 8000);
  };

  if (!hasPermission) {
    return <View style={styles.center}><Text>Camera permission needed.</Text></View>;
  }

  const scanLineStyle = {
    transform: [{
      translateY: scanAnim.interpolate({ inputRange: [0, 1], outputRange: [0, Dimensions.get('window').height] })
    }],
    opacity: isScanning ? 1 : 0
  };

  // Convert Concealer Style
  const concealerStyleInt = 
    concealerStyle === 'facelift' ? 1 :
    concealerStyle === 'corrector_green' ? 2 :
    concealerStyle === 'corrector_peach' ? 3 : 0;

  return (
    <View style={styles.container}>
      <FizgravityARView
        ref={arViewRef}
        style={StyleSheet.absoluteFill}
        showMesh={showMesh}
        scanTrigger={scanTriggerId}
        makeupFoundation={hexToRGBA(foundationColor, foundationOpacity)}
        makeupLipstick={hexToRGBA(lipstickColor, lipstickOpacity)}
        makeupBlush={hexToRGBA(blushColor, blushOpacity)}
        makeupConcealer={hexToRGBA(concealerColor, concealerOpacity)}
        makeupEyeshadow={hexToRGBA(eyeshadowColor, eyeshadowOpacity)}
        makeupContour={hexToRGBA(contourColor, contourIntensity)}
        makeupHighlight={hexToRGBA('#FFF5E6', contourIntensity > 0 ? 0.35 : 0)}
        makeupContourStyle={contourStyle === 'normal' ? 0 : contourStyle === 'slim' ? 1 : contourStyle === 'pinch' ? 2 : 3}
        makeupBlushStyle={blushStyle === 'normal' ? 0 : blushStyle === 'contour_45' ? 1 : 2}
        makeupFoundationType={foundationType === 'dewy' ? 1 : foundationType === 'sheer' ? 2 : foundationType === 'satin' ? 3 : foundationType === 'luminous' ? 4 : 0}
        makeupFoundationBlur={foundationBlur}
        makeupConcealerStyle={concealerStyleInt}
      />

      <Animated.View style={[styles.scanLine, scanLineStyle]} />

      <View style={styles.uiOverlay} pointerEvents="box-none">
        {/* Top Header */}
        <View style={styles.header}>
          <Text style={styles.logoText}>M & B</Text>
          <TouchableOpacity style={styles.meshToggle} onPress={() => setShowMesh(!showMesh)}>
            <Text style={styles.meshToggleText}>{showMesh ? 'Hide Mesh' : 'Show Mesh'}</Text>
          </TouchableOpacity>
        </View>

        {/* Floating Scanner Button */}
        {!isScanning && (
          <TouchableOpacity style={styles.aiButton} onPress={startAIScan}>
            <Text style={styles.aiButtonText}>✨ 1-CLICK AI BEST LOOK</Text>
          </TouchableOpacity>
        )}

        {isScanning && (
          <View style={styles.aiScanningBadge}>
            <ActivityIndicator size="small" color="#E5A9A9" />
            <Text style={styles.aiScanningText}>Analyzing Face...</Text>
          </View>
        )}

        {/* Floating Bottom Panel */}
        <View style={styles.bottomPanel}>
          <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.tabsScroll}>
            <TabButton title="Complexion" active={activeTab === 'complexion'} onPress={() => setActiveTab('complexion')} />
            <TabButton title="Concealer" active={activeTab === 'concealer'} onPress={() => setActiveTab('concealer')} />
            <TabButton title="Contour" active={activeTab === 'contour'} onPress={() => setActiveTab('contour')} />
            <TabButton title="Blush" active={activeTab === 'blush'} onPress={() => setActiveTab('blush')} />
            <TabButton title="Diagnostics" active={activeTab === 'diagnostics'} onPress={() => setActiveTab('diagnostics')} />
          </ScrollView>

          <View style={styles.panelContent}>
            {activeTab === 'complexion' && (
              <ScrollView showsVerticalScrollIndicator={false}>
                <ColorPicker
                  label="Foundation Shade"
                  colors={['#00000000', '#F6C3A2', '#EBB48F', '#F0C7AC', '#DF9B72', '#C68257', '#00FFFF']}
                  selectedColor={foundationColor}
                  onSelect={(c) => {
                    setFoundation({ foundationColor: c });
                    if (c !== '#00000000' && foundationOpacity === 0) setFoundation({ foundationOpacity: 0.5 });
                  }}
                />
                <GlassSlider label="Application Thickness" min={0} max={1} value={foundationOpacity} onChange={v => setFoundation({ foundationOpacity: v })} />
                <GlassSlider label="Skin Smoothing" min={0} max={20} value={foundationBlur} onChange={v => setFoundation({ foundationBlur: v })} />
                
                <Text style={styles.pickerLabel}>Finish Type</Text>
                <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                  <StylePill title="Matte" selected={foundationType === 'matte'} onPress={() => setFoundation({ foundationType: 'matte' })} />
                  <StylePill title="Dewy" selected={foundationType === 'dewy'} onPress={() => setFoundation({ foundationType: 'dewy' })} />
                  <StylePill title="Sheer" selected={foundationType === 'sheer'} onPress={() => setFoundation({ foundationType: 'sheer' })} />
                  <StylePill title="Satin" selected={foundationType === 'satin'} onPress={() => setFoundation({ foundationType: 'satin' })} />
                  <StylePill title="Luminous" selected={foundationType === 'luminous'} onPress={() => setFoundation({ foundationType: 'luminous' })} />
                </ScrollView>
              </ScrollView>
            )}

            {activeTab === 'concealer' && (
              <ScrollView showsVerticalScrollIndicator={false}>
                <Text style={styles.pickerLabel}>Concealer Style</Text>
                <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                  <StylePill title="Traditional" selected={concealerStyle === 'traditional'} onPress={() => setConcealer({ concealerStyle: 'traditional' })} />
                  <StylePill title="Facelift Hack" selected={concealerStyle === 'facelift'} onPress={() => setConcealer({ concealerStyle: 'facelift' })} />
                  <StylePill title="Green Corrector" selected={concealerStyle === 'corrector_green'} onPress={() => setConcealer({ concealerStyle: 'corrector_green', concealerColor: '#78B58D', concealerOpacity: 0.6 })} />
                  <StylePill title="Peach Corrector" selected={concealerStyle === 'corrector_peach'} onPress={() => setConcealer({ concealerStyle: 'corrector_peach', concealerColor: '#FFB085', concealerOpacity: 0.6 })} />
                </ScrollView>

                {/* Hide Color Picker if Corrector is selected */}
                {(concealerStyle === 'traditional' || concealerStyle === 'facelift') && (
                  <ColorPicker
                    label="Concealer Shade"
                    colors={['#00000000', '#F2C9A8', '#E8B894', '#DDAA82', '#C89570', '#B98262']}
                    selectedColor={concealerColor}
                    onSelect={(c) => {
                      setConcealer({ concealerColor: c });
                      if (c !== '#00000000' && concealerOpacity === 0) setConcealer({ concealerOpacity: 0.6 });
                    }}
                  />
                )}
                
                <GlassSlider label="Concealer Coverage" min={0} max={1} value={concealerOpacity} onChange={v => setConcealer({ concealerOpacity: v })} />
              </ScrollView>
            )}

            {activeTab === 'contour' && (
              <ScrollView showsVerticalScrollIndicator={false}>
                <ColorPicker
                  label="Contour Shade"
                  colors={['#00000000', '#A36B46', '#8C5A3C', '#6E432A']}
                  selectedColor={contourColor}
                  onSelect={(c) => {
                    setContour({ contourColor: c });
                    if (c !== '#00000000' && contourIntensity === 0) setContour({ contourIntensity: 0.5 });
                  }}
                />
                <GlassSlider label="Contour Intensity" min={0} max={1} value={contourIntensity} onChange={v => setContour({ contourIntensity: v })} />
                
                <Text style={styles.pickerLabel}>Contour Style</Text>
                <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                  <StylePill title="Normal" selected={contourStyle === 'normal'} onPress={() => setContour({ contourStyle: 'normal' })} />
                  <StylePill title="Slimming" selected={contourStyle === 'slim'} onPress={() => setContour({ contourStyle: 'slim' })} />
                  <StylePill title="Pinch" selected={contourStyle === 'pinch'} onPress={() => setContour({ contourStyle: 'pinch' })} />
                  <StylePill title="Straighten" selected={contourStyle === 'straight'} onPress={() => setContour({ contourStyle: 'straight' })} />
                </ScrollView>
              </ScrollView>
            )}

            {activeTab === 'blush' && (
              <ScrollView showsVerticalScrollIndicator={false}>
                <ColorPicker
                  label="Blush Shade"
                  colors={['#00000000', '#FF8C9D', '#FF6B8B', '#E55A7B', '#F2856D']}
                  selectedColor={blushColor}
                  onSelect={(c) => {
                    setBlush({ blushColor: c });
                    if (c !== '#00000000' && blushOpacity === 0) setBlush({ blushOpacity: 0.5 });
                  }}
                />
                <GlassSlider label="Blush Intensity" min={0} max={1} value={blushOpacity} onChange={v => setBlush({ blushOpacity: v })} />
                
                <Text style={styles.pickerLabel}>Blush Style</Text>
                <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                  <StylePill title="Apple" selected={blushStyle === 'normal'} onPress={() => setBlush({ blushStyle: 'normal' })} />
                  <StylePill title="Draped (Lifting)" selected={blushStyle === 'contour_45'} onPress={() => setBlush({ blushStyle: 'contour_45' })} />
                  <StylePill title="Sun-kissed" selected={blushStyle === 'horizontal'} onPress={() => setBlush({ blushStyle: 'horizontal' })} />
                </ScrollView>
              </ScrollView>
            )}

            {activeTab === 'diagnostics' && (
              <ScrollView showsVerticalScrollIndicator={false} contentContainerStyle={{ paddingBottom: 20 }}>
                <Text style={styles.diagTitle}>Face Anatomy Report</Text>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Face Shape:</Text>
                  <Text style={styles.diagValue}>{state.faceShape}</Text>
                </View>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Eye Shape:</Text>
                  <Text style={styles.diagValue}>{state.eyeShape}</Text>
                </View>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Nose Shape:</Text>
                  <Text style={styles.diagValue}>{state.noseShape}</Text>
                </View>

                <Text style={[styles.diagTitle, { marginTop: 15 }]}>Biometrics</Text>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Jaw Width:</Text>
                  <Text style={styles.diagValue}>{state.jawWidth?.toFixed(3)}</Text>
                </View>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Face Length:</Text>
                  <Text style={styles.diagValue}>{state.faceLength?.toFixed(3)}</Text>
                </View>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Canthal Tilt:</Text>
                  <Text style={styles.diagValue}>{state.canthalTilt?.toFixed(2)}°</Text>
                </View>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Eye Aspect Ratio:</Text>
                  <Text style={styles.diagValue}>{state.eyeAspectRatio?.toFixed(3)}</Text>
                </View>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Alar Base Width:</Text>
                  <Text style={styles.diagValue}>{state.alarBaseWidth?.toFixed(3)}</Text>
                </View>
                <View style={styles.diagRow}>
                  <Text style={styles.diagLabel}>Intercanthal Dist:</Text>
                  <Text style={styles.diagValue}>{state.intercanthalDistance?.toFixed(3)}</Text>
                </View>
              </ScrollView>
            )}

          </View>
        </View>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#000' },
  center: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  uiOverlay: { flex: 1, justifyContent: 'space-between', paddingBottom: 30 },
  
  // Premium Header
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingTop: 50,
    paddingHorizontal: 24,
  },
  logoText: { color: '#FFF', fontSize: 22, fontWeight: '200', letterSpacing: 4 },
  meshToggle: {
    backgroundColor: 'rgba(255,255,255,0.15)',
    paddingHorizontal: 12, paddingVertical: 6, borderRadius: 20,
  },
  meshToggleText: { color: '#FFF', fontSize: 12 },

  // AI Button
  aiButton: {
    alignSelf: 'center',
    backgroundColor: 'rgba(255, 255, 255, 0.25)',
    paddingHorizontal: 24, paddingVertical: 14,
    borderRadius: 30,
    borderWidth: 1, borderColor: 'rgba(255,255,255,0.4)',
    marginTop: 'auto', marginBottom: 20,
  },
  aiButtonText: { color: '#FFF', fontWeight: '600', letterSpacing: 1 },
  aiScanningBadge: {
    alignSelf: 'center', flexDirection: 'row', alignItems: 'center',
    backgroundColor: 'rgba(0,0,0,0.5)', padding: 12, borderRadius: 20,
    marginTop: 'auto', marginBottom: 20,
  },
  aiScanningText: { color: '#E5A9A9', marginLeft: 8 },

  // Glassmorphism Bottom Panel
  bottomPanel: {
    backgroundColor: 'rgba(30, 25, 30, 0.65)',
    borderTopLeftRadius: 30, borderTopRightRadius: 30,
    paddingTop: 16, paddingBottom: 24,
    borderTopWidth: 1, borderColor: 'rgba(255,255,255,0.1)',
    minHeight: 320,
  },
  tabsScroll: { paddingHorizontal: 20, gap: 12, marginBottom: 16 },
  tabButton: { paddingVertical: 8, paddingHorizontal: 4 },
  activeTabButton: { borderBottomWidth: 2, borderColor: '#E5A9A9' },
  tabText: { color: 'rgba(255,255,255,0.5)', fontSize: 16, fontWeight: '500' },
  activeTabText: { color: '#E5A9A9', fontWeight: 'bold' },
  panelContent: { paddingHorizontal: 24 },

  // Premium Sliders
  sliderContainer: { marginBottom: 20 },
  sliderHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 8 },
  sliderLabel: { color: '#E5A9A9', fontSize: 13, letterSpacing: 1 },
  sliderValue: { color: 'rgba(255,255,255,0.7)', fontSize: 13 },
  sliderTrackBg: { height: 4, backgroundColor: 'rgba(255,255,255,0.1)', borderRadius: 2, position: 'relative' },
  sliderTrackFill: { height: '100%', backgroundColor: '#E5A9A9', borderRadius: 2 },
  sliderThumb: { 
    position: 'absolute', width: 16, height: 16, borderRadius: 8, 
    backgroundColor: '#FFF', top: -6, marginLeft: -8,
    shadowColor: '#000', shadowOffset: { width: 0, height: 2 }, shadowOpacity: 0.5, shadowRadius: 4
  },

  // Premium Swatches
  pickerContainer: { marginBottom: 20 },
  pickerLabel: { color: '#E5A9A9', fontSize: 13, letterSpacing: 1, marginBottom: 12 },
  swatchScroll: { gap: 12, marginBottom: 20 },
  swatch: { width: 40, height: 40, borderRadius: 20, borderWidth: 2, borderColor: 'transparent' },
  swatchSelected: { borderColor: '#FFF', transform: [{ scale: 1.1 }] },
  swatchClear: { borderWidth: 1, borderColor: 'rgba(255,255,255,0.2)', justifyContent: 'center', alignItems: 'center' },

  // Pills
  pill: { paddingHorizontal: 16, paddingVertical: 8, borderRadius: 20, backgroundColor: 'rgba(255,255,255,0.1)' },
  pillSelected: { backgroundColor: '#E5A9A9' },
  pillText: { color: '#FFF', fontSize: 13 },
  pillTextSelected: { color: '#1A1A1A', fontWeight: 'bold' },

  // Diagnostics
  diagTitle: { color: '#E5A9A9', fontSize: 16, fontWeight: 'bold', marginBottom: 12 },
  diagText: { color: '#FFF', fontSize: 14, marginBottom: 4 },
  diagRow: { flexDirection: 'row', justifyContent: 'space-between', paddingVertical: 8, borderBottomWidth: 1, borderColor: 'rgba(255,255,255,0.1)' },
  diagLabel: { color: 'rgba(255,255,255,0.7)', fontSize: 14 },
  diagValue: { color: '#FFF', fontSize: 14, fontWeight: '500' },

  scanLine: {
    position: 'absolute', top: 0, left: 0, right: 0, height: 2,
    backgroundColor: '#E5A9A9',
    shadowColor: '#E5A9A9', shadowOffset: { width: 0, height: 0 }, shadowOpacity: 1, shadowRadius: 10
  }
});
export default App;
