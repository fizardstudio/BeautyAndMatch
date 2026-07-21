#import <Foundation/Foundation.h>
#import <VisionCamera/FrameProcessorPlugin.h>
#import <VisionCamera/FrameProcessorPluginRegistry.h>
#import <VisionCamera/Frame.h>
#import <MediaPipeTasksVision/MediaPipeTasksVision.h>
#include "MatchAndBeautyCore.h"

@interface MediaPipeFrameProcessorPlugin : FrameProcessorPlugin {
  MPVFaceLandmarker* _faceLandmarker;
  std::shared_ptr<match_and_beauty::MatchAndBeautyCore> _core;
}
@end

@implementation MediaPipeFrameProcessorPlugin

- (instancetype)initWithOptions:(NSDictionary*)options {
  self = [super initWithOptions:options];
  if (self) {
    _core = std::make_shared<match_and_beauty::MatchAndBeautyCore>();
    
    // Load MediaPipe Face Landmarker model from bundle
    NSString* modelPath = [[NSBundle mainBundle] pathForResource:@"face_landmarker" ofType:@"task"];
    if (modelPath) {
      MPVFaceLandmarkerOptions* options = [[MPVFaceLandmarkerOptions alloc] init];
      options.baseOptions.modelAssetPath = modelPath;
      options.runningMode = MPVRunningModeImage;
      options.outputFaceBlendshapes = YES;
      options.minFaceDetectionConfidence = 0.5;
      options.minFacePresenceConfidence = 0.5;
      options.minTrackingConfidence = 0.5;
      
      NSError* error = nil;
      _faceLandmarker = [[MPVFaceLandmarker alloc] initWithOptions:options error:&error];
      if (error) {
        NSLog(@"[MediaPipePlugin] Failed to initialize: %@", error.localizedDescription);
      } else {
        NSLog(@"[MediaPipePlugin] Initialized successfully!");
      }
    } else {
      NSLog(@"[MediaPipePlugin] Model file face_landmarker.task not found in bundle!");
    }
  }
  return self;
}

- (id)callback:(Frame*)frame withArguments:(NSDictionary*)arguments {
  if (!_faceLandmarker) return nil;
  
  CVPixelBufferRef pixelBuffer = frame.pixelBuffer;
  if (!pixelBuffer) return nil;
  
  // Convert CVPixelBufferRef to MPImage
  MPImage* mpImage = [[MPImage alloc] initWithPixelBuffer:pixelBuffer];
  
  NSError* error = nil;
  MPVFaceLandmarkerResult* result = [_faceLandmarker detectImage:mpImage error:&error];
  if (error) {
    NSLog(@"[MediaPipePlugin] Detection error: %@", error.localizedDescription);
    return nil;
  }
  
  NSMutableDictionary* resultMap = [NSMutableDictionary dictionary];
  NSMutableArray* landmarksArray = [NSMutableArray array];
  
  if (result.faceLandmarks.count > 0) {
    NSArray<MPVNormalizedLandmark*>* faceLandmarks = result.faceLandmarks[0];
    std::vector<match_and_beauty::Landmark> cppLandmarks;
    
    for (MPVNormalizedLandmark* landmark in faceLandmarks) {
      cppLandmarks.push_back({landmark.x, landmark.y, landmark.z});
      [landmarksArray addObject:@{
        @"x": @(landmark.x),
        @"y": @(landmark.y),
        @"z": @(landmark.z)
      }];
    }
    
    // Call C++ Diagnostics math logic
    match_and_beauty::DiagnosticsResult diagResult = _core->analyzeMorphology(cppLandmarks);
    
    resultMap[@"diagnostics"] = @{
      @"faceShape": [NSString stringWithUTF8String:diagResult.faceShape.c_str()],
      @"eyeShape": [NSString stringWithUTF8String:diagResult.eyeShape.c_str()],
      @"noseShape": [NSString stringWithUTF8String:diagResult.noseShape.c_str()],
      @"jawWidth": @(diagResult.jawWidth),
      @"faceLength": @(diagResult.faceLength),
      @"canthalTilt": @(diagResult.canthalTilt),
      @"eyeAspectRatio": @(diagResult.eyeAspectRatio),
      @"alarBaseWidth": @(diagResult.alarBaseWidth),
      @"intercanthalDistance": @(diagResult.intercanthalDistance)
    };
  }
  resultMap[@"landmarks"] = landmarksArray;
  
  NSMutableArray* blendshapesArray = [NSMutableArray array];
  if (result.faceBlendshapes.count > 0) {
    NSArray<MPVCategory*>* blendshapes = result.faceBlendshapes[0];
    for (MPVCategory* category in blendshapes) {
      [blendshapesArray addObject:@{
        @"categoryName": category.categoryName,
        @"score": @(category.score)
      }];
    }
  }
  resultMap[@"blendshapes"] = blendshapesArray;
  
  return resultMap;
}

+ (void)load {
  [FrameProcessorPluginRegistry registerFrameProcessorPlugin:@"detectFaceLandmarks"
                                             withInitializer:^FrameProcessorPlugin*(NSDictionary* options) {
    return [[MediaPipeFrameProcessorPlugin alloc] initWithOptions:options];
  }];
}

@end
