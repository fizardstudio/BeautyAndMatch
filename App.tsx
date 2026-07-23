import React, { useEffect, useRef, useState, useMemo } from 'react';
import {
  StyleSheet,
  View,
  Text,
  TouchableOpacity,
  ScrollView,
  Dimensions,
  SafeAreaView,
  StatusBar,
  ActivityIndicator,
  PanResponder,
  Platform,
} from 'react-native';
import { requireNativeComponent } from 'react-native';
const FizgravityARView = requireNativeComponent<any>('FizgravityARView');

import {
  Camera,
  useCameraDevice,
  useCameraPermission,
} from 'react-native-vision-camera';
import Animated, {
  useSharedValue,
  useAnimatedStyle,
  withTiming,
  withSpring,
  withSequence,
  runOnJS,
  useDerivedValue,
} from 'react-native-reanimated';
import { useMakeupStore } from './src/store/makeupStore';
import { detectFaceLandmarks, Landmark, Diagnostics } from './src/native/detectFaceLandmarks';
import { useRunOnJS } from 'react-native-worklets-core';
import {
  foundationShaderCode,
  contourShaderCode,
  lipstickShaderCode,
  eyeShaderCode,
} from './src/shaders/shadersSource';

const { width: screenWidth, height: screenHeight } = Dimensions.get('window');

// Static mappings removed. We will define them dynamically inside AppContent.

const calculateDistance = (a: Landmark, b: Landmark): number => {
  'worklet';
  return Math.sqrt(
    Math.pow(a.x - b.x, 2) +
    Math.pow(a.y - b.y, 2) +
    Math.pow(a.z - b.z, 2)
  );
};

const getLandmark = (landmarks: any, idx: number) => {
  'worklet';
  const offset = idx * 3;
  if (!landmarks || offset + 2 >= landmarks.length) {
    return { x: 0, y: 0, z: 0 };
  }
  return {
    x: landmarks[offset],
    y: landmarks[offset + 1],
    z: landmarks[offset + 2],
  };
};

const hexToRgb = (hex: string): [number, number, number] => {
  'worklet';
  const cleanHex = hex.replace('#', '');
  const r = parseInt(cleanHex.substring(0, 2), 16) / 255;
  const g = parseInt(cleanHex.substring(2, 4), 16) / 255;
  const b = parseInt(cleanHex.substring(4, 6), 16) / 255;
  return [r, g, b];
};

function App() {
  const { hasPermission, requestPermission } = useCameraPermission();

  useEffect(() => {
    if (!hasPermission) {
      requestPermission();
    }
  }, [hasPermission, requestPermission]);

  if (!hasPermission) {
    return (
      <View style={styles.centerContainer}>
        <StatusBar barStyle="light-content" />
        <Text style={styles.errorText}>Camera permission is required to run Match&Beauty.</Text>
        <TouchableOpacity style={styles.accentButton} onPress={requestPermission}>
          <Text style={styles.accentButtonText}>Grant Permission</Text>
        </TouchableOpacity>
      </View>
    );
  }



  return (
    <View style={styles.container}>
      <StatusBar barStyle="light-content" translucent backgroundColor="transparent" />
      <AppContent />
    </View>
  );
}

function AppContent() {
  const {
    foundationColor,
    foundationOpacity,
    foundationBlur,
    blushColor,
    blushOpacity,
    blushStyle,
    contourColor,
    contourIntensity,
    contourStyle,
    lipstickColor,
    lipstickGlossiness,
    lipstickOpacity,
    eyeshadowColor,
    eyeshadowOpacity,
    eyeshadowStyle,
    faceShape,
    eyeShape,
    noseShape,
    jawWidth,
    faceLength,
    canthalTilt,
    eyeAspectRatio,
    alarBaseWidth,
    aiMode,
    cameraActive: storeActive,
    setFoundation,
    setBlush,
    setContour,
    setLipstick,
    setEyeshadow,
    setDiagnostics,
    applyAIBestLook,
    resetMakeup,
    setAIMode,
  } = useMakeupStore();

  const [activeTab, setActiveTab] = useState<'complexion' | 'blush' | 'contour' | 'lips' | 'eyes' | 'diagnostics'>('complexion');
  const [showDiagnosticsPanel, setShowDiagnosticsPanel] = useState(false);
  const [showMesh, setShowMesh] = useState(true);
  const [isScanning, setIsScanning] = useState(false);

  // Shared values for the face tracking mesh & animations (accepts flat Float32Array)
  const scanLineY = useSharedValue(-50);
  const scanOpacity = useSharedValue(0);
  const isProcessing = useSharedValue(false);
  const frameCounter = useSharedValue(0); // Frame counter for throttling

  const hexToRGBA = (hex: string, alpha: number) => {
    if (!hex || hex === '#00000000' || hex === 'transparent') {
      return [0, 0, 0, 0];
    }
    try {
      let r = parseInt(hex.slice(1, 3), 16) / 255;
      let g = parseInt(hex.slice(3, 5), 16) / 255;
      let b = parseInt(hex.slice(5, 7), 16) / 255;
      return [r, g, b, alpha];
    } catch (e) {
      return [0, 0, 0, 0];
    }
  };

  const screenAspect = screenHeight / screenWidth;
  const cameraAspect = 16.0 / 9.0;
  let scaleX = 1.0;
  let scaleY = 1.0;
  
  if (screenAspect > cameraAspect) {
    scaleX = screenAspect / cameraAspect;
  } else {
    scaleY = cameraAspect / screenAspect;
  }

  const mapX = (x: number) => {
    'worklet';
    // Native coordinate system is already mirrored and rotated
    return ((x - 0.5) * scaleX + 0.5) * screenWidth;
  };
  const mapY = (y: number) => {
    'worklet';
    return ((y - 0.5) * scaleY + 0.5) * screenHeight;
  };

  // Diagnostic values ref to hold frame results before applying AI mode
  const latestDiagnostics = useRef<Diagnostics | null>(null);
 
  // AI Diagnostic Scanning sequence
  const startAIScan = () => {
    setIsScanning(true);
    setAIMode('manual');
    scanOpacity.value = 1;
    scanLineY.value = -50;

    // Fast Sweeping Animation (Cyan/Magenta laser line)
    scanLineY.value = withSequence(
      withTiming(screenHeight + 50, { duration: 600 }),
      withTiming(-50, { duration: 500 }),
      withTiming(screenHeight / 2, { duration: 400 })
    );
    
    // Guaranteed JS-thread execution (anti-hang mechanism)
    setTimeout(() => {
      scanOpacity.value = withTiming(0, { duration: 300 });
      finalizeAIScan();
    }, 1500);
  };

  const finalizeAIScan = () => {
    setIsScanning(false);
    console.log('Finalizing scan, latestDiagnostics.current:', latestDiagnostics.current);
    if (latestDiagnostics.current) {
      setDiagnostics(latestDiagnostics.current);
    }
    applyAIBestLook();
    setShowDiagnosticsPanel(true);
  };

  // Animated scanner overlay styles
  const scanLineStyle = useAnimatedStyle(() => {
    return {
      top: scanLineY.value,
      opacity: scanOpacity.value,
    };
  });

  // Diagnostic drawer translate animation
  const drawerY = useSharedValue(600);

  useEffect(() => {
    if (showDiagnosticsPanel) {
      drawerY.value = withSpring(0, { damping: 18, stiffness: 90 });
    } else {
      drawerY.value = withSpring(600, { damping: 18, stiffness: 90 });
    }
  }, [showDiagnosticsPanel, drawerY]);

  // Efficiently sync diagnostics state when user switches to diagnostics tab
  useEffect(() => {
    if (activeTab === 'diagnostics' && latestDiagnostics.current) {
      setDiagnostics(latestDiagnostics.current);
    }
  }, [activeTab]);

  const animatedDrawerStyle = useAnimatedStyle(() => {
    return {
      transform: [{ translateY: drawerY.value }],
    };
  });

  return (
    <View style={styles.container}>
      {/* 1. Hardware Camera Feed (Native OpenGL ES 3.0) */}
      <FizgravityARView
        style={StyleSheet.absoluteFill}
        makeupLipstick={hexToRGBA(lipstickColor, lipstickOpacity * 0.6)}
        makeupBlush={hexToRGBA(blushColor, blushOpacity * 0.5)}
        makeupFoundation={hexToRGBA(foundationColor, foundationOpacity * 0.3)}
        makeupEyeshadow={hexToRGBA(eyeshadowColor, eyeshadowOpacity * 0.5)}
        makeupContour={hexToRGBA(contourColor, contourIntensity * 0.4)}
      />

      {/* 3. AI Scan Laser Line Overlay */}
      <Animated.View style={[styles.scanLine, scanLineStyle]} />

      {/* 4. Glassmorphism UI Panels & Sliders */}
      <SafeAreaView style={styles.overlayContainer} pointerEvents="box-none">
        {/* Top Header Panel */}
        <View style={styles.headerPanel}>
          <View>
            <Text style={styles.logoText}>MATCH&BEAUTY</Text>
            <Text style={styles.subtitleText}>Elite AR Beauty Advisor</Text>
          </View>
          <View style={styles.headerRight}>
            {aiMode === 'ai_auto' && (
              <View style={styles.aiTag}>
                <Text style={styles.aiTagText}>AI OPTIMIZED</Text>
              </View>
            )}
            <TouchableOpacity
              style={[styles.meshToggleButton, showMesh && styles.activeMeshButton]}
              onPress={() => setShowMesh(!showMesh)}
            >
              <Text style={styles.meshToggleText}>{showMesh ? 'Hide Mesh' : 'Show Mesh'}</Text>
            </TouchableOpacity>
          </View>
        </View>

        {/* AI "1-Click" Scanner Button */}
        {!isScanning && (
          <TouchableOpacity style={styles.aiScanButton} onPress={startAIScan}>
            <View style={styles.aiScanInner}>
              <Text style={styles.aiScanTitle}>1-CLICK AI BEST LOOK</Text>
              <Text style={styles.aiScanSubtitle}>Scan Face & Apply Shaders</Text>
            </View>
          </TouchableOpacity>
        )}

        {isScanning && (
          <View style={styles.scanningIndicator}>
            <ActivityIndicator size="small" color="#00FFCC" />
            <Text style={styles.scanningText}>ANALYZING MORPHOLOGY...</Text>
          </View>
        )}

        {/* Bottom Drawer Control Panel */}
        <View style={styles.controlPanel}>
          {/* Scrollable Tabs */}
          <ScrollView
            horizontal
            showsHorizontalScrollIndicator={false}
            contentContainerStyle={styles.tabsScroll}
          >
            <TabButton title="Complexion" active={activeTab === 'complexion'} onPress={() => setActiveTab('complexion')} />
            <TabButton title="Blush" active={activeTab === 'blush'} onPress={() => setActiveTab('blush')} />
            <TabButton title="Contour" active={activeTab === 'contour'} onPress={() => setActiveTab('contour')} />
            <TabButton title="Lips" active={activeTab === 'lips'} onPress={() => setActiveTab('lips')} />
            <TabButton title="Eyes" active={activeTab === 'eyes'} onPress={() => setActiveTab('eyes')} />
            <TabButton title="Diagnostics" active={activeTab === 'diagnostics'} onPress={() => {
              setActiveTab('diagnostics');
              setShowDiagnosticsPanel(true);
            }} />
          </ScrollView>

          {/* Sub-panel Content */}
          <View style={styles.drawersContent}>
            {activeTab === 'complexion' && (
              <ScrollView contentContainerStyle={styles.controlsContent} showsVerticalScrollIndicator={false}>
                <ColorPicker
                  label="Foundation Shade"
                  colors={['#00000000', '#F6C3A2', '#EBB48F', '#F0C7AC', '#DF9B72', '#C68257']}
                  selectedColor={foundationColor}
                  onSelect={(color) => setFoundation({ foundationColor: color })}
                />
                <GlassSlider
                  label="Coverage"
                  min={0}
                  max={1}
                  value={foundationOpacity}
                  onChange={(val) => setFoundation({ foundationOpacity: val })}
                />
                <GlassSlider
                  label="Foundation Smooth (Blur)"
                  min={0}
                  max={20}
                  value={foundationBlur}
                  onChange={(val) => setFoundation({ foundationBlur: val })}
                />
              </ScrollView>
            )}

            {activeTab === 'blush' && (
              <ScrollView>
                <ColorPicker
                    label="Blush Tint"
                    colors={['#00000000', '#E2725B', '#D87093', '#F4C2C2', '#FF8C00', '#C0392B']}
                    selectedColor={blushColor}
                    onSelect={(color) => setBlush({ blushColor: color })}
                  />
                <GlassSlider label="Intensity" min={0} max={1} value={blushOpacity} onChange={(val) => setBlush({ blushOpacity: val })} />
                <View style={styles.styleSelector}>
                  <Text style={styles.selectorLabel}>Application Pattern</Text>
                  <View style={styles.optionsRow}>
                    <OptionButton title="Normal" selected={blushStyle === 'normal'} onPress={() => setBlush({ blushStyle: 'normal' })} />
                    <OptionButton title="Contour 45°" selected={blushStyle === 'contour_45'} onPress={() => setBlush({ blushStyle: 'contour_45' })} />
                    <OptionButton title="Horizontal" selected={blushStyle === 'horizontal'} onPress={() => setBlush({ blushStyle: 'horizontal' })} />
                  </View>
                </View>
              </ScrollView>
            )}

            {activeTab === 'contour' && (
              <ScrollView>
                <ColorPicker
                  label="Contour Shade"
                  colors={['#00000000', '#6B4D3C', '#5C4033', '#4A3525', '#8A6D5E', '#A08070']}
                  selectedColor={contourColor}
                  onSelect={(color) => setContour({ contourColor: color })}
                />
                <GlassSlider
                  label="Contour Intensity"
                  min={0}
                  max={1}
                  value={contourIntensity}
                  onChange={(val) => setContour({ contourIntensity: val })}
                />
                <View style={styles.styleSelector}>
                  <Text style={styles.selectorLabel}>Sculpt Technique</Text>
                  <View style={styles.optionsRow}>
                    <OptionButton title="Normal" selected={contourStyle === 'normal'} onPress={() => setContour({ contourStyle: 'normal' })} />
                    <OptionButton title="Slim Face" selected={contourStyle === 'slim'} onPress={() => setContour({ contourStyle: 'slim' })} />
                    <OptionButton title="Pinch Nose" selected={contourStyle === 'pinch'} onPress={() => setContour({ contourStyle: 'pinch' })} />
                    <OptionButton title="Straighten" selected={contourStyle === 'straight'} onPress={() => setContour({ contourStyle: 'straight' })} />
                  </View>
                </View>
              </ScrollView>
            )}

            {activeTab === 'lips' && (
              <ScrollView contentContainerStyle={styles.controlsContent} showsVerticalScrollIndicator={false}>
                  <ColorPicker
                    label="Lip Tint"
                    colors={['#00000000', '#D35400', '#C0392B', '#E74C3C', '#9B59B6', '#E91E63', '#FF4081']}
                    selectedColor={lipstickColor}
                    onSelect={(color) => setLipstick({ lipstickColor: color })}
                  />
                  <GlassSlider label="Opacity" min={0} max={1} value={lipstickOpacity} onChange={(val) => setLipstick({ lipstickOpacity: val })} />
                  <GlassSlider label="Glossiness" min={0} max={1} value={lipstickGlossiness} onChange={(val) => setLipstick({ lipstickGlossiness: val })} />
              </ScrollView>
            )}

            {activeTab === 'eyes' && (
              <ScrollView>
                <ColorPicker
                    label="Eyeshadow Shade"
                    colors={['#00000000', '#5D4037', '#4A3B32', '#8A3324', '#B08D57', '#C19A6B', '#3E2723']}
                    selectedColor={eyeshadowColor}
                    onSelect={(color) => setEyeshadow({ eyeshadowColor: color })}
                  />
                <GlassSlider
                  label="Eyeshadow Opacity"
                  min={0}
                  max={1}
                  value={eyeshadowOpacity}
                  onChange={(val) => setEyeshadow({ eyeshadowOpacity: val })}
                />
                <View style={styles.styleSelector}>
                  <Text style={styles.selectorLabel}>Eye Accent Style</Text>
                  <View style={styles.optionsRow}>
                    <OptionButton title="Normal" selected={eyeshadowStyle === 'normal'} onPress={() => setEyeshadow({ eyeshadowStyle: 'normal' })} />
                    <OptionButton title="Lifting" selected={eyeshadowStyle === 'lifting'} onPress={() => setEyeshadow({ eyeshadowStyle: 'lifting' })} />
                    <OptionButton title="Gradient" selected={eyeshadowStyle === 'gradient'} onPress={() => setEyeshadow({ eyeshadowStyle: 'gradient' })} />
                    <OptionButton title="Halo" selected={eyeshadowStyle === 'halo'} onPress={() => setEyeshadow({ eyeshadowStyle: 'halo' })} />
                  </View>
                </View>
              </ScrollView>
            )}

            {activeTab === 'diagnostics' && (
              <View style={styles.diagnosticsTabSummary}>
                <Text style={styles.diagnosticsSummaryTitle}>Face Morphology Diagnostics</Text>
                <Text style={styles.diagnosticsSummaryDesc}>
                  Detected: Face: <Text style={styles.summaryValue}>{faceShape}</Text> | Eyes: <Text style={styles.summaryValue}>{eyeShape}</Text> | Nose: <Text style={styles.summaryValue}>{noseShape}</Text>
                </Text>
                <TouchableOpacity
                  style={styles.viewDetailedButton}
                  onPress={() => setShowDiagnosticsPanel(true)}
                >
                  <Text style={styles.viewDetailedText}>View Detailed Metrics Dashboard</Text>
                </TouchableOpacity>
              </View>
            )}
          </View>

          {/* Reset Panel Button */}
          <TouchableOpacity style={styles.resetButton} onPress={resetMakeup}>
            <Text style={styles.resetButtonText}>Reset All Shaders</Text>
          </TouchableOpacity>
        </View>
      </SafeAreaView>

      {/* 5. Detailed Diagnostics Slide-in Glassmorphism Drawer */}
      {showDiagnosticsPanel && (
        <Animated.View style={[styles.diagnosticsDrawer, animatedDrawerStyle]}>
          <View style={styles.drawerHeader}>
            <Text style={styles.drawerHeaderTitle}>AI Face Scan Results</Text>
            <TouchableOpacity
              style={styles.closeDrawerButton}
              onPress={() => setShowDiagnosticsPanel(false)}
            >
              <Text style={styles.closeDrawerText}>✖</Text>
            </TouchableOpacity>
          </View>

          <ScrollView contentContainerStyle={styles.drawerContentScroll}>
            {/* Shape Cards */}
            <View style={styles.shapesRow}>
              <ShapeCard label="Face Shape" value={faceShape} icon="👤" description="Analyzed from jaw width & face height ratios." />
              <ShapeCard label="Eye Shape" value={eyeShape} icon="👁️" description="Analyzed from aspect ratio & canthal tilt." />
              <ShapeCard label="Nose Shape" value={noseShape} icon="👃" description="Analyzed from width & nose bridge straightness." />
            </View>

            {/* Detailed Metric Ratios */}
            <Text style={styles.metricSectionTitle}>Euclidean Morphology Ratios</Text>
            
            <MetricBar label="Jaw Width Ratio" value={jawWidth} normVal={jawWidth * 2} min={0.1} max={0.6} />
            <MetricBar label="Face Aspect Ratio (Length/Width)" value={faceLength / (jawWidth > 0 ? jawWidth : 1)} normVal={(faceLength / (jawWidth > 0 ? jawWidth : 1)) / 2} min={0.5} max={1.5} />
            <MetricBar label="Canthal Tilt Angle" value={canthalTilt} normVal={(canthalTilt + 15) / 30} suffix="°" min={-10} max={15} />
            <MetricBar label="Eye Aspect Ratio (EAR)" value={eyeAspectRatio} normVal={eyeAspectRatio * 3} min={0.15} max={0.35} />
            <MetricBar label="Alar Base Nose Width" value={alarBaseWidth} normVal={alarBaseWidth * 3} min={0.1} max={0.4} />

            <View style={styles.diagnosticsNote}>
              <Text style={styles.noteText}>
                💡 <Text style={styles.boldText}>AI Recommendation:</Text> {faceShape === 'Round' ? 'Lifted blush and slimming jawline contours are applied to elongate the face.' : faceShape === 'Oblong' ? 'Horizontal blush application breaks up vertical space.' : 'Standard contours highlight symmetry.'} {noseShape === 'Wide' ? 'Pinch contours align closer to correct nose bridge width.' : ''} {eyeShape === 'Downturned' ? 'Eyeshadow and liner rotated upwards to lift outer corners.' : ''}
              </Text>
            </View>
            
            <TouchableOpacity style={styles.applyAIBtn} onPress={() => {
              applyAIBestLook();
              setShowDiagnosticsPanel(false);
            }}>
              <Text style={styles.applyAIBtnText}>Re-Apply AI Best Look Profile</Text>
            </TouchableOpacity>
          </ScrollView>
        </Animated.View>
      )}
    </View>
  );
}

// Helper component: Tab Button
interface TabButtonProps {
  title: string;
  active: boolean;
  onPress: () => void;
}
const TabButton: React.FC<TabButtonProps> = ({ title, active, onPress }) => (
  <TouchableOpacity style={[styles.tabButton, active && styles.activeTabButton]} onPress={onPress}>
    <Text style={[styles.tabText, active && styles.activeTabText]}>{title}</Text>
  </TouchableOpacity>
);

// Helper component: Options Selector Button
interface OptionButtonProps {
  title: string;
  selected: boolean;
  onPress: () => void;
}
const OptionButton: React.FC<OptionButtonProps> = ({ title, selected, onPress }) => (
  <TouchableOpacity style={[styles.optionButton, selected && styles.selectedOptionButton]} onPress={onPress}>
    <Text style={[styles.optionText, selected && styles.selectedOptionText]}>{title}</Text>
  </TouchableOpacity>
);

// Helper component: Color Picker Circle
interface ColorPickerProps {
  label: string;
  colors: string[];
  selectedColor: string;
  onSelect: (color: string) => void;
}
const ColorPicker: React.FC<ColorPickerProps> = ({ label, colors, selectedColor, onSelect }) => (
  <View style={styles.colorPickerContainer}>
    <Text style={styles.colorPickerLabel}>{label}</Text>
    <View style={styles.colorsRow}>
      {colors.map((color) => {
        const isTransparent = color === '#00000000';
        return (
          <TouchableOpacity
            key={color}
            style={[
              styles.colorCircle,
              { backgroundColor: isTransparent ? 'rgba(255,255,255,0.1)' : color },
              selectedColor.toLowerCase() === color.toLowerCase() && styles.selectedColorCircle,
              isTransparent && { borderWidth: 1, borderColor: 'rgba(255,255,255,0.3)', borderStyle: 'dashed', justifyContent: 'center', alignItems: 'center' }
            ]}
            onPress={() => onSelect(color)}
          >
            {isTransparent && <Text style={{ color: 'rgba(255,255,255,0.5)', fontSize: 14, fontWeight: 'bold' }}>X</Text>}
          </TouchableOpacity>
        );
      })}
    </View>
  </View>
);

// Helper component: Slider
interface SliderProps {
  label: string;
  min: number;
  max: number;
  value: number;
  onChange: (val: number) => void;
  suffix?: string;
}

const GlassSlider: React.FC<SliderProps> = ({ label, min, max, value, onChange, suffix = '' }) => {
  const widthRef = useRef(200);
  const leftRef = useRef(0);
  const trackRef = useRef<View>(null);

  const handleTouch = (pageX: number) => {
    const relativeX = pageX - leftRef.current;
    const percentage = Math.max(0, Math.min(1, relativeX / widthRef.current));
    const val = min + percentage * (max - min);
    onChange(Number(val.toFixed(2)));
  };

  const panResponder = useRef(
    PanResponder.create({
      onStartShouldSetPanResponder: () => true,
      onMoveShouldSetPanResponder: () => true,
      onPanResponderGrant: (evt) => {
        handleTouch(evt.nativeEvent.pageX);
      },
      onPanResponderMove: (evt) => {
        handleTouch(evt.nativeEvent.pageX);
      },
    })
  ).current;

  const percentage = (value - min) / (max - min);

  return (
    <View style={styles.sliderContainer}>
      <View style={styles.sliderLabelRow}>
        <Text style={styles.sliderLabel}>{label}</Text>
        <Text style={styles.sliderValue}>
          {value.toFixed(2)}
          {suffix}
        </Text>
      </View>
      <View
        ref={trackRef}
        style={styles.sliderTrackContainer}
        {...panResponder.panHandlers}
        onLayout={(evt) => {
          widthRef.current = evt.nativeEvent.layout.width;
          trackRef.current?.measure((_x, _y, _w, _h, px, _py) => {
            leftRef.current = px;
          });
        }}
      >
        <View style={styles.sliderTrack} />
        <View style={[styles.sliderTrackFill, { width: `${percentage * 100}%` }]} />
        <View style={[styles.sliderThumb, { left: `${percentage * 100}%` }]} />
      </View>
    </View>
  );
};

// Helper component: Shapes card in diagnostics
interface ShapeCardProps {
  label: string;
  value: string;
  icon: string;
  description: string;
}
const ShapeCard: React.FC<ShapeCardProps> = ({ label, value, icon, description }) => (
  <View style={styles.shapeCard}>
    <Text style={styles.shapeCardIcon}>{icon}</Text>
    <Text style={styles.shapeCardLabel}>{label}</Text>
    <Text style={styles.shapeCardValue}>{value}</Text>
    <Text style={styles.shapeCardDesc}>{description}</Text>
  </View>
);

// Helper component: Metric progress bar
interface MetricBarProps {
  label: string;
  value: number;
  normVal: number;
  suffix?: string;
  min: number;
  max: number;
}
const MetricBar: React.FC<MetricBarProps> = ({ label, value, normVal, suffix = '', min, max }) => {
  const percentage = Math.max(0, Math.min(100, normVal * 100));
  return (
    <View style={styles.metricContainer}>
      <View style={styles.metricLabelRow}>
        <Text style={styles.metricLabel}>{label}</Text>
        <Text style={styles.metricValue}>
          {value.toFixed(3)}
          {suffix}
        </Text>
      </View>
      <View style={styles.metricTrack}>
        <View style={[styles.metricFill, { width: `${percentage}%` }]} />
      </View>
      <View style={styles.metricRangeRow}>
        <Text style={styles.metricRangeText}>Min: {min.toFixed(2)}</Text>
        <Text style={styles.metricRangeText}>Max: {max.toFixed(2)}</Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000',
  },
  centerContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#0A0A0E',
    padding: 24,
  },
  errorText: {
    color: '#FFF',
    fontSize: 16,
    textAlign: 'center',
    marginBottom: 20,
    lineHeight: 24,
    opacity: 0.8,
  },
  accentButton: {
    backgroundColor: '#00FFCC',
    paddingVertical: 12,
    paddingHorizontal: 24,
    borderRadius: 25,
  },
  accentButtonText: {
    color: '#0A0A0E',
    fontWeight: 'bold',
    fontSize: 14,
  },
  overlayContainer: {
    flex: 1,
    justifyContent: 'space-between',
  },
  headerPanel: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    backgroundColor: 'rgba(12, 12, 18, 0.75)',
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255, 255, 255, 0.1)',
    paddingHorizontal: 16,
    paddingVertical: 12,
    marginTop: Platform.OS === 'android' ? 24 : 0,
  },
  logoText: {
    color: '#FFF',
    fontSize: 18,
    fontWeight: '900',
    letterSpacing: 2,
  },
  subtitleText: {
    color: '#00FFCC',
    fontSize: 11,
    fontWeight: '600',
    letterSpacing: 1,
  },
  headerRight: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  aiTag: {
    backgroundColor: 'rgba(0, 255, 204, 0.15)',
    borderWidth: 1,
    borderColor: '#00FFCC',
    borderRadius: 10,
    paddingHorizontal: 6,
    paddingVertical: 3,
    marginRight: 8,
  },
  aiTagText: {
    color: '#00FFCC',
    fontSize: 8,
    fontWeight: 'bold',
  },
  meshToggleButton: {
    backgroundColor: 'rgba(255, 255, 255, 0.15)',
    borderRadius: 16,
    paddingHorizontal: 12,
    paddingVertical: 6,
  },
  activeMeshButton: {
    backgroundColor: '#00FFCC',
  },
  meshToggleText: {
    color: '#FFF',
    fontSize: 11,
    fontWeight: 'bold',
  },
  aiScanButton: {
    alignSelf: 'center',
    backgroundColor: 'rgba(12, 12, 18, 0.85)',
    borderWidth: 1.5,
    borderColor: '#00FFCC',
    borderRadius: 30,
    paddingVertical: 14,
    paddingHorizontal: 28,
    shadowColor: '#00FFCC',
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.6,
    shadowRadius: 15,
    elevation: 8,
    position: 'absolute',
    bottom: 270,
  },
  aiScanInner: {
    alignItems: 'center',
  },
  aiScanTitle: {
    color: '#00FFCC',
    fontSize: 15,
    fontWeight: 'bold',
    letterSpacing: 1,
  },
  aiScanSubtitle: {
    color: '#FFF',
    fontSize: 9,
    opacity: 0.6,
    marginTop: 2,
  },
  scanningIndicator: {
    position: 'absolute',
    bottom: 270,
    alignSelf: 'center',
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: 'rgba(12, 12, 18, 0.9)',
    borderWidth: 1,
    borderColor: '#00FFCC',
    borderRadius: 20,
    paddingHorizontal: 20,
    paddingVertical: 10,
  },
  scanningText: {
    color: '#00FFCC',
    fontWeight: 'bold',
    fontSize: 12,
    marginLeft: 10,
    letterSpacing: 1,
  },
  scanLine: {
    position: 'absolute',
    left: 0,
    right: 0,
    height: 4,
    backgroundColor: '#00FFCC',
    shadowColor: '#00FFCC',
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.9,
    shadowRadius: 10,
    elevation: 10,
  },
  controlPanel: {
    backgroundColor: 'rgba(12, 12, 18, 0.85)',
    borderTopWidth: 1,
    borderTopColor: 'rgba(255, 255, 255, 0.12)',
    paddingBottom: 16,
    borderTopLeftRadius: 24,
    borderTopRightRadius: 24,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: -4 },
    shadowOpacity: 0.4,
    shadowRadius: 8,
  },
  tabsScroll: {
    paddingHorizontal: 12,
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255, 255, 255, 0.08)',
  },
  tabButton: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 20,
    marginRight: 8,
    backgroundColor: 'rgba(255, 255, 255, 0.05)',
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.05)',
  },
  activeTabButton: {
    backgroundColor: 'rgba(0, 255, 204, 0.1)',
    borderColor: '#00FFCC',
  },
  tabText: {
    color: 'rgba(255, 255, 255, 0.6)',
    fontSize: 12,
    fontWeight: '600',
  },
  activeTabText: {
    color: '#00FFCC',
  },
  drawersContent: {
    height: 160,
    paddingHorizontal: 16,
    paddingTop: 8,
  },
  sliderContainer: {
    marginBottom: 14,
  },
  sliderLabelRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 6,
  },
  sliderLabel: {
    color: '#FFF',
    fontSize: 12,
    fontWeight: '500',
  },
  sliderValue: {
    color: '#00FFCC',
    fontSize: 12,
    fontWeight: 'bold',
  },
  sliderTrackContainer: {
    height: 32,
    justifyContent: 'center',
    position: 'relative',
  },
  sliderTrack: {
    height: 6,
    backgroundColor: 'rgba(255, 255, 255, 0.1)',
    borderRadius: 3,
  },
  sliderTrackFill: {
    height: 6,
    backgroundColor: '#00FFCC',
    borderRadius: 3,
    position: 'absolute',
    left: 0,
  },
  sliderThumb: {
    width: 18,
    height: 18,
    borderRadius: 9,
    backgroundColor: '#FFF',
    position: 'absolute',
    top: 7,
    marginLeft: -9,
    shadowColor: '#00FFCC',
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.8,
    shadowRadius: 5,
    elevation: 3,
  },
  styleSelector: {
    marginBottom: 14,
  },
  selectorLabel: {
    color: '#FFF',
    fontSize: 12,
    fontWeight: '500',
    marginBottom: 8,
  },
  optionsRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
  },
  optionButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 14,
    marginRight: 8,
    marginBottom: 6,
    backgroundColor: 'rgba(255, 255, 255, 0.05)',
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.05)',
  },
  selectedOptionButton: {
    backgroundColor: 'rgba(0, 255, 204, 0.1)',
    borderColor: '#00FFCC',
  },
  optionText: {
    color: 'rgba(255, 255, 255, 0.6)',
    fontSize: 11,
  },
  selectedOptionText: {
    color: '#00FFCC',
    fontWeight: 'bold',
  },
  colorPickerContainer: {
    marginBottom: 8,
  },
  colorPickerLabel: {
    color: '#FFF',
    fontSize: 12,
    fontWeight: '500',
    marginBottom: 8,
  },
  colorsRow: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  colorCircle: {
    width: 28,
    height: 28,
    borderRadius: 14,
    marginRight: 10,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  selectedColorCircle: {
    borderColor: '#00FFCC',
    transform: [{ scale: 1.15 }],
  },
  diagnosticsTabSummary: {
    alignItems: 'center',
    justifyContent: 'center',
    flex: 1,
  },
  diagnosticsSummaryTitle: {
    color: '#FFF',
    fontSize: 14,
    fontWeight: 'bold',
  },
  diagnosticsSummaryDesc: {
    color: 'rgba(255, 255, 255, 0.6)',
    fontSize: 11,
    marginTop: 4,
  },
  summaryValue: {
    color: '#00FFCC',
    fontWeight: 'bold',
  },
  viewDetailedButton: {
    backgroundColor: 'rgba(0, 255, 204, 0.1)',
    borderWidth: 1,
    borderColor: '#00FFCC',
    borderRadius: 18,
    paddingVertical: 6,
    paddingHorizontal: 16,
    marginTop: 10,
  },
  viewDetailedText: {
    color: '#00FFCC',
    fontSize: 11,
    fontWeight: 'bold',
  },
  resetButton: {
    alignSelf: 'center',
    paddingVertical: 6,
    paddingHorizontal: 20,
    marginTop: 4,
  },
  resetButtonText: {
    color: 'rgba(255, 255, 255, 0.4)',
    fontSize: 11,
    fontWeight: '600',
  },
  diagnosticsDrawer: {
    position: 'absolute',
    bottom: 0,
    left: 0,
    right: 0,
    height: 520,
    backgroundColor: 'rgba(12, 12, 20, 0.95)',
    borderTopLeftRadius: 28,
    borderTopRightRadius: 28,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.15)',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: -10 },
    shadowOpacity: 0.5,
    shadowRadius: 15,
    elevation: 20,
  },
  drawerHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 20,
    paddingVertical: 16,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255, 255, 255, 0.08)',
  },
  drawerHeaderTitle: {
    color: '#FFF',
    fontSize: 16,
    fontWeight: 'bold',
  },
  closeDrawerButton: {
    padding: 4,
  },
  closeDrawerText: {
    color: 'rgba(255, 255, 255, 0.5)',
    fontSize: 16,
  },
  drawerContentScroll: {
    padding: 20,
    paddingBottom: 40,
  },
  shapesRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 20,
  },
  shapeCard: {
    width: '31%',
    backgroundColor: 'rgba(255, 255, 255, 0.04)',
    borderRadius: 14,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.08)',
    padding: 10,
    alignItems: 'center',
  },
  shapeCardIcon: {
    fontSize: 22,
    marginBottom: 4,
  },
  shapeCardLabel: {
    color: 'rgba(255, 255, 255, 0.4)',
    fontSize: 9,
    fontWeight: '600',
  },
  shapeCardValue: {
    color: '#00FFCC',
    fontSize: 14,
    fontWeight: 'bold',
    marginTop: 2,
  },
  shapeCardDesc: {
    color: 'rgba(255, 255, 255, 0.3)',
    fontSize: 7,
    textAlign: 'center',
    marginTop: 4,
  },
  metricSectionTitle: {
    color: '#FFF',
    fontSize: 13,
    fontWeight: 'bold',
    marginBottom: 12,
    letterSpacing: 0.5,
  },
  metricContainer: {
    marginBottom: 12,
    backgroundColor: 'rgba(255, 255, 255, 0.02)',
    borderRadius: 10,
    padding: 8,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.03)',
  },
  metricLabelRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 4,
  },
  metricLabel: {
    color: 'rgba(255, 255, 255, 0.8)',
    fontSize: 11,
    fontWeight: '500',
  },
  metricValue: {
    color: '#00FFCC',
    fontSize: 11,
    fontWeight: 'bold',
  },
  metricTrack: {
    height: 4,
    backgroundColor: 'rgba(255, 255, 255, 0.1)',
    borderRadius: 2,
    marginVertical: 4,
  },
  metricFill: {
    height: 4,
    backgroundColor: '#00FFCC',
    borderRadius: 2,
  },
  metricRangeRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  metricRangeText: {
    color: 'rgba(255, 255, 255, 0.3)',
    fontSize: 8,
  },
  diagnosticsNote: {
    backgroundColor: 'rgba(0, 255, 204, 0.05)',
    borderWidth: 1,
    borderColor: 'rgba(0, 255, 204, 0.15)',
    borderRadius: 12,
    padding: 12,
    marginTop: 8,
    marginBottom: 20,
  },
  noteText: {
    color: 'rgba(255, 255, 255, 0.8)',
    fontSize: 11,
    lineHeight: 16,
  },
  boldText: {
    fontWeight: 'bold',
  },
  applyAIBtn: {
    backgroundColor: '#00FFCC',
    borderRadius: 24,
    paddingVertical: 14,
    alignItems: 'center',
    shadowColor: '#00FFCC',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 10,
    elevation: 4,
  },
  applyAIBtnText: {
    color: '#0A0A0E',
    fontWeight: 'bold',
    fontSize: 14,
    letterSpacing: 0.5,
  },
});

export default App;
