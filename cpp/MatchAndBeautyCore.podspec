Pod::Spec.new do |s|
  s.name         = "MatchAndBeautyCore"
  s.version      = "0.0.1"
  s.summary      = "MatchAndBeauty C++ Core Library"
  s.description  = "C++ core image processing and math calculations for MatchAndBeauty."
  s.homepage     = "https://github.com/user/MatchAndBeauty"
  s.license      = "MIT"
  s.author       = { "Author" => "author@domain.com" }
  s.platform     = :ios, "13.0"
  s.source       = { :path => '.' }
  s.source_files = "*.{h,cpp}"
  s.public_header_files = "*.h"
  s.header_mappings_dir = "."
end
