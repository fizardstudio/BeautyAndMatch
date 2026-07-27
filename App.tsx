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

// --- TEMA WARNA FEMININ PREMIUM ---
const THEME = {
  primary: '#F2A7AD',
  textLight: '#FFFFFF',
  textMuted: 'rgba(255,255,255,0.6)',
  glassBg: 'rgba(28, 16, 22, 0.45)',
  glassBorder: 'rgba(255,255,255,0.2)',
};

// --- Helper Functions ---
function hexToRGBA(hex: string, alpha: number): [number, number, number, number] {
  if (hex === '#00000000') return [0, 0, 0, 0];
  const r = parseInt(hex.slice(1, 3), 16) / 255.0;
  const g = parseInt(hex.slice(3, 5), 16) / 255.0;
  const b = parseInt(hex.slice(5, 7), 16) / 255.0;
  return [r, g, b, alpha];
}

// --- ARRAY TYPES (Fix untuk TypeScript Error) ---
const foundationTypes = ['matte', 'dewy', 'sheer', 'satin', 'luminous'] as const;

// --- MODERN UI COMPONENTS ---
const GlassSlider = ({ label, min, max, value, onChange }: any) => {
  const [trackWidth, setTrackWidth] = useState(0);

  const panResponder = useMemo(() => PanResponder.create({
    onStartShouldSetPanResponder: () => true,
    onMoveShouldSetPanResponder: () => true,
    onPanResponderMove: (evt) => {
      if (trackWidth > 0) onChange(Math.max(min, Math.min(max, (evt.nativeEvent.locationX / trackWidth) * (max - min) + min)));
    }
  }), [min, max, onChange, trackWidth]);

  return (
    <View style={styles.sliderContainer}>
      <View style={styles.sliderHeader}>
        <Text style={styles.sliderLabel}>{label}</Text>
        <Text style={styles.sliderValue}>{max > 1 ? Math.round(value) : `${Math.round(value * 100)}%`}</Text>
      </View>
      <View style={styles.sliderTrackBg} onLayout={(e) => setTrackWidth(e.nativeEvent.layout.width)}>
        <View style={[styles.sliderTrackFill, { width: trackWidth > 0 ? `${((value - min) / (max - min)) * 100}%` : 0 }]} />
        <View style={[styles.sliderThumb, { left: trackWidth > 0 ? `${((value - min) / (max - min)) * 100}%` : 0 }]} pointerEvents="none">
          <View style={styles.sliderThumbInner} />
        </View>
        <View {...panResponder.panHandlers} style={{ position: 'absolute', top: -20, bottom: -20, left: -10, right: -10 }} />
      </View>
    </View>
  );
};

const ColorSwatch = ({ color, selected, onPress }: any) => (
  <TouchableOpacity style={[styles.swatchWrapper, selected && styles.swatchWrapperSelected]} onPress={onPress}>
    <View style={[styles.swatch, { backgroundColor: color === '#00000000' ? 'transparent' : color }, color === '#00000000' && styles.swatchClear]}>
      {color === '#00000000' && <Text style={{ color: 'rgba(255,255,255,0.4)', fontSize: 16 }}>⊘</Text>}
    </View>
  </TouchableOpacity>
);

const ColorPicker = ({ label, colors, selectedColor, onSelect }: any) => (
  <View style={styles.pickerContainer}>
    <Text style={styles.pickerLabel}>{label}</Text>
    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
      {colors.map((c: string) => (
        <ColorSwatch key={c} color={c} selected={selectedColor === c} onPress={() => onSelect(c)} />
      ))}
    </ScrollView>
  </View>
);

const StylePill = ({ title, selected, onPress }: any) => (
  <TouchableOpacity style={[styles.pill, selected && styles.pillSelected]} onPress={onPress}>
    <Text style={[styles.pillText, selected && styles.pillTextSelected]}>{title}</Text>
  </TouchableOpacity>
);

const TabButton = ({ title, active, onPress }: any) => (
  <TouchableOpacity style={styles.tabButton} onPress={onPress}>
    <Text style={[styles.tabText, active && styles.activeTabText]}>{title}</Text>
    {active && <View style={styles.activeTabDot} />}
  </TouchableOpacity>
);

// --- MAIN APP ---
const App = () => {
  const arViewRef = useRef<any>(null);
  const [hasPermission, setHasPermission] = useState(false);
  const [showMesh, setShowMesh] = useState(true);
  const [activeTab, setActiveTab] = useState('complexion');
  const [showDiagnosticsPanel, setShowDiagnosticsPanel] = useState(false);
  const [isScanning, setIsScanning] = useState(false);
  const [scanTriggerId, setScanTriggerId] = useState(0);

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
  }, [scanAnim]);

  const startAIScan = () => {
    setIsScanning(true);
    setScanTriggerId(Date.now());

    Animated.loop(
      Animated.sequence([
        Animated.timing(scanAnim, { toValue: 1, duration: 1500, easing: Easing.inOut(Easing.ease), useNativeDriver: true }),
        Animated.timing(scanAnim, { toValue: 0, duration: 1500, easing: Easing.inOut(Easing.ease), useNativeDriver: true })
      ])
    ).start();

    setTimeout(() => {
      scanAnim.stopAnimation();
      setIsScanning(false);
      setShowDiagnosticsPanel(true);
      setActiveTab('diagnostics');
    }, 8000);
  };

  if (!hasPermission) {
    return <View style={styles.center}><ActivityIndicator color={THEME.primary} size="large" /></View>;
  }

  const scanLineStyle = {
    transform: [{ translateY: scanAnim.interpolate({ inputRange: [0, 1], outputRange: [0, Dimensions.get('window').height] }) }],
    opacity: isScanning ? 1 : 0
  };

  const concealerStyleInt =
    concealerStyle === 'facelift' ? 1 :
      concealerStyle === 'corrector_green' ? 2 :
        concealerStyle === 'corrector_peach' ? 3 : 0;

  return (
    <View style={styles.container}>
      {/* LAYER 1: AR VIEW */}
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

      {/* LAYER 2: UI OVERLAY */}
      <View style={styles.uiOverlay} pointerEvents="box-none">

        {/* Header */}
        <View style={styles.header}>
          <Text style={styles.logoText}>M <Text style={{ color: THEME.primary }}>&</Text> B</Text>
          <TouchableOpacity onPress={() => setShowMesh(!showMesh)}>
            <View style={styles.meshToggle}>
              <Text style={styles.meshToggleText}>{showMesh ? '✨ Hide Mesh' : '🕸️ Show Mesh'}</Text>
            </View>
          </TouchableOpacity>
        </View>

        {/* BOTTOM AREA */}
        <View style={styles.bottomArea} pointerEvents="box-none">

          {/* Action Button */}
          <View style={styles.floatingActionContainer} pointerEvents="box-none">
            {!isScanning ? (
              <TouchableOpacity style={styles.aiButton} onPress={startAIScan}>
                <View style={[styles.aiButtonGlass, { backgroundColor: 'rgba(0,0,0,0.4)' }]}>
                  <Text style={styles.aiButtonText}>✨ 1-CLICK AI BEST LOOK</Text>
                </View>
              </TouchableOpacity>
            ) : (
              <View style={[styles.aiButtonGlass, { borderRadius: 30 }]}>
                <ActivityIndicator size="small" color={THEME.primary} />
                <Text style={styles.aiScanningText}>Analyzing Face...</Text>
              </View>
            )}
          </View>

          {/* FLOATING GLASS PANEL */}
          <View style={styles.floatingPanelContainer}>
            <View style={styles.glassPanel}>

              <View style={styles.dragHandle} />

              <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.tabsScroll}>
                {['complexion', 'concealer', 'contour', 'blush', 'diagnostics'].map((tab) => (
                  <TabButton key={tab} title={tab.charAt(0).toUpperCase() + tab.slice(1)} active={activeTab === tab} onPress={() => setActiveTab(tab)} />
                ))}
              </ScrollView>

              <View style={styles.panelContent}>

                {activeTab === 'complexion' && (
                  <ScrollView showsVerticalScrollIndicator={false} contentContainerStyle={{ paddingBottom: 20 }}>
                    <ColorPicker label="FOUNDATION SHADE" colors={['#00000000', '#F6C3A2', '#EBB48F', '#F0C7AC', '#DF9B72', '#C68257', '#00FFFF']} selectedColor={foundationColor} onSelect={(c: string) => { setFoundation({ foundationColor: c }); if (c !== '#00000000' && foundationOpacity === 0) setFoundation({ foundationOpacity: 0.5 }); }} />
                    <GlassSlider label="Application Thickness" min={0} max={1} value={foundationOpacity} onChange={(v: any) => setFoundation({ foundationOpacity: v })} />
                    <GlassSlider label="Skin Smoothing" min={0} max={20} value={foundationBlur} onChange={(v: any) => setFoundation({ foundationBlur: v })} />

                    <Text style={styles.pickerLabel}>FINISH TYPE</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      {foundationTypes.map((type) => (
                        <StylePill key={type} title={type.charAt(0).toUpperCase() + type.slice(1)} selected={foundationType === type} onPress={() => setFoundation({ foundationType: type })} />
                      ))}
                    </ScrollView>
                  </ScrollView>
                )}

                {activeTab === 'concealer' && (
                  <ScrollView showsVerticalScrollIndicator={false} contentContainerStyle={{ paddingBottom: 20 }}>
                    <Text style={styles.pickerLabel}>CONCEALER STYLE</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      <StylePill title="Traditional" selected={concealerStyle === 'traditional'} onPress={() => setConcealer({ concealerStyle: 'traditional' })} />
                      <StylePill title="Facelift Hack" selected={concealerStyle === 'facelift'} onPress={() => setConcealer({ concealerStyle: 'facelift' })} />
                      <StylePill title="Green Corrector" selected={concealerStyle === 'corrector_green'} onPress={() => setConcealer({ concealerStyle: 'corrector_green', concealerColor: '#78B58D', concealerOpacity: 0.6 })} />
                      <StylePill title="Peach Corrector" selected={concealerStyle === 'corrector_peach'} onPress={() => setConcealer({ concealerStyle: 'corrector_peach', concealerColor: '#FFB085', concealerOpacity: 0.6 })} />
                    </ScrollView>

                    {(concealerStyle === 'traditional' || concealerStyle === 'facelift') && (
                      <ColorPicker label="CONCEALER SHADE" colors={['#00000000', '#F2C9A8', '#E8B894', '#DDAA82', '#C89570', '#B98262']} selectedColor={concealerColor} onSelect={(c: string) => { setConcealer({ concealerColor: c }); if (c !== '#00000000' && concealerOpacity === 0) setConcealer({ concealerOpacity: 0.6 }); }} />
                    )}

                    <GlassSlider label="Concealer Coverage" min={0} max={1} value={concealerOpacity} onChange={(v: any) => setConcealer({ concealerOpacity: v })} />
                  </ScrollView>
                )}

                {activeTab === 'contour' && (
                  <ScrollView showsVerticalScrollIndicator={false} contentContainerStyle={{ paddingBottom: 20 }}>
                    <ColorPicker label="CONTOUR SHADE" colors={['#00000000', '#A36B46', '#8C5A3C', '#6E432A']} selectedColor={contourColor} onSelect={(c: string) => { setContour({ contourColor: c }); if (c !== '#00000000' && contourIntensity === 0) setContour({ contourIntensity: 0.5 }); }} />
                    <GlassSlider label="Contour Intensity" min={0} max={1} value={contourIntensity} onChange={(v: any) => setContour({ contourIntensity: v })} />

                    <Text style={styles.pickerLabel}>CONTOUR STYLE</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      <StylePill title="Normal" selected={contourStyle === 'normal'} onPress={() => setContour({ contourStyle: 'normal' })} />
                      <StylePill title="Slimming" selected={contourStyle === 'slim'} onPress={() => setContour({ contourStyle: 'slim' })} />
                      <StylePill title="Pinch" selected={contourStyle === 'pinch'} onPress={() => setContour({ contourStyle: 'pinch' })} />

                      <StylePill title="Straight" selected={contourStyle === 'straight'} onPress={() => setContour({ contourStyle: 'straight' })} />
                    </ScrollView>
                  </ScrollView>
                )}

                {activeTab === 'blush' && (
                  <ScrollView showsVerticalScrollIndicator={false} contentContainerStyle={{ paddingBottom: 20 }}>
                    <ColorPicker label="BLUSH SHADE" colors={['#00000000', '#FF8C9D', '#FF6B8B', '#E55A7B', '#F2856D']} selectedColor={blushColor} onSelect={(c: string) => { setBlush({ blushColor: c }); if (c !== '#00000000' && blushOpacity === 0) setBlush({ blushOpacity: 0.5 }); }} />
                    <GlassSlider label="Blush Intensity" min={0} max={1} value={blushOpacity} onChange={(v: any) => setBlush({ blushOpacity: v })} />

                    <Text style={styles.pickerLabel}>BLUSH STYLE</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      <StylePill title="Apple" selected={blushStyle === 'normal'} onPress={() => setBlush({ blushStyle: 'normal' })} />
                      <StylePill title="Draped (Lifting)" selected={blushStyle === 'contour_45'} onPress={() => setBlush({ blushStyle: 'contour_45' })} />
                      <StylePill title="Sun-kissed" selected={blushStyle === 'horizontal'} onPress={() => setBlush({ blushStyle: 'horizontal' })} />
                    </ScrollView>
                  </ScrollView>
                )}

                {activeTab === 'diagnostics' && (
                  <ScrollView showsVerticalScrollIndicator={false} contentContainerStyle={{ paddingBottom: 20 }}>
                    <Text style={styles.diagTitle}>ANATOMY REPORT</Text>
                    <View style={styles.diagGrid}>
                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Face</Text><Text style={styles.diagValue}>{state.faceShape || '-'}</Text></View>
                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Eyes</Text><Text style={styles.diagValue}>{state.eyeShape || '-'}</Text></View>
                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Nose</Text><Text style={styles.diagValue}>{state.noseShape || '-'}</Text></View>

                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Jaw Width</Text><Text style={styles.diagValue}>{state.jawWidth ? state.jawWidth.toFixed(2) : '-'}</Text></View>
                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Face Len</Text><Text style={styles.diagValue}>{state.faceLength ? state.faceLength.toFixed(2) : '-'}</Text></View>
                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Tilt</Text><Text style={styles.diagValue}>{state.canthalTilt ? `${state.canthalTilt.toFixed(1)}°` : '-'}</Text></View>

                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Eye Ratio</Text><Text style={styles.diagValue}>{state.eyeAspectRatio ? state.eyeAspectRatio.toFixed(2) : '-'}</Text></View>
                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Alar Base</Text><Text style={styles.diagValue}>{state.alarBaseWidth ? state.alarBaseWidth.toFixed(2) : '-'}</Text></View>
                      <View style={styles.diagCard}><Text style={styles.diagLabel}>Inner Dist</Text><Text style={styles.diagValue}>{state.intercanthalDistance ? state.intercanthalDistance.toFixed(2) : '-'}</Text></View>
                    </View>
                  </ScrollView>
                )}
              </View>
            </View>
          </View>
        </View>
      </View>
    </View>
  );
};

// --- STYLING ---
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#000' },
  center: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  uiOverlay: { flex: 1, justifyContent: 'space-between' },
  header: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', paddingTop: Platform.OS === 'ios' ? 60 : 40, paddingHorizontal: 24 },
  logoText: { color: THEME.textLight, fontSize: 20, fontWeight: '400', letterSpacing: 4, fontStyle: 'italic' },
  meshToggle: { paddingHorizontal: 16, paddingVertical: 8, borderRadius: 20, overflow: 'hidden', borderWidth: 1, borderColor: THEME.glassBorder },
  meshToggleText: { color: THEME.textLight, fontSize: 11, fontWeight: '600', letterSpacing: 1 },
  scanLine: { position: 'absolute', top: 0, left: 0, right: 0, height: 3, backgroundColor: '#FFF', shadowColor: THEME.primary, shadowOffset: { width: 0, height: 0 }, shadowOpacity: 1, shadowRadius: 15, elevation: 10 },
  bottomArea: { flex: 1, justifyContent: 'flex-end', paddingBottom: Platform.OS === 'ios' ? 36 : 24 },
  floatingActionContainer: { alignItems: 'center', marginBottom: 20 },
  aiButton: { overflow: 'hidden', borderRadius: 30, shadowColor: THEME.primary, shadowOffset: { width: 0, height: 4 }, shadowOpacity: 0.6, shadowRadius: 15, elevation: 8 },
  aiButtonGlass: { flexDirection: 'row', alignItems: 'center', paddingHorizontal: 28, paddingVertical: 14, backgroundColor: 'rgba(242, 167, 173, 0.2)', borderWidth: 1, borderColor: THEME.primary },
  aiButtonText: { color: THEME.textLight, fontWeight: 'bold', letterSpacing: 1.5, fontSize: 12 },
  aiScanningText: { color: THEME.primary, marginLeft: 10, fontWeight: '600', fontSize: 12 },
  floatingPanelContainer: { marginHorizontal: 16, borderRadius: 36, overflow: 'hidden', shadowColor: '#000', shadowOffset: { width: 0, height: 10 }, shadowOpacity: 0.3, shadowRadius: 20, elevation: 10 },
  glassPanel: { backgroundColor: THEME.glassBg, height: 340 },
  dragHandle: { width: 36, height: 4, backgroundColor: 'rgba(255,255,255,0.3)', borderRadius: 2, alignSelf: 'center', marginTop: 16, marginBottom: 12 },
  tabsScroll: { paddingHorizontal: 24, gap: 24, marginBottom: 15, maxHeight: 40 },
  tabButton: { alignItems: 'center', justifyContent: 'center', paddingBottom: 5 },
  tabText: { color: THEME.textMuted, fontSize: 13, fontWeight: '500', letterSpacing: 0.5 },
  activeTabText: { color: THEME.textLight, fontWeight: 'bold' },
  activeTabDot: { position: 'absolute', bottom: -2, width: 5, height: 5, borderRadius: 2.5, backgroundColor: THEME.primary, shadowColor: THEME.primary, shadowOpacity: 1, shadowRadius: 6, elevation: 4 },
  panelContent: { paddingHorizontal: 24, flex: 1 },
  sliderContainer: { marginBottom: 20 },
  sliderHeader: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 10 },
  sliderLabel: { color: THEME.textLight, fontSize: 11, fontWeight: '600', letterSpacing: 1, textTransform: 'uppercase', opacity: 0.9 },
  sliderValue: { color: THEME.primary, fontSize: 12, fontWeight: 'bold' },
  sliderTrackBg: { height: 6, backgroundColor: 'rgba(255,255,255,0.1)', borderRadius: 3 },
  sliderTrackFill: { height: '100%', backgroundColor: THEME.primary, borderRadius: 3, shadowColor: THEME.primary, shadowOpacity: 0.5, shadowRadius: 5 },
  sliderThumb: { position: 'absolute', width: 26, height: 26, borderRadius: 13, backgroundColor: 'rgba(255,255,255,0.2)', top: -10, marginLeft: -13, justifyContent: 'center', alignItems: 'center' },
  sliderThumbInner: { width: 14, height: 14, borderRadius: 7, backgroundColor: '#FFF', shadowColor: THEME.primary, shadowOpacity: 1, shadowRadius: 8, elevation: 5 },
  pickerContainer: { marginBottom: 20 },
  pickerLabel: { color: THEME.textLight, fontSize: 11, fontWeight: '600', letterSpacing: 1, marginBottom: 12, opacity: 0.9 },
  swatchScroll: { gap: 14, paddingRight: 20, paddingBottom: 5 },
  swatchWrapper: { width: 48, height: 48, borderRadius: 24, justifyContent: 'center', alignItems: 'center', borderWidth: 1, borderColor: 'transparent' },
  swatchWrapperSelected: { borderColor: THEME.primary, shadowColor: THEME.primary, shadowOpacity: 0.6, shadowRadius: 6, elevation: 4 },
  swatch: { width: 36, height: 36, borderRadius: 18, borderWidth: 1, borderColor: THEME.glassBorder, justifyContent: 'center', alignItems: 'center' },
  swatchClear: { borderStyle: 'dashed' },
  pill: { paddingHorizontal: 18, paddingVertical: 8, borderRadius: 20, backgroundColor: 'rgba(255,255,255,0.05)', borderWidth: 1, borderColor: THEME.glassBorder },
  pillSelected: { backgroundColor: THEME.primary, borderColor: THEME.primary, shadowColor: THEME.primary, shadowOffset: { width: 0, height: 3 }, shadowOpacity: 0.4, shadowRadius: 8 },
  pillText: { color: THEME.textLight, fontSize: 12, fontWeight: '400' },
  pillTextSelected: { color: '#000', fontWeight: 'bold' },
  diagTitle: { color: THEME.primary, fontSize: 12, fontWeight: 'bold', letterSpacing: 1, marginBottom: 15 },
  diagGrid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
  diagCard: { width: '31%', backgroundColor: 'rgba(255,255,255,0.04)', paddingVertical: 12, paddingHorizontal: 4, borderRadius: 12, borderWidth: 1, borderColor: THEME.glassBorder, alignItems: 'center', marginBottom: 12 },
  diagLabel: { color: THEME.textMuted, fontSize: 9, textTransform: 'uppercase', letterSpacing: 1, marginBottom: 6, textAlign: 'center' },
  diagValue: { color: '#FFF', fontSize: 12, fontWeight: 'bold', textAlign: 'center' },
});

export default App;