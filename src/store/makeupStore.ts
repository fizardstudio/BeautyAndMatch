import { create } from 'zustand';

export type AIMode = 'manual' | 'ai_auto';
export type FaceShape = 'Round' | 'Square' | 'Oblong' | 'Normal' | 'Unknown';
export type EyeShape = 'Downturned' | 'Monolid' | 'Hooded' | 'Normal' | 'Unknown';
export type NoseShape = 'Wide' | 'Crooked' | 'Normal' | 'Unknown';

export interface MakeupState {
  foundationColor: string;
  foundationOpacity: number;
  foundationBlur: number;

  blushColor: string;
  blushOpacity: number;
  blushStyle: 'contour_45' | 'horizontal' | 'normal';

  contourColor: string;
  contourIntensity: number;
  contourStyle: 'slim' | 'normal' | 'pinch' | 'straight';

  lipstickColor: string;
  lipstickGlossiness: number;
  lipstickOpacity: number;

  eyeshadowColor: string;
  eyeshadowOpacity: number;
  eyeshadowStyle: 'lifting' | 'gradient' | 'halo' | 'normal';

  faceShape: FaceShape;
  eyeShape: EyeShape;
  noseShape: NoseShape;
  jawWidth: number;
  faceLength: number;
  canthalTilt: number;
  eyeAspectRatio: number;
  alarBaseWidth: number;
  intercanthalDistance: number;

  aiMode: AIMode;
  cameraActive: boolean;

  // Setters
  setFoundation: (settings: Partial<Pick<MakeupState, 'foundationColor' | 'foundationOpacity' | 'foundationBlur'>>) => void;
  setBlush: (settings: Partial<Pick<MakeupState, 'blushColor' | 'blushOpacity' | 'blushStyle'>>) => void;
  setContour: (settings: Partial<Pick<MakeupState, 'contourColor' | 'contourIntensity' | 'contourStyle'>>) => void;
  setLipstick: (settings: Partial<Pick<MakeupState, 'lipstickColor' | 'lipstickGlossiness' | 'lipstickOpacity'>>) => void;
  setEyeshadow: (settings: Partial<Pick<MakeupState, 'eyeshadowColor' | 'eyeshadowOpacity' | 'eyeshadowStyle'>>) => void;
  setDiagnostics: (results: Partial<Pick<MakeupState, 'faceShape' | 'eyeShape' | 'noseShape' | 'jawWidth' | 'faceLength' | 'canthalTilt' | 'eyeAspectRatio' | 'alarBaseWidth' | 'intercanthalDistance'>>) => void;
  setAIMode: (mode: AIMode) => void;
  setCameraActive: (active: boolean) => void;
  resetMakeup: () => void;
  applyAIBestLook: () => void;
}

const initialMakeup = {
  foundationColor: '#F6C3A2',
  foundationOpacity: 0.5,
  foundationBlur: 8.0,

  blushColor: '#E2725B',
  blushOpacity: 0.4,
  blushStyle: 'normal' as const,

  contourColor: '#6B4D3C',
  contourIntensity: 0.4,
  contourStyle: 'normal' as const,

  lipstickColor: '#D35400',
  lipstickGlossiness: 0.5,
  lipstickOpacity: 0.6,

  eyeshadowColor: '#5D4037',
  eyeshadowOpacity: 0.4,
  eyeshadowStyle: 'normal' as const,
};

export const useMakeupStore = create<MakeupState>((set, get) => ({
  ...initialMakeup,

  faceShape: 'Unknown',
  eyeShape: 'Unknown',
  noseShape: 'Unknown',
  jawWidth: 0,
  faceLength: 0,
  canthalTilt: 0,
  eyeAspectRatio: 0,
  alarBaseWidth: 0,
  intercanthalDistance: 0,

  aiMode: 'manual',
  cameraActive: true,

  setFoundation: (settings) => set((state) => ({ ...state, ...settings })),
  setBlush: (settings) => set((state) => ({ ...state, ...settings })),
  setContour: (settings) => set((state) => ({ ...state, ...settings })),
  setLipstick: (settings) => set((state) => ({ ...state, ...settings })),
  setEyeshadow: (settings) => set((state) => ({ ...state, ...settings })),
  setDiagnostics: (results) => set((state) => ({ ...state, ...results })),
  setAIMode: (mode) => set({ aiMode: mode }),
  setCameraActive: (active) => set({ cameraActive: active }),
  resetMakeup: () => set((state) => ({ ...state, ...initialMakeup })),

  applyAIBestLook: () => {
    const { faceShape, eyeShape, noseShape } = get();

    // 1-Click AI Best Look Optimization Rules
    let finalBlushStyle: MakeupState['blushStyle'] = 'normal';
    let finalBlushColor = '#E2725B';
    let finalBlushOpacity = 0.5;

    let finalContourStyle: MakeupState['contourStyle'] = 'normal';
    let finalContourIntensity = 0.5;
    let finalContourColor = '#6B4D3C';

    // Face Shape Optimization
    if (faceShape === 'Round') {
      finalBlushStyle = 'contour_45'; // Lifting effect
      finalBlushColor = '#D87093'; // PaleVioletRed
      finalBlushOpacity = 0.65;
      finalContourStyle = 'slim'; // Slimming jawline/cheeks
      finalContourIntensity = 0.75;
    } else if (faceShape === 'Oblong') {
      finalBlushStyle = 'horizontal'; // Shortens face visually
      finalBlushColor = '#F4C2C2'; // Baby pink
      finalBlushOpacity = 0.5;
      finalContourStyle = 'normal';
      finalContourIntensity = 0.4;
    } else if (faceShape === 'Square') {
      finalBlushStyle = 'normal'; // Centered on apples of cheeks
      finalBlushColor = '#FF8C00'; // Coral
      finalBlushOpacity = 0.6;
      finalContourStyle = 'slim'; // Soften strong angles
      finalContourIntensity = 0.65;
    }

    // Nose Shape Optimization
    if (noseShape === 'Wide') {
      finalContourStyle = 'pinch'; // Draw nose contour closer to center bridge
      finalContourIntensity = Math.max(finalContourIntensity, 0.7);
    } else if (noseShape === 'Crooked') {
      finalContourStyle = 'straight'; // Draw straight vertical contours to correct deviation
      finalContourIntensity = Math.max(finalContourIntensity, 0.75);
    }

    // Eye Shape Optimization
    let finalEyeshadowStyle: MakeupState['eyeshadowStyle'] = 'normal';
    let finalEyeshadowColor = '#5D4037';
    let finalEyeshadowOpacity = 0.5;

    if (eyeShape === 'Downturned') {
      finalEyeshadowStyle = 'lifting'; // Dramatic winged/lifting blend
      finalEyeshadowColor = '#4A3B32'; // Deep espresso
      finalEyeshadowOpacity = 0.75;
    } else if (eyeShape === 'Monolid') {
      finalEyeshadowStyle = 'gradient'; // Vertical blend to create depth
      finalEyeshadowColor = '#8A3324'; // Copper/terracotta
      finalEyeshadowOpacity = 0.7;
    } else if (eyeShape === 'Hooded') {
      finalEyeshadowStyle = 'halo'; // Halo look to give dimension
      finalEyeshadowColor = '#B08D57'; // Gold/shimmer taupe
      finalEyeshadowOpacity = 0.65;
    }

    // Harmonize Lips
    const finalLipstickColor = '#C0392B'; // Rich crimson/rose
    const finalLipstickGlossiness = 0.75; // Premium gloss specular
    const finalLipstickOpacity = 0.8;

    // Foundation
    const finalFoundationBlur = 12.0; // High smoothing
    const finalFoundationOpacity = 0.7;

    set({
      blushStyle: finalBlushStyle,
      blushColor: finalBlushColor,
      blushOpacity: finalBlushOpacity,

      contourStyle: finalContourStyle,
      contourIntensity: finalContourIntensity,
      contourColor: finalContourColor,

      eyeshadowStyle: finalEyeshadowStyle,
      eyeshadowColor: finalEyeshadowColor,
      eyeshadowOpacity: finalEyeshadowOpacity,

      lipstickColor: finalLipstickColor,
      lipstickGlossiness: finalLipstickGlossiness,
      lipstickOpacity: finalLipstickOpacity,

      foundationBlur: finalFoundationBlur,
      foundationOpacity: finalFoundationOpacity,
      aiMode: 'ai_auto',
    });
  },
}));
