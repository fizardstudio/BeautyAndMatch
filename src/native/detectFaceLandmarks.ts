import { VisionCameraProxy, Frame } from 'react-native-vision-camera';
import { FaceShape, EyeShape, NoseShape } from '../store/makeupStore';

// Initialize the native detectFaceLandmarks Frame Processor Plugin
const plugin = VisionCameraProxy.initFrameProcessorPlugin('detectFaceLandmarks', {});

export interface Landmark {
  x: number;
  y: number;
  z: number;
}

export interface Diagnostics {
  faceShape: FaceShape;
  eyeShape: EyeShape;
  noseShape: NoseShape;
  jawWidth: number;
  faceLength: number;
  canthalTilt: number;
  eyeAspectRatio: number;
  alarBaseWidth: number;
  intercanthalDistance: number;
}

export interface FaceLandmarkerResult {
  landmarks: Landmark[];
  blendshapes: { categoryName: string; score: number }[];
  faceShape?: FaceShape;
  eyeShape?: EyeShape;
  noseShape?: NoseShape;
  jawWidth?: number;
  faceLength?: number;
  canthalTilt?: number;
  eyeAspectRatio?: number;
  alarBaseWidth?: number;
  intercanthalDistance?: number;
}

/**
 * Detects 478 facial landmarks and 52 blendshapes synchronously in a Camera Frame.
 * This runs on the worklet thread with zero latency.
 */
export function detectFaceLandmarks(frame: Frame): FaceLandmarkerResult | null {
  'worklet';
  if (plugin == null) {
    return null;
  }
  return plugin.call(frame) as unknown as FaceLandmarkerResult;
}
