import React, { useEffect, useRef, useState, useMemo } from 'react';
import {
  StyleSheet, View, Text, TouchableOpacity, PermissionsAndroid,
  Platform, ActivityIndicator, Dimensions, ScrollView, Animated, Easing,
  PanResponder, DeviceEventEmitter, BackHandler, Modal
} from 'react-native';
import { requireNativeComponent } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { Gesture, GestureDetector, GestureHandlerRootView } from 'react-native-gesture-handler';
import ReanimatedAnimated, { useSharedValue, useAnimatedStyle, runOnJS } from 'react-native-reanimated';
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

function hexToRGBTuple(hex: string): [number, number, number] {
  return [parseInt(hex.slice(1, 3), 16), parseInt(hex.slice(3, 5), 16), parseInt(hex.slice(5, 7), 16)];
}
function rgbTupleToHex([r, g, b]: [number, number, number]): string {
  const clamp = (v: number) => Math.max(0, Math.min(255, Math.round(v)));
  return '#' + [r, g, b].map((v) => clamp(v).toString(16).padStart(2, '0')).join('');
}
// Bilinear interpolation across 4 corner colors of a 2D (undertone x depth) grid —
// u=0..1 warm->cool (horizontal), v=0..1 light->deep (vertical). Gives continuous
// coverage of the whole practical skin-tone color space from one 2D touch point,
// instead of only the colors reachable by mixing 2 adjacent fixed swatches on a line.
function bilinearHex(u: number, v: number, tl: string, tr: string, bl: string, br: string): string {
  const [tlR, tlG, tlB] = hexToRGBTuple(tl);
  const [trR, trG, trB] = hexToRGBTuple(tr);
  const [blR, blG, blB] = hexToRGBTuple(bl);
  const [brR, brG, brB] = hexToRGBTuple(br);
  const topR = tlR + (trR - tlR) * u, topG = tlG + (trG - tlG) * u, topB = tlB + (trB - tlB) * u;
  const botR = blR + (brR - blR) * u, botG = blG + (brG - blG) * u, botB = blB + (brB - blB) * u;
  return rgbTupleToHex([topR + (botR - topR) * v, topG + (botG - topG) * v, topB + (botB - topB) * v]);
}

// Foundation undertone grid — researched, not guessed (see FIZGRAVITY_ROADMAP.md /
// this feature's TAMO research pass). A plain 2-corner warm<->cool bilinear blend
// can NEVER reach an olive undertone: olive is a distinct yellow-green-muted hue,
// not a midpoint on the warm-cool line, so straight RGB lerp between an orange-warm
// and a pink-cool corner converges toward a duller pink/peach, never toward olive.
// This is why the pad previously couldn't reach some real skin tones (incl. common
// SE Asian/Indonesian golden-olive undertones) even at its most extreme corners.
// Fix (per Fenty Pro Filt'r's & MAC's NC/NW undertone taxonomy, both of which treat
// cool/neutral/warm/olive as 4 distinct buckets, not 2 opposite poles): 4 undertone
// COLUMNS instead of 2, each still bilinearly blended against a light/deep row —
// olive sits its own column, off the warm-cool line entirely, not "past warm".
const FOUNDATION_UNDERTONE_COLUMNS = ['cool', 'neutral', 'warm', 'olive'] as const;
const FOUNDATION_PAD_GRID: Record<'light' | 'deep', Record<typeof FOUNDATION_UNDERTONE_COLUMNS[number], string>> = {
  light: { cool: '#F2C6C6', neutral: '#F0C8A8', warm: '#F6C3A2', olive: '#E3D8A8' },
  deep: { cool: '#6B4040', neutral: '#8C624C', warm: '#8B5A2B', olive: '#7C501A' },
};
// u=0..1 spans all 4 columns (cool->neutral->warm->olive) as 3 chained bilinear
// segments — NOT one blend across the whole range — so olive is reached via its own
// real reference color, not by extrapolating past warm. v=0..1 is light->deep, same
// as before.
function foundationGridColor(u: number, v: number): string {
  const clamped = Math.max(0, Math.min(1, u)) * (FOUNDATION_UNDERTONE_COLUMNS.length - 1);
  const segment = Math.min(Math.floor(clamped), FOUNDATION_UNDERTONE_COLUMNS.length - 2);
  const localU = clamped - segment;
  const colA = FOUNDATION_UNDERTONE_COLUMNS[segment];
  const colB = FOUNDATION_UNDERTONE_COLUMNS[segment + 1];
  return bilinearHex(localU, v, FOUNDATION_PAD_GRID.light[colA], FOUNDATION_PAD_GRID.light[colB], FOUNDATION_PAD_GRID.deep[colA], FOUNDATION_PAD_GRID.deep[colB]);
}

// Grid density for the pad's blended background — see UndertoneDepthPad. Plain
// RN Views, not a shader: this screen already runs a raw OpenGL ES pipeline for the
// AR camera (FizgravityRenderer.cpp), and Skia's own GL-based Canvas rendering
// (tried first) conflicted with it — logcat showed "EGLConsumer is not attached to
// an OpenGL ES context" on every frame, so the pad never actually painted. A fine
// grid of flat-colored Views has no GL/EGL involvement at all, so it can't conflict,
// and at this density (18x8 = 144 cells) individual cell edges aren't perceptible.
const FOUNDATION_GRID_COLS = 18;
const FOUNDATION_GRID_ROWS = 8;
// Computed once at module load — this never varies, it's derived purely from the
// static FOUNDATION_PAD_GRID constant, not from anything per-instance.
const FOUNDATION_PAD_GRID_CELLS: string[][] = Array.from({ length: FOUNDATION_GRID_ROWS }, (_, row) =>
  Array.from({ length: FOUNDATION_GRID_COLS }, (_, col) =>
    foundationGridColor(col / (FOUNDATION_GRID_COLS - 1), row / (FOUNDATION_GRID_ROWS - 1))
  )
);
// Isolated into its own memoized component, with NO props tied to drag state, so
// React skips reconciling these 144 small Views entirely on every touch-move frame
// instead of re-diffing them 60+ times/sec — this was the dominant remaining source
// of drag jank even after throttling the live-AR-preview commit (see
// UndertoneDepthPad's onPanResponderMove): only the local thumb position needs to
// update per frame, not this static background.
const FoundationPadGridBackground = React.memo(() => (
  <View style={StyleSheet.absoluteFill} pointerEvents="none">
    {FOUNDATION_PAD_GRID_CELLS.map((row, r) => (
      <View key={r} style={styles.padGridRow}>
        {row.map((color, c) => <View key={c} style={[styles.padGridCell, { backgroundColor: color }]} />)}
      </View>
    ))}
  </View>
));

const UndertoneDepthPad = ({ label, u, v, onChange }: any) => {
  // Tracked separately, NOT a single "padSize" — the pad area has a fixed aspectRatio
  // (width != height), so reusing one dimension for both axes would misplace the
  // thumb vertically (v near 1 would land past the pad's actual bottom edge).
  const [padWidth, setPadWidth] = useState(0);
  const [padHeight, setPadHeight] = useState(0);

  // Thumb position lives on the UI thread (Reanimated shared values), driven
  // directly by the gesture worklet — NOT React state. Dragging a plain useState
  // through PanResponder meant every touch-move ran on the JS thread (state update
  // -> re-render -> reconciliation), and that thread is shared with MediaPipe/the
  // AR bridge; when it's busy, drag tracking visibly lags. Shared values bypass the
  // JS thread entirely for the actual finger-following motion.
  const thumbX = useSharedValue(0);
  const thumbY = useSharedValue(0);
  const padWidthShared = useSharedValue(0);
  const padHeightShared = useSharedValue(0);

  // JS-side drag position — only needed for the hex readout text and for
  // committing to the global store + native AR view (onChange), both throttled.
  const [dragU, setDragU] = useState(u);
  const [dragV, setDragV] = useState(v);
  // Throttle gate lives on the UI thread too (shared value), so the decision to
  // even CALL runOnJS happens inside the worklet, BEFORE crossing the bridge — not
  // after. Throttling only inside the JS-side callback (as a first pass at this did)
  // still dispatches runOnJS on every single onUpdate frame (up to 100+/sec); if the
  // JS thread falls behind for a moment, those calls queue up and keep draining
  // afterward, replaying stale positions even after the finger has already lifted —
  // which is exactly the "hex keeps moving / picker jumps back" symptom. Gating the
  // dispatch itself means we never queue more than the JS thread can keep up with.
  const lastCommitTime = useSharedValue(0);
  // 16ms (~60/sec) — tight enough that the "Warna saat ini" swatch (a plain RN
  // View, refreshes on the JS/UI thread's own cadence) reads as instant, not
  // trailing behind the thumb. The live AR face preview is a separate story: its
  // camera pipeline only renders ~17fps (MediaPipe alone runs ~26ms/frame — see
  // FizgravityPerf logs), so pushing color updates faster than that ceiling can't
  // make the FACE catch up any quicker; that lag is bounded by the render loop
  // itself, not by this throttle. Still gated before runOnJS, not after — see above.
  const COMMIT_INTERVAL_MS = 16;

  const handlePadLayout = (e: any) => {
    const { width, height } = e.nativeEvent.layout;
    setPadWidth(width);
    setPadHeight(height);
    padWidthShared.value = width;
    padHeightShared.value = height;
    thumbX.value = u * width;
    thumbY.value = v * height;
  };

  const commitToJS = (nu: number, nv: number) => {
    setDragU(nu);
    setDragV(nv);
    onChange(nu, nv);
  };

  // event.x/y from gesture-handler are computed by its own native touch geometry,
  // relative to the GestureDetector's attached view — unlike RN core's
  // locationX/Y (which resolves to whichever view is "under the finger" via touch
  // responder bubbling, and could momentarily resolve wrong near the pad's edges,
  // snapping the position toward the middle), so no extra page-offset math needed.
  const panGesture = Gesture.Pan()
    .onUpdate((event) => {
      const w = padWidthShared.value;
      const h = padHeightShared.value;
      if (w <= 0 || h <= 0) return;
      const clampedX = Math.max(0, Math.min(w, event.x));
      const clampedY = Math.max(0, Math.min(h, event.y));
      thumbX.value = clampedX;
      thumbY.value = clampedY;
      const now = Date.now();
      if (now - lastCommitTime.value > COMMIT_INTERVAL_MS) {
        lastCommitTime.value = now;
        runOnJS(commitToJS)(clampedX / w, clampedY / h);
      }
    })
    .onEnd((event) => {
      const w = padWidthShared.value;
      const h = padHeightShared.value;
      if (w <= 0 || h <= 0) return;
      const clampedX = Math.max(0, Math.min(w, event.x));
      const clampedY = Math.max(0, Math.min(h, event.y));
      thumbX.value = clampedX;
      thumbY.value = clampedY;
      lastCommitTime.value = Date.now();
      runOnJS(commitToJS)(clampedX / w, clampedY / h); // always commit the exact release position
    });

  const thumbAnimatedStyle = useAnimatedStyle(() => ({
    left: thumbX.value - 14,
    top: thumbY.value - 14,
  }));

  const currentColor = foundationGridColor(dragU, dragV);

  return (
    <View style={styles.padContainer}>
      <View style={styles.sliderHeader}>
        <Text style={styles.sliderLabel}>{label}</Text>
        <Text style={styles.sliderValue}>{currentColor.toUpperCase()}</Text>
      </View>
      {/* Column headers name the 4 undertone reference points the pad actually blends
          between (cool/neutral/warm/olive) — replaces per-corner labels now that
          there are 4 columns, not 2, to name. */}
      <View style={styles.padColumnHeaderRow}>
        <Text style={styles.padColumnHeaderText}>Cool</Text>
        <Text style={styles.padColumnHeaderText}>Netral</Text>
        <Text style={styles.padColumnHeaderText}>Warm</Text>
        <Text style={styles.padColumnHeaderText}>Olive</Text>
      </View>
      <GestureDetector gesture={panGesture}>
        <View style={styles.padArea} onLayout={handlePadLayout}>
          <FoundationPadGridBackground />
          <View style={[styles.padCornerGroup, { top: 4, left: 4 }]}>
            <Text style={styles.padAxisText}>Terang</Text>
          </View>
          <View style={[styles.padCornerGroup, { bottom: 4, left: 4 }]}>
            <Text style={styles.padAxisText}>Gelap</Text>
          </View>
          {padWidth > 0 && padHeight > 0 && (
            <ReanimatedAnimated.View
              style={[styles.padThumb, { backgroundColor: currentColor }, thumbAnimatedStyle]}
              pointerEvents="none"
            />
          )}
        </View>
      </GestureDetector>
    </View>
  );
};

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
  // Undertone/depth pad position (0..1 each) — lives here, not in the makeup store,
  // since it's a UI control position that DERIVES foundationColor, not persisted
  // makeup state itself. Defaults near the light-warm corner (top-left), matching
  // where the old fixed-swatch default already sat.
  const [foundationPadU, setFoundationPadU] = useState(0.15);
  const [foundationPadV, setFoundationPadV] = useState(0.2);
  // Whether the 2D mixing pad's own modal is open. Starts closed — the "current
  // color" swatch alone (eventually an AI-scan recommendation) is what shows when
  // Foundation is first opened; the pad lives in a separate Modal (see below), not
  // inline in this panel, specifically so opening it never resizes/repositions the
  // dock panel itself.
  const [foundationPadOpen, setFoundationPadOpen] = useState(false);
  useEffect(() => {
    if (activeCategory !== 'foundation') setFoundationPadOpen(false);
  }, [activeCategory]);
  // Real measured height of the bottom dock (icons row + its own padding), NOT a
  // guessed constant — the category panel's position is derived from this so it can
  // never visually/touch-overlap the dock regardless of device nav-bar insets. 78 is
  // just a same-frame fallback before the first onLayout fires.
  const [dockHeight, setDockHeight] = useState(78);
  const insets = useSafeAreaInsets();
  const screenHeight = Dimensions.get('window').height;
  // Give the panel as much room as the screen can actually spare above the dock,
  // capped so it doesn't dominate the frame on tall screens. Content beyond this
  // still scrolls inside the panel — this just minimizes how often that's needed.
  // Reserve is derived from the REAL measured dockHeight (not a flat guess).
  const TOPBAR_RESERVE = 90;
  const categoryPanelMaxHeight = Math.max(240, Math.min(480, screenHeight - insets.top - insets.bottom - dockHeight - TOPBAR_RESERVE - 12));

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

        {/* CATEGORY PANEL — bottom/maxHeight computed from the dock's real measured
            height + safe-area insets, not a hardcoded guess, so it can never sit on
            top of the dock and block it from being tapped. Stays compact/bottom-
            anchored like the rest of the dock UI — Foundation's 2D pad lives in its
            own Modal (below), NOT inline here, specifically so opening it never
            resizes or repositions this panel. ScrollView is the direct flex child of
            the maxHeight-capped panel (no extra flex:1 wrapper View) — an extra
            wrapper here is a known Android/Yoga case where the inner scroll area
            fails to respect the cap and content renders past the screen edge. */}
        {activeCategory && (
          <View
            style={[styles.categoryPanel, { bottom: dockHeight + 12, maxHeight: categoryPanelMaxHeight }]}
            pointerEvents="box-none"
          >
            <ScrollView
              style={styles.categoryPanelContent}
              showsVerticalScrollIndicator={true}
              contentContainerStyle={styles.categoryContentScroll}
            >
                {activeCategory === 'foundation' && (
                  <>
                    <View style={styles.padOffRow}>
                      <Text style={styles.pickerLabel}>FOUNDATION SHADE</Text>
                      <TouchableOpacity
                        style={[styles.padOffButton, foundationColor === '#00000000' && styles.padOffButtonActive]}
                        onPress={() => setFoundation({ foundationColor: '#00000000', foundationOpacity: 0 })}
                      >
                        <Text style={styles.padOffButtonText}>Tanpa Foundation</Text>
                      </TouchableOpacity>
                    </View>
                    {/* Collapsed by default — shows the CURRENT color only (eventually an
                        AI-scan recommendation), not the pad. Tapping it opens the pad in
                        its own Modal (rendered below, outside this panel) so this panel's
                        own size/position never changes. */}
                    <TouchableOpacity style={styles.currentColorRow} onPress={() => setFoundationPadOpen(true)} activeOpacity={0.8}>
                      <View style={[styles.currentColorSwatch, { backgroundColor: foundationColor === '#00000000' ? 'transparent' : foundationColor }, foundationColor === '#00000000' && styles.swatchClear]}>
                        {foundationColor === '#00000000' && <Text style={{ color: 'rgba(255,255,255,0.4)', fontSize: 14 }}>⊘</Text>}
                      </View>
                      <View style={{ flex: 1 }}>
                        <Text style={styles.currentColorLabel}>Warna saat ini</Text>
                        <Text style={styles.currentColorHex}>{foundationColor === '#00000000' ? 'Belum dipilih' : foundationColor.toUpperCase()}</Text>
                      </View>
                      <Text style={styles.currentColorChevron}>▸</Text>
                    </TouchableOpacity>
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
        )}

        {/* FOUNDATION COLOR-MIX MODAL — a separate bottom-sheet overlay, not inline in
            the category panel above. Opening it must NEVER resize/reposition that
            panel; it's a fully independent layer that slides up from the bottom and
            can freely cover whatever's beneath it (dock included) while open. */}
        <Modal visible={foundationPadOpen} transparent animationType="slide" onRequestClose={() => setFoundationPadOpen(false)}>
          {/* RN's Modal renders into its OWN separate native Android window (a Dialog),
              not the main Activity's view tree — so the GestureHandlerRootView wrapping
              the app root (App.tsx) doesn't extend into it. Without a root wrapper here
              too, the pad's Gesture.Pan() silently never receives touches (no error,
              just nothing happens) because gesture-handler has no root to attach its
              native touch interception to in this window. */}
          <GestureHandlerRootView style={{ flex: 1 }}>
            <View style={styles.padModalBackdrop}>
              <TouchableOpacity style={StyleSheet.absoluteFill} activeOpacity={1} onPress={() => setFoundationPadOpen(false)} />
              <View style={[styles.padModalSheet, { paddingBottom: Math.max(20, insets.bottom + 12) }]}>
                <View style={styles.padModalHandle} />
                <UndertoneDepthPad
                  label="Geser untuk campur warna"
                  u={foundationPadU}
                  v={foundationPadV}
                  onChange={(u: number, v: number) => {
                    setFoundationPadU(u);
                    setFoundationPadV(v);
                    const mixed = foundationGridColor(u, v);
                    setFoundation({ foundationColor: mixed, foundationOpacity: foundationOpacity === 0 ? 0.5 : foundationOpacity });
                  }}
                />
                <TouchableOpacity style={styles.padModalDoneButton} onPress={() => setFoundationPadOpen(false)}>
                  <Text style={styles.padModalDoneText}>Selesai</Text>
                </TouchableOpacity>
              </View>
            </View>
          </GestureHandlerRootView>
        </Modal>

        {/* BOTTOM DOCK — zIndex kept ABOVE the category panel so its buttons stay
            tappable even if a future panel content change makes it taller than
            expected; the panel's own bottom offset (dockHeight-derived, above) is
            the primary defense, this is the backstop. onLayout feeds dockHeight so
            the panel above always positions itself off the dock's real height. */}
        <View
          style={[styles.dockContainer, { paddingBottom: Math.max(20, insets.bottom + 12) }]}
          pointerEvents="box-none"
          onLayout={(e) => setDockHeight(e.nativeEvent.layout.height)}
        >
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
    // bottom & maxHeight are set inline per-render (dockHeight-derived + safe-area
    // aware) — see the JSX. Left as position/left/right/decoration only here.
    position: 'absolute',
    left: 14,
    right: 14,
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
    paddingHorizontal: 16,
    paddingVertical: 14,
  },
  categoryContentScroll: {
    paddingBottom: 10,
  },

  dockContainer: {
    // Kept ABOVE categoryPanel's zIndex (40) so the dock always wins touch
    // resolution even if the panel above it ever overlaps by a pixel or two.
    // paddingBottom is set inline (insets.bottom-aware) — see the JSX.
    alignItems: 'center',
    zIndex: 60,
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
  padOffRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  padOffButton: {
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 12,
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.25)',
  },
  padOffButtonActive: {
    backgroundColor: THEME.roseQuartz,
    borderColor: THEME.roseQuartz,
  },
  padOffButtonText: {
    color: THEME.textLight,
    fontSize: 10,
    fontWeight: '600',
  },
  currentColorRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
    backgroundColor: 'rgba(255,255,255,0.05)',
    borderRadius: 14,
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.12)',
    paddingHorizontal: 12,
    paddingVertical: 10,
    marginBottom: 16,
  },
  currentColorSwatch: {
    width: 40,
    height: 40,
    borderRadius: 20,
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.3)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  currentColorLabel: {
    color: THEME.textLight,
    fontSize: 10,
    fontWeight: '600',
    letterSpacing: 0.5,
    opacity: 0.7,
    textTransform: 'uppercase',
  },
  currentColorHex: {
    color: THEME.roseQuartz,
    fontSize: 13,
    fontWeight: 'bold',
    marginTop: 2,
  },
  currentColorChevron: {
    color: THEME.taupeMist,
    fontSize: 12,
  },
  padModalBackdrop: {
    // No dark scrim — the AR camera preview above the sheet is the whole point of a
    // color-mixing tool (judging the shade against your real live face), so it must
    // stay fully clear while the pad is open, not dimmed. The sheet below is already
    // opaque, which is enough visual separation; tap-outside-to-dismiss still works
    // via the invisible TouchableOpacity covering this area.
    flex: 1,
    justifyContent: 'flex-end',
  },
  padModalSheet: {
    backgroundColor: THEME.voidPlum,
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    paddingHorizontal: 16,
    paddingTop: 10,
    borderWidth: 1,
    borderColor: THEME.glassBorder,
  },
  padModalHandle: {
    alignSelf: 'center',
    width: 40,
    height: 4,
    borderRadius: 2,
    backgroundColor: 'rgba(255,255,255,0.25)',
    marginBottom: 14,
  },
  padModalDoneButton: {
    backgroundColor: THEME.roseQuartz,
    borderRadius: 14,
    paddingVertical: 12,
    alignItems: 'center',
    marginTop: 4,
  },
  padModalDoneText: {
    color: '#000',
    fontSize: 13,
    fontWeight: '700',
  },
  padColumnHeaderRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 4,
  },
  padColumnHeaderText: {
    color: THEME.textLight,
    fontSize: 9,
    fontWeight: '600',
    opacity: 0.6,
    textTransform: 'uppercase',
    letterSpacing: 0.5,
  },
  padGridRow: {
    flex: 1,
    flexDirection: 'row',
  },
  padGridCell: {
    flex: 1,
  },
  padContainer: {
    marginBottom: 16,
  },
  padArea: {
    width: '100%',
    // 2.2, not 1.6 — the pad still needs to be tall enough to comfortably drag on,
    // but every extra dp here is a dp Thickness/Smoothing/Finish don't get without
    // scrolling. Shade accuracy from the 2D drag isn't sensitive to this ratio.
    aspectRatio: 2.2,
    borderRadius: 12,
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.25)',
    overflow: 'hidden',
  },
  padCornerGroup: {
    position: 'absolute',
    alignItems: 'flex-start',
  },
  padThumb: {
    position: 'absolute',
    width: 28,
    height: 28,
    borderRadius: 14,
    borderWidth: 3,
    borderColor: '#FFF',
    shadowColor: '#000',
    shadowOpacity: 0.4,
    shadowRadius: 4,
    elevation: 5,
  },
  padAxisText: {
    color: THEME.textLight,
    fontSize: 8,
    opacity: 0.7,
    letterSpacing: 0.3,
    textShadowColor: 'rgba(0,0,0,0.8)',
    textShadowRadius: 3,
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
