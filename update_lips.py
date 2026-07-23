import json
import re

new_lips = [0, 11, 12, 13, 14, 15, 16, 17, 37, 38, 39, 40, 41, 42, 61, 62, 72, 73, 74, 76, 77, 78, 80, 81, 82, 84, 85, 86, 87, 88, 89, 90, 91, 95, 96, 146, 178, 179, 180, 181, 183, 184, 185, 191, 267, 268, 269, 270, 271, 272, 291, 292, 302, 303, 304, 306, 307, 308, 310, 311, 312, 314, 315, 316, 317, 318, 319, 320, 321, 324, 325, 375, 402, 403, 404, 405, 407, 408, 409, 415]

with open("cpp/FizgravityMakeupIndices.h", "r") as f:
    content = f.read()

# Replace the LIPS_INDICES or LIP_INDICES array
# Assuming the array is defined as `static const unsigned short LIPS_INDICES[] = { ... };`
# We'll use regex to find and replace it
new_array_str = ", ".join(str(x) for x in new_lips)

content = re.sub(r'(static const unsigned short LIPS?_INDICES\[\] = \{)[^\}]+(\})',
                 r'\1\n    ' + new_array_str + r'\n\2', content)
                 
content = re.sub(r'#define NUM_LIPS?_INDICES \d+', f'#define NUM_LIPS_INDICES {len(new_lips)}', content)

with open("cpp/FizgravityMakeupIndices.h", "w") as f:
    f.write(content)

print("Updated FizgravityMakeupIndices.h")
