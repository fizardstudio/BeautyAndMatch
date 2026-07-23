import re

inner_lips = [78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308, 324, 318, 402, 317, 14, 87, 178, 88, 95]

with open("cpp/FizgravityMakeupIndices.h", "r") as f:
    content = f.read()

new_array_str = "static const unsigned short INNER_LIPS_INDICES[] = {\n    " + ", ".join(str(x) for x in inner_lips) + "\n};\n#define NUM_INNER_LIPS_INDICES " + str(len(inner_lips)) + "\n"

# Append if not exists
if "INNER_LIPS_INDICES" not in content:
    content += "\n" + new_array_str

with open("cpp/FizgravityMakeupIndices.h", "w") as f:
    f.write(content)

print("Added INNER_LIPS_INDICES")
