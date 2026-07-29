import React, { useEffect, useRef, useState, useMemo } from 'react';
import {
  StyleSheet, View, Text, TouchableOpacity, PermissionsAndroid,
  Platform, ActivityIndicator, Dimensions, ScrollView, Animated, Easing,
  PanResponder, DeviceEventEmitter, BackHandler
} from 'react-native';
import { requireNativeComponent } from 'react-native';
import { useMakeupStore } from '../store/makeupStore';
import { THEME } from '../theme';

const FizgravityARView = requireNativeComponent<any>('FizgravityARView');

interface TryOnScreenProps {
  onBack?: () => void;
}

function hexToRGBA(hex: string, alpha: number): [number, number, number, number] {
  if (hex === '#00000000') return [0, 0, 0, 0];
  const r = parseInt(hex.slice(1, 3), 16) / 255.0;
  const g = parseInt(hex.slice(3, 5), 16) / 255.0;
  const b = parseInt(hex.slice(5, 7), 16) / 255.0;
  return [r, g, b, alpha];
}

const foundationTypes = ['matte', 'dewy', 'sheer', 'satin', 'luminous'] as const;
const lipstickFinishes = ['matte', 'satin', 'glossy', 'sheer', 'shimmer'] as const;

// Draggable before/after divider. Renders FIRST inside uiOverlay (before topBar/bottom
// dock) so its full-width drag layer sits BELOW them in touch z-order — later siblings
// intercept their own taps normally, while the uncovered middle area falls through to
// this drag layer. value/onChange are the same 0..1 fraction the native shader uses to
// position the split (0 = divider at left edge/all raw, 1 = divider at right edge/all
// makeup); left of the line shows makeup, right shows raw camera.
const CompareDivider = ({ value, onChange }: any) => {
  const screenWidth = Dimensions.get('window').width;
  const dividerX = value * screenWidth;

  const panResponder = useMemo(() => PanResponder.create({
    onStartShouldSetPanResponder: () => true,
    onMoveShouldSetPanResponder: () => true,
    onPanResponderMove: (_evt, gestureState) => {
      const clamped = Math.max(0, Math.min(1, gestureState.moveX / screenWidth));
      onChange(clamped);
    },
  }), [screenWidth, onChange]);

  return (
    <View style={StyleSheet.absoluteFill} pointerEvents="box-none">
      <View style={StyleSheet.absoluteFill} {...panResponder.panHandlers} />
      <View style={[styles.compareLabelChip, { left: 16 }]} pointerEvents="none">
        <Text style={styles.compareLabelText}>MAKEUP</Text>
      </View>
      <View style={[styles.compareLabelChip, { right: 16 }]} pointerEvents="none">
        <Text style={styles.compareLabelText}>ASLI</Text>
      </View>
      <View style={[styles.compareDividerLine, { left: dividerX }]} pointerEvents="none" />
      <View style={[styles.compareDividerHandle, { left: dividerX - 20 }]} pointerEvents="none">
        <Text style={styles.compareDividerHandleIcon}>⇄</Text>
      </View>
    </View>
  );
};

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

const TryOnScreen = ({ onBack }: TryOnScreenProps) => {
  const arViewRef = useRef<any>(null);
  const [hasPermission, setHasPermission] = useState(false);
  const [showMesh, setShowMesh] = useState(true);
  const [activeCategory, setActiveCategory] = useState<'foundation' | 'concealer' | 'contour' | 'blush' | 'lipstick' | null>(null);
  const [isScanning, setIsScanning] = useState(false);
  const [scanTriggerId, setScanTriggerId] = useState(0);
  const [showDiagnosticsOverlay, setShowDiagnosticsOverlay] = useState(false);
  const [compareMode, setCompareMode] = useState(false);
  const [showMakeupValue, setShowMakeupValue] = useState(0.5);

  const scanAnim = useRef(new Animated.Value(0)).current;

  const state = useMakeupStore();
  const {
    foundationColor, foundationOpacity, foundationBlur, foundationType,
    concealerColor, concealerOpacity, concealerStyle,
    lipstickColor, lipstickOpacity, lipstickFinish, lipstickGlossiness, blushColor, blushOpacity, blushStyle,
    contourColor, contourIntensity, contourStyle,
    eyeshadowColor, eyeshadowOpacity,
    setFoundation, setConcealer, setBlush, setContour, setLipstick,
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
    if (!onBack) return;
    const sub = BackHandler.addEventListener('hardwareBackPress', () => {
      onBack();
      return true;
    });
    return () => sub.remove();
  }, [onBack]);

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
      setShowDiagnosticsOverlay(true);
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
      setShowDiagnosticsOverlay(true);
    }, 8000);
  };

  if (!hasPermission) {
    return <View style={styles.center}><ActivityIndicator color={THEME.roseQuartz} size="large" /></View>;
  }

  const scanLineStyle = {
    transform: [{ translateY: scanAnim.interpolate({ inputRange: [0, 1], outputRange: [0, Dimensions.get('window').height] }) }],
    opacity: isScanning ? 1 : 0
  };

  const concealerStyleInt =
    concealerStyle === 'facelift' ? 1 :
      concealerStyle === 'corrector_green' ? 2 :
        concealerStyle === 'corrector_peach' ? 3 : 0;

  const toggleCategory = (cat: 'foundation' | 'concealer' | 'contour' | 'blush' | 'lipstick') => {
    setActiveCategory(activeCategory === cat ? null : cat);
  };

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
        makeupLipstickFinish={lipstickFinish === 'satin' ? 1 : lipstickFinish === 'glossy' ? 2 : lipstickFinish === 'sheer' ? 3 : lipstickFinish === 'shimmer' ? 4 : 0}
        makeupLipstickGlossiness={lipstickGlossiness}
        showMakeup={compareMode ? showMakeupValue : 1.0}
      />

      <Animated.View style={[styles.scanLine, scanLineStyle]} />

      <View style={styles.uiOverlay} pointerEvents="box-none">
        {/* BEFORE/AFTER DRAGGABLE DIVIDER — rendered first so topBar/bottom dock (later
            siblings below) sit on top of it in touch z-order and stay tappable. */}
        {compareMode && <CompareDivider value={showMakeupValue} onChange={setShowMakeupValue} />}

        {/* TOP BAR */}
        <View style={styles.topBar}>
          <TouchableOpacity style={styles.iconChip} onPress={onBack} disabled={!onBack}>
            <Text style={styles.chipIcon}>‹</Text>
          </TouchableOpacity>

          <Text style={styles.wordmark}>M & B</Text>

          <View style={styles.topRightIcons}>
            <TouchableOpacity style={styles.iconChip} onPress={() => setCompareMode(!compareMode)}>
              <Text style={[styles.chipIcon, compareMode && { color: THEME.roseQuartz }]}>⇄</Text>
            </TouchableOpacity>

            <TouchableOpacity style={styles.iconChip} onPress={() => setShowMesh(!showMesh)}>
              <Text style={[styles.chipIcon, !showMesh && styles.chipIconInactive]}>{showMesh ? '✨' : '🕸️'}</Text>
            </TouchableOpacity>

            <TouchableOpacity style={styles.iconChip} onPress={startAIScan} disabled={isScanning}>
              {isScanning ? (
                <ActivityIndicator size="small" color={THEME.glowGold} />
              ) : (
                <Text style={[styles.chipIcon, { color: THEME.glowGold }]}>◈</Text>
              )}
            </TouchableOpacity>
          </View>
        </View>

        <View style={{ flex: 1 }} />

        {/* DIAGNOSTICS OVERLAY */}
        {showDiagnosticsOverlay && (
          <View style={styles.overlayBackdrop} pointerEvents="box-none">
            <TouchableOpacity
              style={styles.overlayBackdropTouch}
              activeOpacity={1}
              onPress={() => setShowDiagnosticsOverlay(false)}
              pointerEvents="box-none"
            />
            <View style={styles.diagnosticsCard} pointerEvents="box-none">
              <TouchableOpacity
                style={styles.diagnosticsClose}
                onPress={() => setShowDiagnosticsOverlay(false)}
                hitSlop={{ top: 8, bottom: 8, left: 8, right: 8 }}
              >
                <Text style={styles.closeIcon}>×</Text>
              </TouchableOpacity>

              <Text style={styles.diagTitle}>ANATOMY REPORT</Text>
              <View style={styles.diagGrid}>
                <View style={styles.diagCard}><Text style={styles.diagLabel}>Face</Text><Text style={styles.diagValue}>{state.faceShape || '-'}</Text></View>
                <View style={styles.diagCard}><Text style={styles.diagLabel}>Eyes</Text><Text style={styles.diagValue}>{state.eyeShape || '-'}</Text></View>
                <View style={styles.diagCard}><Text style={styles.diagLabel}>Nose</Text><Text style={styles.diagValue}>{state.noseShape || '-'}</Text></View>

                <View style={styles.diagCard}><Text style={styles.diagLabel}>Jaw</Text><Text style={styles.diagValue}>{state.jawWidth ? state.jawWidth.toFixed(2) : '-'}</Text></View>
                <View style={styles.diagCard}><Text style={styles.diagLabel}>Length</Text><Text style={styles.diagValue}>{state.faceLength ? state.faceLength.toFixed(2) : '-'}</Text></View>
                <View style={styles.diagCard}><Text style={styles.diagLabel}>Tilt</Text><Text style={styles.diagValue}>{state.canthalTilt ? `${state.canthalTilt.toFixed(1)}°` : '-'}</Text></View>

                <View style={styles.diagCard}><Text style={styles.diagLabel}>Eye Ratio</Text><Text style={styles.diagValue}>{state.eyeAspectRatio ? state.eyeAspectRatio.toFixed(2) : '-'}</Text></View>
                <View style={styles.diagCard}><Text style={styles.diagLabel}>Alar</Text><Text style={styles.diagValue}>{state.alarBaseWidth ? state.alarBaseWidth.toFixed(2) : '-'}</Text></View>
                <View style={styles.diagCard}><Text style={styles.diagLabel}>Inner Dist</Text><Text style={styles.diagValue}>{state.intercanthalDistance ? state.intercanthalDistance.toFixed(2) : '-'}</Text></View>
              </View>
            </View>
          </View>
        )}

        {/* CATEGORY PANEL */}
        {activeCategory && (
          <View style={styles.categoryPanel} pointerEvents="box-none">
            <View style={styles.categoryPanelContent}>
              <ScrollView showsVerticalScrollIndicator={false} contentContainerStyle={styles.categoryContentScroll}>
                {activeCategory === 'foundation' && (
                  <>
                    <ColorPicker label="FOUNDATION SHADE" colors={['#00000000', '#F6C3A2', '#EBB48F', '#F0C7AC', '#DF9B72', '#C68257', '#00FFFF']} selectedColor={foundationColor} onSelect={(c: string) => { setFoundation({ foundationColor: c }); if (c !== '#00000000' && foundationOpacity === 0) setFoundation({ foundationOpacity: 0.5 }); }} />
                    <GlassSlider label="Thickness" min={0} max={1} value={foundationOpacity} onChange={(v: any) => setFoundation({ foundationOpacity: v })} />
                    <GlassSlider label="Smoothing" min={0} max={20} value={foundationBlur} onChange={(v: any) => setFoundation({ foundationBlur: v })} />

                    <Text style={styles.pickerLabel}>FINISH</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      {foundationTypes.map((type) => (
                        <StylePill key={type} title={type.charAt(0).toUpperCase() + type.slice(1)} selected={foundationType === type} onPress={() => setFoundation({ foundationType: type })} />
                      ))}
                    </ScrollView>
                  </>
                )}

                {activeCategory === 'concealer' && (
                  <>
                    <Text style={styles.pickerLabel}>STYLE</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      <StylePill title="Traditional" selected={concealerStyle === 'traditional'} onPress={() => setConcealer({ concealerStyle: 'traditional' })} />
                      <StylePill title="Facelift" selected={concealerStyle === 'facelift'} onPress={() => setConcealer({ concealerStyle: 'facelift' })} />
                      <StylePill title="Green" selected={concealerStyle === 'corrector_green'} onPress={() => setConcealer({ concealerStyle: 'corrector_green', concealerColor: '#78B58D', concealerOpacity: 0.6 })} />
                      <StylePill title="Peach" selected={concealerStyle === 'corrector_peach'} onPress={() => setConcealer({ concealerStyle: 'corrector_peach', concealerColor: '#FFB085', concealerOpacity: 0.6 })} />
                    </ScrollView>

                    {(concealerStyle === 'traditional' || concealerStyle === 'facelift') && (
                      <ColorPicker label="SHADE" colors={['#00000000', '#F2C9A8', '#E8B894', '#DDAA82', '#C89570', '#B98262']} selectedColor={concealerColor} onSelect={(c: string) => { setConcealer({ concealerColor: c }); if (c !== '#00000000' && concealerOpacity === 0) setConcealer({ concealerOpacity: 0.6 }); }} />
                    )}

                    <GlassSlider label="Coverage" min={0} max={1} value={concealerOpacity} onChange={(v: any) => setConcealer({ concealerOpacity: v })} />
                  </>
                )}

                {activeCategory === 'contour' && (
                  <>
                    <ColorPicker label="SHADE" colors={['#00000000', '#A36B46', '#8C5A3C', '#6E432A']} selectedColor={contourColor} onSelect={(c: string) => { setContour({ contourColor: c }); if (c !== '#00000000' && contourIntensity === 0) setContour({ contourIntensity: 0.5 }); }} />
                    <GlassSlider label="Intensity" min={0} max={1} value={contourIntensity} onChange={(v: any) => setContour({ contourIntensity: v })} />

                    <Text style={styles.pickerLabel}>STYLE</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      <StylePill title="Normal" selected={contourStyle === 'normal'} onPress={() => setContour({ contourStyle: 'normal' })} />
                      <StylePill title="Slim" selected={contourStyle === 'slim'} onPress={() => setContour({ contourStyle: 'slim' })} />
                      <StylePill title="Pinch" selected={contourStyle === 'pinch'} onPress={() => setContour({ contourStyle: 'pinch' })} />
                      <StylePill title="Straight" selected={contourStyle === 'straight'} onPress={() => setContour({ contourStyle: 'straight' })} />
                    </ScrollView>
                  </>
                )}

                {activeCategory === 'blush' && (
                  <>
                    <ColorPicker label="SHADE" colors={['#00000000', '#FF8C9D', '#FF6B8B', '#E55A7B', '#F2856D']} selectedColor={blushColor} onSelect={(c: string) => { setBlush({ blushColor: c }); if (c !== '#00000000' && blushOpacity === 0) setBlush({ blushOpacity: 0.5 }); }} />
                    <GlassSlider label="Intensity" min={0} max={1} value={blushOpacity} onChange={(v: any) => setBlush({ blushOpacity: v })} />

                    <Text style={styles.pickerLabel}>STYLE</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      <StylePill title="Apple" selected={blushStyle === 'normal'} onPress={() => setBlush({ blushStyle: 'normal' })} />
                      <StylePill title="Draped" selected={blushStyle === 'contour_45'} onPress={() => setBlush({ blushStyle: 'contour_45' })} />
                      <StylePill title="Sun-kissed" selected={blushStyle === 'horizontal'} onPress={() => setBlush({ blushStyle: 'horizontal' })} />
                    </ScrollView>
                  </>
                )}

                {activeCategory === 'lipstick' && (
                  <>
                    <ColorPicker label="SHADE" colors={['#00000000', '#C0392B', '#B5344C', '#E8607A', '#8E3B46', '#D97B93', '#A64B2A']} selectedColor={lipstickColor} onSelect={(c: string) => { setLipstick({ lipstickColor: c }); if (c !== '#00000000' && lipstickOpacity === 0) setLipstick({ lipstickOpacity: 1.0 }); }} />
                    <GlassSlider label="Opacity" min={0} max={1} value={lipstickOpacity} onChange={(v: any) => setLipstick({ lipstickOpacity: v })} />
                    <GlassSlider label="Shine" min={0} max={1} value={lipstickGlossiness} onChange={(v: any) => setLipstick({ lipstickGlossiness: v })} />

                    <Text style={styles.pickerLabel}>FINISH</Text>
                    <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.swatchScroll}>
                      {lipstickFinishes.map((finish) => (
                        <StylePill key={finish} title={finish.charAt(0).toUpperCase() + finish.slice(1)} selected={lipstickFinish === finish} onPress={() => setLipstick({ lipstickFinish: finish })} />
                      ))}
                    </ScrollView>
                  </>
                )}
              </ScrollView>
            </View>
          </View>
        )}

        {/* BOTTOM DOCK */}
        <View style={styles.dockContainer} pointerEvents="box-none">
          <View style={styles.dock}>
            <TouchableOpacity style={[styles.dockIcon, activeCategory === 'foundation' && styles.dockIconActive]} onPress={() => toggleCategory('foundation')}>
              <Text style={[styles.dockGlyph, activeCategory === 'foundation' && styles.dockGlyphActive]}>🎨</Text>
            </TouchableOpacity>

            <TouchableOpacity style={[styles.dockIcon, activeCategory === 'concealer' && styles.dockIconActive]} onPress={() => toggleCategory('concealer')}>
              <Text style={[styles.dockGlyph, activeCategory === 'concealer' && styles.dockGlyphActive]}>◐</Text>
            </TouchableOpacity>

            <TouchableOpacity style={[styles.dockIcon, activeCategory === 'contour' && styles.dockIconActive]} onPress={() => toggleCategory('contour')}>
              <Text style={[styles.dockGlyph, activeCategory === 'contour' && styles.dockGlyphActive]}>⌊</Text>
            </TouchableOpacity>

            <TouchableOpacity style={[styles.dockIcon, activeCategory === 'blush' && styles.dockIconActive]} onPress={() => toggleCategory('blush')}>
              <Text style={[styles.dockGlyph, activeCategory === 'blush' && styles.dockGlyphActive]}>●</Text>
            </TouchableOpacity>

            <TouchableOpacity style={[styles.dockIcon, activeCategory === 'lipstick' && styles.dockIconActive]} onPress={() => toggleCategory('lipstick')}>
              <Text style={[styles.dockGlyph, activeCategory === 'lipstick' && styles.dockGlyphActive]}>💋</Text>
            </TouchableOpacity>
          </View>
        </View>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000',
  },
  center: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: THEME.voidPlum,
  },
  uiOverlay: {
    flex: 1,
  },
  scanLine: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    height: 3,
    backgroundColor: '#FFF',
    shadowColor: THEME.roseQuartz,
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 1,
    shadowRadius: 15,
    elevation: 10,
  },

  topBar: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingTop: Platform.OS === 'ios' ? 12 : 8,
    paddingHorizontal: 16,
    paddingBottom: 12,
  },
  wordmark: {
    color: THEME.champagne,
    fontSize: 14,
    fontWeight: '500',
    letterSpacing: 2,
    fontStyle: 'italic',
    fontFamily: Platform.OS === 'ios' ? 'Georgia' : 'serif',
  },
  topRightIcons: {
    flexDirection: 'row',
    gap: 10,
  },
  iconChip: {
    width: 34,
    height: 34,
    borderRadius: 17,
    backgroundColor: 'rgba(20,12,15,0.42)',
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.14)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  compareDividerLine: {
    position: 'absolute',
    top: 0,
    bottom: 0,
    width: 2,
    backgroundColor: 'rgba(255,255,255,0.85)',
    shadowColor: '#000',
    shadowOpacity: 0.5,
    shadowRadius: 4,
  },
  compareDividerHandle: {
    position: 'absolute',
    bottom: 150, // Kept low, clear of the bottom dock but off the face (which usually
                 // sits mid/upper-frame in a selfie) — not vertically centered, which
                 // would land the handle right over the eyes/nose being compared.
    width: 40,
    height: 40,
    borderRadius: 20,
    backgroundColor: 'rgba(20,12,15,0.65)',
    borderWidth: 1.5,
    borderColor: 'rgba(255,255,255,0.85)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  compareDividerHandleIcon: {
    color: '#fff',
    fontSize: 16,
  },
  compareLabelChip: {
    position: 'absolute',
    top: Platform.OS === 'ios' ? 62 : 58,
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 8,
    backgroundColor: 'rgba(20,12,15,0.5)',
  },
  compareLabelText: {
    color: THEME.textLight,
    fontSize: 10,
    fontWeight: '700',
    letterSpacing: 1,
  },
  chipIcon: {
    fontSize: 16,
    color: THEME.taupeMist,
  },
  chipIconInactive: {
    opacity: 0.5,
  },

  overlayBackdrop: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 50,
  },
  overlayBackdropTouch: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
  },
  diagnosticsCard: {
    marginHorizontal: 28,
    marginVertical: 80,
    backgroundColor: THEME.glassBg,
    borderRadius: 20,
    padding: 20,
    borderWidth: 1,
    borderColor: THEME.glassBorder,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 10 },
    shadowOpacity: 0.3,
    shadowRadius: 20,
    elevation: 15,
    zIndex: 51,
  },
  diagnosticsClose: {
    position: 'absolute',
    top: 12,
    right: 12,
    width: 28,
    height: 28,
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 52,
  },
  closeIcon: {
    fontSize: 24,
    color: THEME.textLight,
    fontWeight: '300',
  },
  diagTitle: {
    color: THEME.roseQuartz,
    fontSize: 11,
    fontWeight: 'bold',
    letterSpacing: 1,
    marginBottom: 15,
    textTransform: 'uppercase',
  },
  diagGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
  },
  diagCard: {
    width: '31%',
    backgroundColor: 'rgba(255,255,255,0.04)',
    paddingVertical: 10,
    paddingHorizontal: 4,
    borderRadius: 10,
    borderWidth: 1,
    borderColor: THEME.glassBorder,
    alignItems: 'center',
    marginBottom: 10,
  },
  diagLabel: {
    color: THEME.textMuted,
    fontSize: 8,
    textTransform: 'uppercase',
    letterSpacing: 0.5,
    marginBottom: 4,
    textAlign: 'center',
  },
  diagValue: {
    color: THEME.textLight,
    fontSize: 11,
    fontWeight: 'bold',
    textAlign: 'center',
  },

  categoryPanel: {
    position: 'absolute',
    bottom: 80,
    left: 14,
    right: 14,
    maxHeight: 260,
    backgroundColor: THEME.glassBg,
    borderRadius: 20,
    borderWidth: 1,
    borderColor: THEME.glassBorder,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 10 },
    shadowOpacity: 0.3,
    shadowRadius: 20,
    elevation: 10,
    zIndex: 40,
  },
  categoryPanelContent: {
    flex: 1,
    paddingHorizontal: 16,
    paddingVertical: 14,
  },
  categoryContentScroll: {
    paddingBottom: 10,
  },

  dockContainer: {
    alignItems: 'center',
    paddingBottom: 20,
  },
  dock: {
    flexDirection: 'row',
    gap: 6,
    backgroundColor: 'rgba(20,12,15,0.42)',
    borderRadius: 999,
    paddingHorizontal: 14,
    paddingVertical: 10,
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.14)',
  },
  dockIcon: {
    width: 38,
    height: 38,
    borderRadius: 19,
    justifyContent: 'center',
    alignItems: 'center',
  },
  dockIconActive: {
    backgroundColor: 'rgba(242,167,173,0.2)',
  },
  dockGlyph: {
    fontSize: 18,
    color: THEME.taupeMist,
  },
  dockGlyphActive: {
    color: THEME.roseQuartz,
  },

  sliderContainer: {
    marginBottom: 16,
  },
  sliderHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 8,
  },
  sliderLabel: {
    color: THEME.textLight,
    fontSize: 10,
    fontWeight: '600',
    letterSpacing: 0.8,
    textTransform: 'uppercase',
    opacity: 0.85,
  },
  sliderValue: {
    color: THEME.roseQuartz,
    fontSize: 11,
    fontWeight: 'bold',
  },
  sliderTrackBg: {
    height: 5,
    backgroundColor: 'rgba(255,255,255,0.1)',
    borderRadius: 2.5,
  },
  sliderTrackFill: {
    height: '100%',
    backgroundColor: THEME.roseQuartz,
    borderRadius: 2.5,
    shadowColor: THEME.roseQuartz,
    shadowOpacity: 0.4,
    shadowRadius: 4,
  },
  sliderThumb: {
    position: 'absolute',
    width: 22,
    height: 22,
    borderRadius: 11,
    backgroundColor: 'rgba(255,255,255,0.2)',
    top: -8.5,
    marginLeft: -11,
    justifyContent: 'center',
    alignItems: 'center',
  },
  sliderThumbInner: {
    width: 12,
    height: 12,
    borderRadius: 6,
    backgroundColor: '#FFF',
    shadowColor: THEME.roseQuartz,
    shadowOpacity: 0.8,
    shadowRadius: 6,
    elevation: 4,
  },

  pickerContainer: {
    marginBottom: 16,
  },
  pickerLabel: {
    color: THEME.textLight,
    fontSize: 10,
    fontWeight: '600',
    letterSpacing: 0.8,
    marginBottom: 10,
    opacity: 0.85,
    textTransform: 'uppercase',
  },
  swatchScroll: {
    gap: 10,
    paddingRight: 10,
  },
  swatchWrapper: {
    width: 44,
    height: 44,
    borderRadius: 22,
    justifyContent: 'center',
    alignItems: 'center',
    borderWidth: 1,
    borderColor: 'transparent',
  },
  swatchWrapperSelected: {
    borderColor: THEME.roseQuartz,
    shadowColor: THEME.roseQuartz,
    shadowOpacity: 0.5,
    shadowRadius: 4,
    elevation: 3,
  },
  swatch: {
    width: 32,
    height: 32,
    borderRadius: 16,
    borderWidth: 1,
    borderColor: THEME.glassBorder,
    justifyContent: 'center',
    alignItems: 'center',
  },
  swatchClear: {
    borderStyle: 'dashed',
  },

  pill: {
    paddingHorizontal: 14,
    paddingVertical: 7,
    borderRadius: 18,
    backgroundColor: 'rgba(255,255,255,0.05)',
    borderWidth: 1,
    borderColor: THEME.glassBorder,
  },
  pillSelected: {
    backgroundColor: THEME.roseQuartz,
    borderColor: THEME.roseQuartz,
    shadowColor: THEME.roseQuartz,
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.35,
    shadowRadius: 6,
  },
  pillText: {
    color: THEME.textLight,
    fontSize: 11,
    fontWeight: '400',
  },
  pillTextSelected: {
    color: '#000',
    fontWeight: '600',
  },
});

export default TryOnScreen;
